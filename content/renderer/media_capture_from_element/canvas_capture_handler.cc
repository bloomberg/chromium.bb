// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media_capture_from_element/canvas_capture_handler.h"

#include <utility>

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/limits.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkImage.h"

namespace {

using media::VideoFrame;

const size_t kArgbBytesPerPixel = 4;

}  // namespace

namespace content {

// Implementation VideoCapturerSource that is owned by
// MediaStreamVideoCapturerSource and delegates the Start/Stop calls to
// CanvasCaptureHandler.
// This class is single threaded and pinned to main render thread.
class VideoCapturerSource : public media::VideoCapturerSource {
 public:
  VideoCapturerSource(base::WeakPtr<CanvasCaptureHandler> canvas_handler,
                      double frame_rate)
      : frame_rate_(static_cast<float>(
            std::min(static_cast<double>(media::limits::kMaxFramesPerSecond),
                     frame_rate))),
        canvas_handler_(canvas_handler) {
    DCHECK_LE(0, frame_rate_);
  }

 protected:
  media::VideoCaptureFormats GetPreferredFormats() override {
    DCHECK(main_render_thread_checker_.CalledOnValidThread());
    const blink::WebSize& size = canvas_handler_->GetSourceSize();
    media::VideoCaptureFormats formats;
    formats.push_back(
        media::VideoCaptureFormat(gfx::Size(size.width, size.height),
                                  frame_rate_, media::PIXEL_FORMAT_I420));
    formats.push_back(
        media::VideoCaptureFormat(gfx::Size(size.width, size.height),
                                  frame_rate_, media::PIXEL_FORMAT_YV12A));
    return formats;
  }
  void StartCapture(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& frame_callback,
                    const RunningCallback& running_callback) override {
    DCHECK(main_render_thread_checker_.CalledOnValidThread());
    canvas_handler_->StartVideoCapture(params, frame_callback,
                                       running_callback);
  }
  void RequestRefreshFrame() override {
    DCHECK(main_render_thread_checker_.CalledOnValidThread());
    canvas_handler_->RequestRefreshFrame();
  }
  void StopCapture() override {
    DCHECK(main_render_thread_checker_.CalledOnValidThread());
    if (canvas_handler_.get())
      canvas_handler_->StopVideoCapture();
  }

 private:
  const float frame_rate_;
  // Bound to Main Render thread.
  base::ThreadChecker main_render_thread_checker_;
  // CanvasCaptureHandler is owned by CanvasDrawListener in blink and might be
  // destroyed before StopCapture() call.
  base::WeakPtr<CanvasCaptureHandler> canvas_handler_;
};

class CanvasCaptureHandler::CanvasCaptureHandlerDelegate {
 public:
  explicit CanvasCaptureHandlerDelegate(
      media::VideoCapturerSource::VideoCaptureDeliverFrameCB new_frame_callback)
      : new_frame_callback_(new_frame_callback),
        weak_ptr_factory_(this) {
    io_thread_checker_.DetachFromThread();
  }
  ~CanvasCaptureHandlerDelegate() {
    DCHECK(io_thread_checker_.CalledOnValidThread());
  }

  void SendNewFrameOnIOThread(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& current_time) {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    new_frame_callback_.Run(video_frame, current_time);
  }

  base::WeakPtr<CanvasCaptureHandlerDelegate> GetWeakPtrForIOThread() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const media::VideoCapturerSource::VideoCaptureDeliverFrameCB
      new_frame_callback_;
  // Bound to IO thread.
  base::ThreadChecker io_thread_checker_;
  base::WeakPtrFactory<CanvasCaptureHandlerDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CanvasCaptureHandlerDelegate);
};

CanvasCaptureHandler::CanvasCaptureHandler(
    const blink::WebSize& size,
    double frame_rate,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    blink::WebMediaStreamTrack* track)
    : ask_for_new_frame_(false),
      size_(size),
      io_task_runner_(io_task_runner),
      weak_ptr_factory_(this) {
  std::unique_ptr<media::VideoCapturerSource> video_source(
      new VideoCapturerSource(weak_ptr_factory_.GetWeakPtr(), frame_rate));
  AddVideoCapturerSourceToVideoTrack(std::move(video_source), track);
}

CanvasCaptureHandler::~CanvasCaptureHandler() {
  DVLOG(3) << __func__;
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  io_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());
}

// static
std::unique_ptr<CanvasCaptureHandler>
CanvasCaptureHandler::CreateCanvasCaptureHandler(
    const blink::WebSize& size,
    double frame_rate,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    blink::WebMediaStreamTrack* track) {
  // Save histogram data so we can see how much CanvasCapture is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(WEBKIT_CANVAS_CAPTURE_STREAM);

  return std::unique_ptr<CanvasCaptureHandler>(
      new CanvasCaptureHandler(size, frame_rate, io_task_runner, track));
}

void CanvasCaptureHandler::SendNewFrame(const SkImage* image) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  CreateNewFrame(image);
}

bool CanvasCaptureHandler::NeedsNewFrame() const {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  return ask_for_new_frame_;
}

void CanvasCaptureHandler::StartVideoCapture(
    const media::VideoCaptureParams& params,
    const media::VideoCapturerSource::VideoCaptureDeliverFrameCB&
        new_frame_callback,
    const media::VideoCapturerSource::RunningCallback& running_callback) {
  DVLOG(3) << __func__ << " requested "
           << media::VideoCaptureFormat::ToString(params.requested_format);
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(params.requested_format.IsValid());
  capture_format_ = params.requested_format;
  delegate_.reset(new CanvasCaptureHandlerDelegate(new_frame_callback));
  DCHECK(delegate_);
  ask_for_new_frame_ = true;
  running_callback.Run(true);
}

