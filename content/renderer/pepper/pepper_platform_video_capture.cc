// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_video_capture.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/pepper/pepper_media_device_manager.h"
#include "content/renderer/pepper/pepper_video_capture_host.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "media/video/capture/video_capture_proxy.h"
#include "url/gurl.h"

namespace content {

PepperPlatformVideoCapture::PepperPlatformVideoCapture(
    const base::WeakPtr<RenderViewImpl>& render_view,
    const std::string& device_id,
    const GURL& document_url,
    PepperVideoCaptureHost* handler)
    : render_view_(render_view),
      device_id_(device_id),
      session_id_(0),
      handler_proxy_(new media::VideoCaptureHandlerProxy(
          this,
          base::MessageLoopProxy::current())),
      handler_(handler),
      unbalanced_start_(false),
      pending_open_device_(false),
      pending_open_device_id_(-1) {
  // We need to open the device and obtain the label and session ID before
  // initializing.
  if (render_view_.get()) {
    pending_open_device_id_ = GetMediaDeviceManager()->OpenDevice(
        PP_DEVICETYPE_DEV_VIDEOCAPTURE,
        device_id,
        document_url,
        base::Bind(&PepperPlatformVideoCapture::OnDeviceOpened, this));
    pending_open_device_ = true;
  }
}

void PepperPlatformVideoCapture::StartCapture(
    media::VideoCapture::EventHandler* handler,
    const media::VideoCaptureParams& params) {
  DCHECK(handler == handler_);

  if (unbalanced_start_)
    return;

  if (video_capture_) {
    unbalanced_start_ = true;
    AddRef();  // Will be balanced in OnRemoved().
    video_capture_->StartCapture(handler_proxy_.get(), params);
  }
}

void PepperPlatformVideoCapture::StopCapture(
    media::VideoCapture::EventHandler* handler) {
  DCHECK(handler == handler_);
  if (!unbalanced_start_)
    return;

  if (video_capture_) {
    unbalanced_start_ = false;
    video_capture_->StopCapture(handler_proxy_.get());
  }
}

bool PepperPlatformVideoCapture::CaptureStarted() {
  return handler_proxy_->state().started;
}

int PepperPlatformVideoCapture::CaptureFrameRate() {
  return handler_proxy_->state().frame_rate;
}

void PepperPlatformVideoCapture::GetDeviceSupportedFormats(
    const DeviceFormatsCallback& callback) {
  NOTREACHED();
}

void PepperPlatformVideoCapture::GetDeviceFormatsInUse(
    const DeviceFormatsInUseCallback& callback) {
  NOTREACHED();
}

void PepperPlatformVideoCapture::DetachEventHandler() {
  handler_ = NULL;
  StopCapture(NULL);

  video_capture_.reset();

  if (render_view_.get()) {
    if (!label_.empty()) {
      GetMediaDeviceManager()->CloseDevice(label_);
      label_.clear();
    }
    if (pending_open_device_) {
      GetMediaDeviceManager()->CancelOpenDevice(pending_open_device_id_);
      pending_open_device_ = false;
      pending_open_device_id_ = -1;
    }
  }
}

void PepperPlatformVideoCapture::OnStarted(VideoCapture* capture) {
  if (handler_)
    handler_->OnStarted(capture);
}

void PepperPlatformVideoCapture::OnStopped(VideoCapture* capture) {
  if (handler_)
    handler_->OnStopped(capture);
}

void PepperPlatformVideoCapture::OnPaused(VideoCapture* capture) {
  if (handler_)
    handler_->OnPaused(capture);
}

void PepperPlatformVideoCapture::OnError(VideoCapture* capture,
                                         int error_code) {
  if (handler_)
    handler_->OnError(capture, error_code);
}

void PepperPlatformVideoCapture::OnRemoved(VideoCapture* capture) {
  if (handler_)
    handler_->OnRemoved(capture);

  Release();  // Balance the AddRef() in StartCapture().
}

void PepperPlatformVideoCapture::OnFrameReady(
    VideoCapture* capture,
    const scoped_refptr<media::VideoFrame>& frame) {
  if (handler_)
    handler_->OnFrameReady(capture, frame);
}

PepperPlatformVideoCapture::~PepperPlatformVideoCapture() {
  DCHECK(!video_capture_);
  DCHECK(label_.empty());
  DCHECK(!pending_open_device_);
}

void PepperPlatformVideoCapture::Initialize() {
  VideoCaptureImplManager* manager =
      RenderThreadImpl::current()->video_capture_impl_manager();
  video_capture_ = manager->UseDevice(session_id_);
}

void PepperPlatformVideoCapture::OnDeviceOpened(int request_id,
                                                bool succeeded,
                                                const std::string& label) {
  pending_open_device_ = false;
  pending_open_device_id_ = -1;

  succeeded = succeeded && render_view_.get();
  if (succeeded) {
    label_ = label;
    session_id_ = GetMediaDeviceManager()->GetSessionID(
        PP_DEVICETYPE_DEV_VIDEOCAPTURE, label);
    Initialize();
  }

  if (handler_)
    handler_->OnInitialized(this, succeeded);
}

PepperMediaDeviceManager* PepperPlatformVideoCapture::GetMediaDeviceManager() {
  return PepperMediaDeviceManager::GetForRenderView(render_view_.get());
}

}  // namespace content
