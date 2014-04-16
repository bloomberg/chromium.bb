// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_capturer_source.h"

#include "base/bind.h"
#include "base/location.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/video_frame.h"

namespace {

struct SourceVideoFormat {
  int width;
  int height;
  int frame_rate;
};

// List of formats used if the source doesn't support capability enumeration.
const SourceVideoFormat kVideoFormats[] = {{1920, 1080, 30},
                                           {1280, 720, 30},
                                           {960, 720, 30},
                                           {640, 480, 30},
                                           {640, 360, 30},
                                           {320, 240, 30},
                                           {320, 180, 30}};

}  // namespace

namespace content {

VideoCapturerDelegate::VideoCapturerDelegate(
    const StreamDeviceInfo& device_info)
    : session_id_(device_info.session_id),
      is_screen_cast_(device_info.device.type == MEDIA_TAB_VIDEO_CAPTURE ||
                      device_info.device.type == MEDIA_DESKTOP_VIDEO_CAPTURE),
      got_first_frame_(false) {
  DVLOG(3) << "VideoCapturerDelegate::ctor";
  // RenderThreadImpl::current() may be NULL in testing.
  if (RenderThreadImpl::current()) {
    capture_engine_ = RenderThreadImpl::current()->video_capture_impl_manager()
              ->UseDevice(device_info.session_id);
    DCHECK(capture_engine_);
  }
  message_loop_proxy_ = base::MessageLoopProxy::current();
}

VideoCapturerDelegate::~VideoCapturerDelegate() {
  DVLOG(3) << "VideoCapturerDelegate::dtor";
  DCHECK(new_frame_callback_.is_null());
}

void VideoCapturerDelegate::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    const SupportedFormatsCallback& callback) {
  DVLOG(3) << "GetCurrentSupportedFormats("
           << " { max_requested_height = " << max_requested_height << "})"
           << " { max_requested_width = " << max_requested_width << "})";

  if (is_screen_cast_) {
    media::VideoCaptureFormats formats;
    const int width = max_requested_width ?
        max_requested_width : MediaStreamVideoSource::kDefaultWidth;
    const int height = max_requested_height ?
        max_requested_height : MediaStreamVideoSource::kDefaultHeight;
    formats.push_back(
          media::VideoCaptureFormat(
              gfx::Size(width, height),
              MediaStreamVideoSource::kDefaultFrameRate,
              media::PIXEL_FORMAT_I420));
    callback.Run(formats);
    return;
  }

  DCHECK(source_formats_callback_.is_null());
  source_formats_callback_ = callback;
  capture_engine_->GetDeviceFormatsInUse(base::Bind(
      &VideoCapturerDelegate::OnDeviceFormatsInUseReceived, this));
}

void VideoCapturerDelegate::StartDeliver(
    const media::VideoCaptureParams& params,
    const NewFrameCallback& new_frame_callback,
    const StartedCallback& started_callback) {
  DCHECK(params.requested_format.IsValid());
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  new_frame_callback_ = new_frame_callback;
  started_callback_ = started_callback;
  got_first_frame_ = false;

  // Increase the reference count to ensure the object is not deleted until
  // it is unregistered in VideoCapturerDelegate::OnRemoved.
  AddRef();
  capture_engine_->StartCapture(this, params);
}

void VideoCapturerDelegate::StopDeliver() {
  // Immediately make sure we don't provide more frames.
  DVLOG(3) << "VideoCapturerDelegate::StopDeliver()";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  capture_engine_->StopCapture(this);
  new_frame_callback_.Reset();
  started_callback_.Reset();
  source_formats_callback_.Reset();
}

void VideoCapturerDelegate::OnStarted(media::VideoCapture* capture) {
  DVLOG(3) << "VideoCapturerDelegate::OnStarted";
  DCHECK(!message_loop_proxy_->BelongsToCurrentThread());
}

void VideoCapturerDelegate::OnStopped(media::VideoCapture* capture) {
  DCHECK(!message_loop_proxy_->BelongsToCurrentThread());
}

void VideoCapturerDelegate::OnPaused(media::VideoCapture* capture) {
  DCHECK(!message_loop_proxy_->BelongsToCurrentThread());
}

void VideoCapturerDelegate::OnError(media::VideoCapture* capture,
                                    int error_code) {
  DVLOG(3) << "VideoCapturerDelegate::OnError";
  DCHECK(!message_loop_proxy_->BelongsToCurrentThread());
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCapturerDelegate::OnErrorOnRenderThread, this, capture));
}

void VideoCapturerDelegate::OnRemoved(media::VideoCapture* capture) {
  DVLOG(3) << " MediaStreamVideoCapturerSource::OnRemoved";
  DCHECK(!message_loop_proxy_->BelongsToCurrentThread());

  // Balance the AddRef in StartDeliver.
  // This means we are no longer registered as an event handler and can safely
  // be deleted.
  Release();
}