void CanvasCaptureHandler::RequestRefreshFrame() {
  DVLOG(3) << __func__;
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (last_frame_ && delegate_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CanvasCaptureHandler::CanvasCaptureHandlerDelegate::
                           SendNewFrameOnIOThread,
                       delegate_->GetWeakPtrForIOThread(), last_frame_,
                       base::TimeTicks::Now()));
  }
}

void CanvasCaptureHandler::StopVideoCapture() {
  DVLOG(3) << __func__;
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  ask_for_new_frame_ = false;
  io_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());
}

void CanvasCaptureHandler::CreateNewFrame(const SkImage* image) {
  DVLOG(4) << __func__;
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(image);
  TRACE_EVENT0("webrtc", "CanvasCaptureHandler::CreateNewFrame");

  const uint8_t* source_ptr = nullptr;
  size_t source_length = 0;
  size_t source_stride = 0;
  gfx::Size image_size(image->width(), image->height());
  SkColorType source_color_type = kUnknown_SkColorType;

  // Initially try accessing pixels directly if they are in memory.
  SkPixmap pixmap;
  if (image->peekPixels(&pixmap) &&
      (pixmap.colorType() == kRGBA_8888_SkColorType ||
       pixmap.colorType() == kBGRA_8888_SkColorType) &&
      pixmap.alphaType() == kUnpremul_SkAlphaType) {
    source_ptr = static_cast<const uint8*>(pixmap.addr(0, 0));
    source_length = pixmap.getSafeSize64();
    source_stride = pixmap.rowBytes();
    image_size.set_width(pixmap.width());
    image_size.set_height(pixmap.height());
    source_color_type = pixmap.colorType();
  }

  // Copy the pixels into memory. This call may block main render thread.
  if (!source_ptr) {
    if (image_size != last_size) {
      temp_data_stride_ = kArgbBytesPerPixel * image_size.width();
      temp_data_.resize(temp_data_stride_ * image_size.height());
      image_info_ = SkImageInfo::MakeN32(
          image_size.width(), image_size.height(), kUnpremul_SkAlphaType);
      last_size = image_size;
    }
    if (image->readPixels(image_info_, &temp_data_[0], temp_data_stride_, 0,
                          0)) {
      source_ptr = temp_data_.data();
      source_length = temp_data_.size();
      source_stride = temp_data_stride_;
      source_color_type = kN32_SkColorType;
    } else {
      DLOG(ERROR) << "Couldn't read SkImage pixels";
      return;
    }
  }

  const bool isOpaque = image->isOpaque();
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  scoped_refptr<media::VideoFrame> video_frame = frame_pool_.CreateFrame(
      isOpaque ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_YV12A,
      image_size, gfx::Rect(image_size), image_size,
      timestamp - base::TimeTicks());
  DCHECK(video_frame);
  libyuv::FourCC source_pixel_format = libyuv::FOURCC_ANY;
  switch (source_color_type) {
    case kRGBA_8888_SkColorType:
      source_pixel_format = libyuv::FOURCC_ABGR;
      break;
    case kBGRA_8888_SkColorType:
      source_pixel_format = libyuv::FOURCC_ARGB;
      break;
    default:
      NOTREACHED() << "Unexpected SkColorType.";
  }
  libyuv::ConvertToI420(source_ptr, source_length,
                        video_frame->visible_data(media::VideoFrame::kYPlane),
                        video_frame->stride(media::VideoFrame::kYPlane),
                        video_frame->visible_data(media::VideoFrame::kUPlane),
                        video_frame->stride(media::VideoFrame::kUPlane),
                        video_frame->visible_data(media::VideoFrame::kVPlane),
                        video_frame->stride(media::VideoFrame::kVPlane),
                        0 /* crop_x */, 0 /* crop_y */, image_size.width(),
                        image_size.height(), image_size.width(),
                        image_size.height(), libyuv::kRotate0,
                        source_pixel_format);
  if (!isOpaque) {
    // It is ok to use ARGB function because alpha has the same alignment for
    // both ABGR and ARGB.
    libyuv::ARGBExtractAlpha(source_ptr, source_stride,
                             video_frame->visible_data(VideoFrame::kAPlane),
                             video_frame->stride(VideoFrame::kAPlane),
                             image_size.width(), image_size.height());
  }

  last_frame_ = video_frame;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CanvasCaptureHandler::CanvasCaptureHandlerDelegate::
                         SendNewFrameOnIOThread,
                     delegate_->GetWeakPtrForIOThread(), video_frame,
                     timestamp));
}

void CanvasCaptureHandler::AddVideoCapturerSourceToVideoTrack(
    std::unique_ptr<media::VideoCapturerSource> source,
    blink::WebMediaStreamTrack* web_track) {
  std::string str_track_id;
  base::Base64Encode(base::RandBytesAsString(64), &str_track_id);
  const blink::WebString track_id = blink::WebString::FromASCII(str_track_id);
  blink::WebMediaStreamSource webkit_source;
  std::unique_ptr<MediaStreamVideoSource> media_stream_source(
      new MediaStreamVideoCapturerSource(
          MediaStreamSource::SourceStoppedCallback(), std::move(source)));
  webkit_source.Initialize(track_id, blink::WebMediaStreamSource::kTypeVideo,
                           track_id, false);
  webkit_source.SetExtraData(media_stream_source.get());

  web_track->Initialize(webkit_source);
  web_track->SetTrackData(new MediaStreamVideoTrack(
      media_stream_source.release(),
      MediaStreamVideoSource::ConstraintsCallback(), true));
}

}  // namespace content
