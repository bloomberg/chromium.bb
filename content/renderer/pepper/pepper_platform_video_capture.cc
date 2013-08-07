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
          this, base::MessageLoopProxy::current())),
      handler_(handler),
      video_capture_(NULL),
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
    const media::VideoCaptureCapability& capability) {
  DCHECK(handler == handler_);

  if (unbalanced_start_)
    return;

  if (video_capture_) {
    unbalanced_start_ = true;
    AddRef();  // Will be balanced in OnRemoved().
    video_capture_->StartCapture(handler_proxy_.get(), capability);
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

void PepperPlatformVideoCapture::FeedBuffer(
    scoped_refptr<VideoFrameBuffer> buffer) {
  if (video_capture_)
    video_capture_->FeedBuffer(buffer);
}

bool PepperPlatformVideoCapture::CaptureStarted() {
  return handler_proxy_->state().started;
}

int PepperPlatformVideoCapture::CaptureWidth() {
  return handler_proxy_->state().width;
}

int PepperPlatformVideoCapture::CaptureHeight() {
  return handler_proxy_->state().height;
}

int PepperPlatformVideoCapture::CaptureFrameRate() {
  return handler_proxy_->state().frame_rate;
}

void PepperPlatformVideoCapture::DetachEventHandler() {
  handler_ = NULL;
  StopCapture(NULL);

  if (video_capture_) {
    VideoCaptureImplManager* manager =
        RenderThreadImpl::current()->video_capture_impl_manager();
    manager->RemoveDevice(session_id_, handler_proxy_.get());
    video_capture_ = NULL;
  }

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

void PepperPlatformVideoCapture::OnBufferReady(
    VideoCapture* capture,
    scoped_refptr<VideoFrameBuffer> buffer) {
  if (handler_) {
    handler_->OnBufferReady(capture, buffer);
  } else {
    // Even after handler_ is detached, we have to return buffers that are in
    // flight to us. Otherwise VideoCaptureController will not tear down.
    FeedBuffer(buffer);
  }
}

void PepperPlatformVideoCapture::OnDeviceInfoReceived(
    VideoCapture* capture,
    const media::VideoCaptureParams& device_info) {
  if (handler_)
    handler_->OnDeviceInfoReceived(capture, device_info);
}

PepperPlatformVideoCapture::~PepperPlatformVideoCapture() {
  DCHECK(!video_capture_);
  DCHECK(label_.empty());
  DCHECK(!pending_open_device_);
}

void PepperPlatformVideoCapture::Initialize() {
  VideoCaptureImplManager* manager =
      RenderThreadImpl::current()->video_capture_impl_manager();
  video_capture_ = manager->AddDevice(session_id_, handler_proxy_.get());
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

PepperMediaDeviceManager*
    PepperPlatformVideoCapture::GetMediaDeviceManager() {
  return PepperMediaDeviceManager::GetForRenderView(render_view_.get());
}

}  // namespace content