void VideoCapturerDelegate::OnFrameReady(
    media::VideoCapture* capture,
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(!message_loop_proxy_->BelongsToCurrentThread());
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCapturerDelegate::OnFrameReadyOnRenderThread,
                 this,
                 capture,
                 frame));
}

void VideoCapturerDelegate::OnFrameReadyOnRenderThread(
    media::VideoCapture* capture,
    const scoped_refptr<media::VideoFrame>& frame) {
  if (!got_first_frame_) {
    got_first_frame_ = true;
    if (!started_callback_.is_null())
      started_callback_.Run(true);
  }

  if (!new_frame_callback_.is_null()) {
    new_frame_callback_.Run(frame);
  }
}

void VideoCapturerDelegate::OnErrorOnRenderThread(
    media::VideoCapture* capture) {
  if (!started_callback_.is_null())
    started_callback_.Run(false);
}

void VideoCapturerDelegate::OnDeviceFormatsInUseReceived(
    const media::VideoCaptureFormats& formats_in_use) {
  DVLOG(3) << "OnDeviceFormatsInUseReceived: " << formats_in_use.size();
  DCHECK(message_loop_proxy_ == base::MessageLoopProxy::current());
  // StopDeliver() might have destroyed |source_formats_callback_| before
  // arriving here.
  if (source_formats_callback_.is_null())
    return;
  if (!formats_in_use.empty()) {
    source_formats_callback_.Run(formats_in_use);
    source_formats_callback_.Reset();
  } else {
    // If there are no formats in use, try to retrieve the whole list of
    // supported formats.
    capture_engine_->GetDeviceSupportedFormats(base::Bind(
        &VideoCapturerDelegate::OnDeviceSupportedFormatsEnumerated, this));
  }
}

void VideoCapturerDelegate::OnDeviceSupportedFormatsEnumerated(
    const media::VideoCaptureFormats& formats) {
  DVLOG(3) << "OnDeviceSupportedFormatsEnumerated: " << formats.size()
           << " received";
  DCHECK(message_loop_proxy_ == base::MessageLoopProxy::current());
  // StopDeliver() might have destroyed |source_formats_callback_| before
  // arriving here.
  if (source_formats_callback_.is_null())
    return;
  if (formats.size()) {
    source_formats_callback_.Run(formats);
  } else {
    // The capture device doesn't seem to support capability enumeration,
    // compose a fallback list of capabilities.
    media::VideoCaptureFormats default_formats;
    for (size_t i = 0; i < arraysize(kVideoFormats); ++i) {
      default_formats.push_back(media::VideoCaptureFormat(
          gfx::Size(kVideoFormats[i].width, kVideoFormats[i].height),
          kVideoFormats[i].frame_rate,
          media::PIXEL_FORMAT_I420));
    }
    source_formats_callback_.Run(default_formats);
  }
  source_formats_callback_.Reset();
}

MediaStreamVideoCapturerSource::MediaStreamVideoCapturerSource(
    const StreamDeviceInfo& device_info,
    const SourceStoppedCallback& stop_callback,
    const scoped_refptr<VideoCapturerDelegate>& delegate,
    MediaStreamDependencyFactory* factory)
    : MediaStreamVideoSource(factory),
      delegate_(delegate) {
  SetDeviceInfo(device_info);
  SetStopCallback(stop_callback);
}

MediaStreamVideoCapturerSource::~MediaStreamVideoCapturerSource() {
}

void MediaStreamVideoCapturerSource::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height) {
  delegate_->GetCurrentSupportedFormats(
      max_requested_width,
      max_requested_height,
      base::Bind(&MediaStreamVideoCapturerSource::OnSupportedFormats,
                 base::Unretained(this)));
}

void MediaStreamVideoCapturerSource::StartSourceImpl(
    const media::VideoCaptureParams& params) {
  media::VideoCaptureParams new_params(params);
  if (device_info().device.type == MEDIA_TAB_VIDEO_CAPTURE ||
      device_info().device.type == MEDIA_DESKTOP_VIDEO_CAPTURE) {
    new_params.allow_resolution_change = true;
  }
  delegate_->StartDeliver(
      new_params,
      base::Bind(&MediaStreamVideoCapturerSource::DeliverVideoFrame,
                 base::Unretained(this)),
      base::Bind(&MediaStreamVideoCapturerSource::OnStartDone,
                 base::Unretained(this)));
}

void MediaStreamVideoCapturerSource::StopSourceImpl() {
  delegate_->StopDeliver();
}

}  // namespace content
