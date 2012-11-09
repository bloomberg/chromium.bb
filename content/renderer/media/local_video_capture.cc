// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/local_video_capture.h"

#include "content/renderer/media/video_capture_impl_manager.h"
#include "media/base/video_util.h"
#include "media/video/capture/video_capture_proxy.h"

using media::CopyYPlane;
using media::CopyUPlane;
using media::CopyVPlane;

namespace content {

LocalVideoCapture::LocalVideoCapture(
    media::VideoCaptureSessionId video_stream_id,
    VideoCaptureImplManager* vc_manager,
    const media::VideoCaptureCapability& capability,
    const base::Closure& error_cb,
    const RepaintCB& repaint_cb)
    : video_stream_id_(video_stream_id),
      vc_manager_(vc_manager),
      capability_(capability),
      error_cb_(error_cb),
      repaint_cb_(repaint_cb),
      capture_engine_(NULL),
      message_loop_proxy_(base::MessageLoopProxy::current()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          handler_proxy_(new media::VideoCaptureHandlerProxy(
              this, message_loop_proxy_))),
      state_(VIDEO_CAPTURE_STATE_STOPPED) {
  DVLOG(3) << "LocalVideoCapture::ctor";
  DCHECK(vc_manager);
}

LocalVideoCapture::~LocalVideoCapture() {
  DVLOG(3) << "LocalVideoCapture::dtor";
}

void LocalVideoCapture::Start() {
  DVLOG(3) << "LocalVideoCapture::Start";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(state_, VIDEO_CAPTURE_STATE_STOPPED);

  capture_engine_ = vc_manager_->AddDevice(video_stream_id_, this);
  state_ = VIDEO_CAPTURE_STATE_STARTED;
  AddRef();  // Will be balanced in OnRemoved().
  capture_engine_->StartCapture(handler_proxy_.get(), capability_);
}

void LocalVideoCapture::Stop() {
  DVLOG(3) << "LocalVideoCapture::Stop";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (capture_engine_) {
    state_ = VIDEO_CAPTURE_STATE_STOPPING;
    capture_engine_->StopCapture(handler_proxy_.get());
    capture_engine_ = NULL;
  }
}

void LocalVideoCapture::Play() {
  DVLOG(3) << "LocalVideoCapture::Play";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (capture_engine_ && state_ == VIDEO_CAPTURE_STATE_PAUSED) {
    state_ = VIDEO_CAPTURE_STATE_STARTED;
  }
}

void LocalVideoCapture::Pause() {
  DVLOG(3) << "LocalVideoCapture::Pause";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (capture_engine_ && state_ == VIDEO_CAPTURE_STATE_STARTED) {
    state_ = VIDEO_CAPTURE_STATE_PAUSED;
  }
}

void LocalVideoCapture::OnStarted(media::VideoCapture* capture) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void LocalVideoCapture::OnStopped(media::VideoCapture* capture) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void LocalVideoCapture::OnPaused(media::VideoCapture* capture) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void LocalVideoCapture::OnError(media::VideoCapture* capture,
                                int error_code) {
  DVLOG(3) << "LocalVideoCapture::OnError, " << error_code;
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  error_cb_.Run();
}

void LocalVideoCapture::OnRemoved(media::VideoCapture* capture) {
  DVLOG(3) << "LocalVideoCapture::OnRemoved";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  // OnRemoved could be triggered by error.
  capture_engine_ = NULL;
  vc_manager_->RemoveDevice(video_stream_id_, this);
  Release();  // Balance the AddRef() in StartCapture().
}

void LocalVideoCapture::OnDeviceInfoReceived(
    media::VideoCapture* capture,
    const media::VideoCaptureParams& device_info) {
  DVLOG(3) << "LocalVideoCapture::OnDeviceInfoReceived";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
}

void LocalVideoCapture::OnBufferReady(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf) {
  DVLOG(3) << "LocalVideoCapture::OnBufferReady, state:" << state_;
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(buf);

  if (state_ != VIDEO_CAPTURE_STATE_STARTED) {
    capture->FeedBuffer(buf);
    return;
  }

  gfx::Size natural_size(buf->width, buf->height);
  scoped_refptr<media::VideoFrame> current_frame =
      media::VideoFrame::CreateFrame(media::VideoFrame::YV12, natural_size,
                                     gfx::Rect(natural_size), natural_size,
                                     buf->timestamp - base::Time());
  uint8* buffer = buf->memory_pointer;

  // Assume YV12 format.
  DCHECK_EQ(capability_.color, media::VideoCaptureCapability::kI420);
  if (capability_.color != media::VideoCaptureCapability::kI420)
    return;

  int y_width = buf->width;
  int y_height = buf->height;
  int uv_width = buf->width / 2;
  int uv_height = buf->height / 2;
  CopyYPlane(buffer, y_width, y_height, current_frame);
  buffer += y_width * y_height;
  CopyUPlane(buffer, uv_width, uv_height, current_frame);
  buffer += uv_width * uv_height;
  CopyVPlane(buffer, uv_width, uv_height, current_frame);

  capture->FeedBuffer(buf);
  repaint_cb_.Run(current_frame);
}

}  // namespace content
