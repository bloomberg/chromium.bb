// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/canvas_capture_handler.h"

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
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/libyuv/include/libyuv.h"

namespace content {

class CanvasCaptureHandler::VideoCapturerSource
    : public media::VideoCapturerSource {
 public:
  explicit VideoCapturerSource(base::WeakPtr<CanvasCaptureHandler>
                                   canvas_handler,
                               double frame_rate)
      : frame_rate_(frame_rate),
        canvas_handler_(canvas_handler) {}

 protected:
  void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      double max_requested_frame_rate,
      const VideoCaptureDeviceFormatsCB& callback) override {
    const blink::WebSize& size = canvas_handler_->GetSourceSize();
    const media::VideoCaptureFormat format(gfx::Size(size.width, size.height),
                                           frame_rate_,
                                           media::PIXEL_FORMAT_I420);
    media::VideoCaptureFormats formats;
    formats.push_back(format);
    callback.Run(formats);
  }
  void StartCapture(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& frame_callback,
                    const RunningCallback& running_callback) override {
    canvas_handler_->StartVideoCapture(params, frame_callback,
                                       running_callback);
  }
  void StopCapture() override {
    canvas_handler_->StopVideoCapture();
  }

 private:
  double frame_rate_;
  // CanvasCaptureHandler is owned by CanvasDrawListener in blink and
  // guaranteed to be alive during the lifetime of this class.
  base::WeakPtr<CanvasCaptureHandler> canvas_handler_;
};

class CanvasCaptureHandler::CanvasCaptureHandlerDelegate {
 public:
  explicit CanvasCaptureHandlerDelegate(
      media::VideoCapturerSource::VideoCaptureDeliverFrameCB new_frame_callback)
      : new_frame_callback_(new_frame_callback),
        weak_ptr_factory_(this) {
    thread_checker_.DetachFromThread();
  }
  ~CanvasCaptureHandlerDelegate() {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  void SendNewFrameOnIOThread(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& current_time) {
    DCHECK(thread_checker_.CalledOnValidThread());
    new_frame_callback_.Run(video_frame, current_time);
  }

  base::WeakPtr<CanvasCaptureHandlerDelegate> GetWeakPtrForIOThread() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const media::VideoCapturerSource::VideoCaptureDeliverFrameCB
      new_frame_callback_;
  base::ThreadChecker thread_checker_;
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
  scoped_ptr<media::VideoCapturerSource> video_source(
      new CanvasCaptureHandler::VideoCapturerSource(
          weak_ptr_factory_.GetWeakPtr(), frame_rate));
  AddVideoCapturerSourceToVideoTrack(std::move(video_source), track);
}

CanvasCaptureHandler::~CanvasCaptureHandler() {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());
}

// static
CanvasCaptureHandler* CanvasCaptureHandler::CreateCanvasCaptureHandler(
    const blink::WebSize& size,
    double frame_rate,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    blink::WebMediaStreamTrack* track) {
  // Save histogram data so we can see how much CanvasCapture is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(WEBKIT_CANVAS_CAPTURE_STREAM);

  return new CanvasCaptureHandler(size, frame_rate, io_task_runner, track);
}

void CanvasCaptureHandler::sendNewFrame(const SkImage* image) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateNewFrame(image);
}

bool CanvasCaptureHandler::needsNewFrame() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ask_for_new_frame_;
}

void CanvasCaptureHandler::StartVideoCapture(
    const media::VideoCaptureParams& params,
    const media::VideoCapturerSource::VideoCaptureDeliverFrameCB&
        new_frame_callback,
    const media::VideoCapturerSource::RunningCallback& running_callback) {
  DVLOG(3) << __FUNCTION__ << " requested "
           << media::VideoCaptureFormat::ToString(params.requested_format);
  DCHECK(params.requested_format.IsValid());
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(emircan): Accomodate to the given frame rate constraints here.
  capture_format_ = params.requested_format;
  delegate_.reset(new CanvasCaptureHandlerDelegate(new_frame_callback));
  DCHECK(delegate_);
  ask_for_new_frame_ = true;
  running_callback.Run(true);
}

void CanvasCaptureHandler::StopVideoCapture() {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  ask_for_new_frame_ = false;
  io_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());
}

void CanvasCaptureHandler::CreateNewFrame(const SkImage* image) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(image);
  const gfx::Size size(image->width(), image->height());
  if (size != last_size) {
    temp_data_.resize(
        media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_ARGB, size));
    row_bytes_ =
        media::VideoFrame::RowBytes(0, media::PIXEL_FORMAT_ARGB, size.width());
    image_info_ =
        SkImageInfo::Make(size.width(), size.height(), kBGRA_8888_SkColorType,
                          kPremul_SkAlphaType);
    last_size = size;
  }

  image->readPixels(image_info_, &temp_data_[0], row_bytes_, 0, 0);
  scoped_refptr<media::VideoFrame> video_frame =
      frame_pool_.CreateFrame(media::PIXEL_FORMAT_I420, size, gfx::Rect(size),
                              size, base::TimeTicks::Now() - base::TimeTicks());
  DCHECK(video_frame);

  libyuv::ARGBToI420(temp_data_.data(), row_bytes_,
                     video_frame->data(media::VideoFrame::kYPlane),
                     video_frame->stride(media::VideoFrame::kYPlane),
                     video_frame->data(media::VideoFrame::kUPlane),
                     video_frame->stride(media::VideoFrame::kUPlane),
                     video_frame->data(media::VideoFrame::kVPlane),
                     video_frame->stride(media::VideoFrame::kVPlane),
                     size.width(), size.height());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CanvasCaptureHandler::CanvasCaptureHandlerDelegate::
                     SendNewFrameOnIOThread,
                 delegate_->GetWeakPtrForIOThread(), video_frame,
                 base::TimeTicks()));
}

void CanvasCaptureHandler::AddVideoCapturerSourceToVideoTrack(
    scoped_ptr<media::VideoCapturerSource> source,
    blink::WebMediaStreamTrack* web_track) {
  std::string str_track_id;
  base::Base64Encode(base::RandBytesAsString(64), &str_track_id);
  const blink::WebString track_id = base::UTF8ToUTF16(str_track_id);
  blink::WebMediaStreamSource webkit_source;
  scoped_ptr<MediaStreamVideoSource> media_stream_source(
      new MediaStreamVideoCapturerSource(
          MediaStreamSource::SourceStoppedCallback(), std::move(source)));
  webkit_source.initialize(track_id, blink::WebMediaStreamSource::TypeVideo,
                           track_id, false, true);
  webkit_source.setExtraData(media_stream_source.get());

  web_track->initialize(webkit_source);
  blink::WebMediaConstraints constraints;
  constraints.initialize();
  web_track->setExtraData(new MediaStreamVideoTrack(
      media_stream_source.release(), constraints,
      MediaStreamVideoSource::ConstraintsCallback(), true));
}

}  // namespace content
