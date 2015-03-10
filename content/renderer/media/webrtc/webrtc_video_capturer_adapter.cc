// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"

#include "base/bind.h"
#include "base/memory/aligned_memory.h"
#include "base/trace_event/trace_event.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "third_party/libjingle/source/talk/media/base/videoframefactory.h"
#include "third_party/libjingle/source/talk/media/webrtc/webrtcvideoframe.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/webrtc/common_video/interface/video_frame_buffer.h"

namespace content {
namespace {

// Empty method used for keeping a reference to the original media::VideoFrame.
// The reference to |frame| is kept in the closure that calls this method.
void ReleaseOriginalFrame(const scoped_refptr<media::VideoFrame>& frame) {
}

int WebRtcToMediaPlaneType(webrtc::PlaneType type) {
  switch (type) {
    case webrtc::kYPlane:
      return media::VideoFrame::kYPlane;
    case webrtc::kUPlane:
      return media::VideoFrame::kUPlane;
    case webrtc::kVPlane:
      return media::VideoFrame::kVPlane;
    default:
      NOTREACHED();
      return media::VideoFrame::kMaxPlanes;
  }
}

// Thin adapter from media::VideoFrame to webrtc::VideoFrameBuffer.
class VideoFrameWrapper : public webrtc::VideoFrameBuffer {
 public:
  VideoFrameWrapper(const scoped_refptr<media::VideoFrame>& frame)
      : frame_(frame) {}

  int width() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->visible_rect().width();
  }

  int height() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->visible_rect().height();
  }

  const uint8_t* data(webrtc::PlaneType type) const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->visible_data(WebRtcToMediaPlaneType(type));
  }

  uint8_t* data(webrtc::PlaneType type) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(frame_->HasOneRef());
    return frame_->visible_data(WebRtcToMediaPlaneType(type));
  }

  int stride(webrtc::PlaneType type) const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->stride(WebRtcToMediaPlaneType(type));
  }

  rtc::scoped_refptr<webrtc::NativeHandle> native_handle() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return nullptr;
  }

 private:
  ~VideoFrameWrapper() override {}
  friend class rtc::RefCountedObject<VideoFrameWrapper>;

  scoped_refptr<media::VideoFrame> frame_;
  base::ThreadChecker thread_checker_;
};

}  // anonymous namespace

// A cricket::VideoFrameFactory for media::VideoFrame. The purpose of this
// class is to avoid a premature frame copy. A media::VideoFrame is injected
// with SetFrame, and converted into a cricket::VideoFrame with
// CreateAliasedFrame. SetFrame should be called before CreateAliasedFrame
// for every frame.
class WebRtcVideoCapturerAdapter::MediaVideoFrameFactory
    : public cricket::VideoFrameFactory {
 public:
  void SetFrame(const scoped_refptr<media::VideoFrame>& frame,
                int64_t elapsed_time) {
    DCHECK(frame.get());
    // Create a CapturedFrame that only contains header information, not the
    // actual pixel data.
    captured_frame_.width = frame->natural_size().width();
    captured_frame_.height = frame->natural_size().height();
    captured_frame_.elapsed_time = elapsed_time;
    captured_frame_.time_stamp = frame->timestamp().InMicroseconds() *
                                 base::Time::kNanosecondsPerMicrosecond;
    captured_frame_.pixel_height = 1;
    captured_frame_.pixel_width = 1;
    captured_frame_.rotation = 0;
    captured_frame_.data = NULL;
    captured_frame_.data_size = cricket::CapturedFrame::kUnknownDataSize;
    captured_frame_.fourcc = static_cast<uint32>(cricket::FOURCC_ANY);

    frame_ = frame;
  }

  void ReleaseFrame() { frame_ = NULL; }

  const cricket::CapturedFrame* GetCapturedFrame() const {
    return &captured_frame_;
  }

  cricket::VideoFrame* CreateAliasedFrame(
      const cricket::CapturedFrame* input_frame,
      int cropped_input_width,
      int cropped_input_height,
      int output_width,
      int output_height) const override {
    // Check that captured_frame is actually our frame.
    DCHECK(input_frame == &captured_frame_);
    DCHECK(frame_.get());

    // Create a centered cropped visible rect that preservers aspect ratio for
    // cropped natural size.
    gfx::Rect visible_rect = frame_->visible_rect();
    visible_rect.ClampToCenteredSize(gfx::Size(
        visible_rect.width() * cropped_input_width / input_frame->width,
        visible_rect.height() * cropped_input_height / input_frame->height));

    const gfx::Size output_size(output_width, output_height);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::WrapVideoFrame(
            frame_, visible_rect, output_size,
            base::Bind(&ReleaseOriginalFrame, frame_));

    const int64_t timestamp_ns = frame_->timestamp().InMicroseconds() *
                                 base::Time::kNanosecondsPerMicrosecond;

    // If no scaling is needed, return a wrapped version of |frame_| directly.
    if (video_frame->natural_size() == video_frame->visible_rect().size()) {
      return new cricket::WebRtcVideoFrame(
          new rtc::RefCountedObject<VideoFrameWrapper>(video_frame),
          captured_frame_.elapsed_time, timestamp_ns);
    }

    // We need to scale the frame before we hand it over to cricket.
    scoped_refptr<media::VideoFrame> scaled_frame =
        scaled_frame_pool_.CreateFrame(media::VideoFrame::I420, output_size,
                                       gfx::Rect(output_size), output_size,
                                       frame_->timestamp());
    libyuv::I420Scale(video_frame->visible_data(media::VideoFrame::kYPlane),
                      video_frame->stride(media::VideoFrame::kYPlane),
                      video_frame->visible_data(media::VideoFrame::kUPlane),
                      video_frame->stride(media::VideoFrame::kUPlane),
                      video_frame->visible_data(media::VideoFrame::kVPlane),
                      video_frame->stride(media::VideoFrame::kVPlane),
                      video_frame->visible_rect().width(),
                      video_frame->visible_rect().height(),
                      scaled_frame->data(media::VideoFrame::kYPlane),
                      scaled_frame->stride(media::VideoFrame::kYPlane),
                      scaled_frame->data(media::VideoFrame::kUPlane),
                      scaled_frame->stride(media::VideoFrame::kUPlane),
                      scaled_frame->data(media::VideoFrame::kVPlane),
                      scaled_frame->stride(media::VideoFrame::kVPlane),
                      output_width, output_height, libyuv::kFilterBilinear);
    return new cricket::WebRtcVideoFrame(
        new rtc::RefCountedObject<VideoFrameWrapper>(scaled_frame),
        captured_frame_.elapsed_time, timestamp_ns);
  }

  cricket::VideoFrame* CreateAliasedFrame(
      const cricket::CapturedFrame* input_frame,
      int output_width,
      int output_height) const override {
    return CreateAliasedFrame(input_frame, input_frame->width,
                              input_frame->height, output_width, output_height);
  }

 private:
  scoped_refptr<media::VideoFrame> frame_;
  cricket::CapturedFrame captured_frame_;
  // This is used only if scaling is needed.
  mutable media::VideoFramePool scaled_frame_pool_;
};

