// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/memory/aligned_memory.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "third_party/libjingle/source/talk/media/base/videoframe.h"
#include "third_party/libjingle/source/talk/media/base/videoframefactory.h"
#include "third_party/libjingle/source/talk/media/webrtc/webrtcvideoframe.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace content {
namespace {

// Empty method used for keeping a reference to the original media::VideoFrame.
// The reference to |frame| is kept in the closure that calls this method.
void ReleaseOriginalFrame(const scoped_refptr<media::VideoFrame>& frame) {
}

// Thin map between an existing media::VideoFrame and cricket::VideoFrame to
// avoid premature deep copies.
// This implementation is only safe to use in a const context and should never
// be written to.
class VideoFrameWrapper : public cricket::VideoFrame {
 public:
  VideoFrameWrapper(const scoped_refptr<media::VideoFrame>& frame,
                    int64 elapsed_time)
      : frame_(media::VideoFrame::WrapVideoFrame(
            frame,
            frame->visible_rect(),
            frame->natural_size(),
            base::Bind(&ReleaseOriginalFrame, frame))),
        elapsed_time_(elapsed_time) {}

  VideoFrame* Copy() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return new VideoFrameWrapper(frame_, elapsed_time_);
  }

  size_t GetWidth() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return static_cast<size_t>(frame_->visible_rect().width());
  }

  size_t GetHeight() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return static_cast<size_t>(frame_->visible_rect().height());
  }

  const uint8* GetYPlane() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  const uint8* GetUPlane() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->visible_data(media::VideoFrame::kUPlane);
  }

  const uint8* GetVPlane() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->visible_data(media::VideoFrame::kVPlane);
  }

  uint8* GetYPlane() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  uint8* GetUPlane() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->visible_data(media::VideoFrame::kUPlane);
  }

  uint8* GetVPlane() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->visible_data(media::VideoFrame::kVPlane);
  }

  int32 GetYPitch() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->stride(media::VideoFrame::kYPlane);
  }

  int32 GetUPitch() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->stride(media::VideoFrame::kUPlane);
  }

  int32 GetVPitch() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->stride(media::VideoFrame::kVPlane);
  }

  void* GetNativeHandle() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return NULL;
  }

  size_t GetPixelWidth() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return 1;
  }
  size_t GetPixelHeight() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return 1;
  }

  int64 GetElapsedTime() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return elapsed_time_;
  }

  int64 GetTimeStamp() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return frame_->timestamp().InMicroseconds() *
           base::Time::kNanosecondsPerMicrosecond;
  }

  void SetElapsedTime(int64 elapsed_time) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    elapsed_time_ = elapsed_time;
  }

  void SetTimeStamp(int64 time_stamp) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    // Round to closest microsecond.
    frame_->set_timestamp(base::TimeDelta::FromMicroseconds(
        (time_stamp + base::Time::kNanosecondsPerMicrosecond / 2) /
        base::Time::kNanosecondsPerMicrosecond));
  }

  int GetRotation() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return 0;
  }

  // TODO(magjed): Refactor into base class.
  size_t ConvertToRgbBuffer(uint32 to_fourcc,
                            uint8* buffer,
                            size_t size,
                            int stride_rgb) const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    const size_t needed = std::abs(stride_rgb) * GetHeight();
    if (size < needed) {
      DLOG(WARNING) << "RGB buffer is not large enough";
      return needed;
    }

    if (libyuv::ConvertFromI420(GetYPlane(),
                                GetYPitch(),
                                GetUPlane(),
                                GetUPitch(),
                                GetVPlane(),
                                GetVPitch(),
                                buffer,
                                stride_rgb,
                                static_cast<int>(GetWidth()),
                                static_cast<int>(GetHeight()),
                                to_fourcc)) {
      DLOG(ERROR) << "RGB type not supported: " << to_fourcc;
      return 0;  // 0 indicates error
    }
    return needed;
  }

  // The rest of the public methods are NOTIMPLEMENTED.
  bool InitToBlack(int w,
                   int h,
                   size_t pixel_width,
                   size_t pixel_height,
                   int64 elapsed_time,
                   int64 time_stamp) override {
    NOTIMPLEMENTED();
    return false;
  }

  bool Reset(uint32 fourcc,
             int w,
             int h,
             int dw,
             int dh,
             uint8* sample,
             size_t sample_size,
             size_t pixel_width,
             size_t pixel_height,
             int64 elapsed_time,
             int64 time_stamp,
             int rotation) override {
    NOTIMPLEMENTED();
    return false;
  }

  bool MakeExclusive() override {
    NOTIMPLEMENTED();
    return false;
  }

  size_t CopyToBuffer(uint8* buffer, size_t size) const override {
    NOTIMPLEMENTED();
    return 0;
  }

 protected:
  // TODO(magjed): Refactor as a static method in WebRtcVideoFrame.
  VideoFrame* CreateEmptyFrame(int w,
                               int h,
                               size_t pixel_width,
                               size_t pixel_height,
                               int64 elapsed_time,
                               int64 time_stamp) const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    VideoFrame* frame = new cricket::WebRtcVideoFrame();
    frame->InitToBlack(
        w, h, pixel_width, pixel_height, elapsed_time, time_stamp);
    return frame;
  }

 private:
  scoped_refptr<media::VideoFrame> frame_;
  int64 elapsed_time_;
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
      const cricket::CapturedFrame* captured_frame,
      int dst_width,
      int dst_height) const override {
    // Check that captured_frame is actually our frame.
    DCHECK(captured_frame == &captured_frame_);
    DCHECK(frame_.get());

    scoped_refptr<media::VideoFrame> video_frame = frame_;
    // Check if scaling is needed.
    if (dst_width != frame_->visible_rect().width() ||
        dst_height != frame_->visible_rect().height()) {
      video_frame =
          scaled_frame_pool_.CreateFrame(media::VideoFrame::I420,
                                         gfx::Size(dst_width, dst_height),
                                         gfx::Rect(0, 0, dst_width, dst_height),
                                         gfx::Size(dst_width, dst_height),
                                         frame_->timestamp());
      libyuv::I420Scale(frame_->visible_data(media::VideoFrame::kYPlane),
                        frame_->stride(media::VideoFrame::kYPlane),
                        frame_->visible_data(media::VideoFrame::kUPlane),
                        frame_->stride(media::VideoFrame::kUPlane),
                        frame_->visible_data(media::VideoFrame::kVPlane),
                        frame_->stride(media::VideoFrame::kVPlane),
                        frame_->visible_rect().width(),
                        frame_->visible_rect().height(),
                        video_frame->data(media::VideoFrame::kYPlane),
                        video_frame->stride(media::VideoFrame::kYPlane),
                        video_frame->data(media::VideoFrame::kUPlane),
                        video_frame->stride(media::VideoFrame::kUPlane),
                        video_frame->data(media::VideoFrame::kVPlane),
                        video_frame->stride(media::VideoFrame::kVPlane),
                        dst_width,
                        dst_height,
                        libyuv::kFilterBilinear);
    }

    // Create a shallow cricket::VideoFrame wrapper around the
    // media::VideoFrame. The caller has ownership of the returned frame.
    return new VideoFrameWrapper(video_frame, captured_frame_.elapsed_time);
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