WebRtcVideoCapturerAdapter::WebRtcVideoCapturerAdapter(bool is_screencast)
    : is_screencast_(is_screencast),
      running_(false),
      first_frame_timestamp_(media::kNoTimestamp()),
      frame_factory_(new MediaVideoFrameFactory) {
  thread_checker_.DetachFromThread();
  // The base class takes ownership of the frame factory.
  set_frame_factory(frame_factory_);
}

WebRtcVideoCapturerAdapter::~WebRtcVideoCapturerAdapter() {
  DVLOG(3) << " WebRtcVideoCapturerAdapter::dtor";
}

cricket::CaptureState WebRtcVideoCapturerAdapter::Start(
    const cricket::VideoFormat& capture_format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!running_);
  DVLOG(3) << " WebRtcVideoCapturerAdapter::Start w = " << capture_format.width
           << " h = " << capture_format.height;

  running_ = true;
  return cricket::CS_RUNNING;
}

void WebRtcVideoCapturerAdapter::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << " WebRtcVideoCapturerAdapter::Stop ";
  DCHECK(running_);
  running_ = false;
  SetCaptureFormat(NULL);
  SignalStateChange(this, cricket::CS_STOPPED);
}

bool WebRtcVideoCapturerAdapter::IsRunning() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return running_;
}

bool WebRtcVideoCapturerAdapter::GetPreferredFourccs(
    std::vector<uint32>* fourccs) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!fourccs || fourccs->empty());
  if (fourccs)
    fourccs->push_back(cricket::FOURCC_I420);
  return fourccs != NULL;
}

bool WebRtcVideoCapturerAdapter::IsScreencast() const {
  return is_screencast_;
}

bool WebRtcVideoCapturerAdapter::GetBestCaptureFormat(
    const cricket::VideoFormat& desired,
    cricket::VideoFormat* best_format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << " GetBestCaptureFormat:: "
           << " w = " << desired.width
           << " h = " << desired.height;

  // Capability enumeration is done in MediaStreamVideoSource. The adapter can
  // just use what is provided.
  // Use the desired format as the best format.
  best_format->width = desired.width;
  best_format->height = desired.height;
  best_format->fourcc = cricket::FOURCC_I420;
  best_format->interval = desired.interval;
  return true;
}

void WebRtcVideoCapturerAdapter::OnFrameCaptured(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("video", "WebRtcVideoCapturerAdapter::OnFrameCaptured");
  if (!(media::VideoFrame::I420 == frame->format() ||
        media::VideoFrame::YV12 == frame->format())) {
    // Some types of sources support textures as output. Since connecting
    // sources and sinks do not check the format, we need to just ignore
    // formats that we can not handle.
    NOTREACHED();
    return;
  }

  if (first_frame_timestamp_ == media::kNoTimestamp())
    first_frame_timestamp_ = frame->timestamp();

  const int64 elapsed_time =
      (frame->timestamp() - first_frame_timestamp_).InMicroseconds() *
      base::Time::kNanosecondsPerMicrosecond;

  // Inject the frame via the VideoFrameFractory.
  DCHECK(frame_factory_ == frame_factory());
  frame_factory_->SetFrame(frame, elapsed_time);

  // This signals to libJingle that a new VideoFrame is available.
  SignalFrameCaptured(this, frame_factory_->GetCapturedFrame());

  frame_factory_->ReleaseFrame();  // Release the frame ASAP.
}

}  // namespace content
