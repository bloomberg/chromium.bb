// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_video_capture_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/pepper/pepper_plugin_delegate_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "media/video/capture/video_capture_proxy.h"

namespace content {

PepperPlatformVideoCaptureImpl::PepperPlatformVideoCaptureImpl(
    const base::WeakPtr<PepperPluginDelegateImpl>& plugin_delegate,
    const std::string& device_id,
    webkit::ppapi::PluginDelegate::PlatformVideoCaptureEventHandler* handler)
    : plugin_delegate_(plugin_delegate),
      device_id_(device_id),
      session_id_(0),
      handler_proxy_(new media::VideoCaptureHandlerProxy(
          this, base::MessageLoopProxy::current())),
      handler_(handler),
      video_capture_(NULL),
      unbalanced_start_(false) {
  if (device_id.empty()) {
    // "1" is the session ID for the default device.
    session_id_ = 1;
    Initialize();
  } else {
    // We need to open the device and obtain the label and session ID before
    // initializing.
    if (plugin_delegate_) {
      plugin_delegate_->OpenDevice(
          PP_DEVICETYPE_DEV_VIDEOCAPTURE, device_id,
          base::Bind(&PepperPlatformVideoCaptureImpl::OnDeviceOpened, this));
    }
  }
}

void PepperPlatformVideoCaptureImpl::StartCapture(
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

void PepperPlatformVideoCaptureImpl::StopCapture(
    media::VideoCapture::EventHandler* handler) {
  DCHECK(handler == handler_);
  if (!unbalanced_start_)
    return;

  if (video_capture_) {
    unbalanced_start_ = false;
    video_capture_->StopCapture(handler_proxy_.get());
  }
}

void PepperPlatformVideoCaptureImpl::FeedBuffer(
    scoped_refptr<VideoFrameBuffer> buffer) {
  if (video_capture_)
    video_capture_->FeedBuffer(buffer);
}

bool PepperPlatformVideoCaptureImpl::CaptureStarted() {
  return handler_proxy_->state().started;
}

int PepperPlatformVideoCaptureImpl::CaptureWidth() {
  return handler_proxy_->state().width;
}

int PepperPlatformVideoCaptureImpl::CaptureHeight() {
  return handler_proxy_->state().height;
}

int PepperPlatformVideoCaptureImpl::CaptureFrameRate() {
  return handler_proxy_->state().frame_rate;
}

void PepperPlatformVideoCaptureImpl::DetachEventHandler() {
  handler_ = NULL;
  StopCapture(NULL);
}

void PepperPlatformVideoCaptureImpl::OnStarted(VideoCapture* capture) {
  if (handler_)
    handler_->OnStarted(capture);
}

void PepperPlatformVideoCaptureImpl::OnStopped(VideoCapture* capture) {
  if (handler_)
    handler_->OnStopped(capture);
}

void PepperPlatformVideoCaptureImpl::OnPaused(VideoCapture* capture) {
  if (handler_)
    handler_->OnPaused(capture);
}

void PepperPlatformVideoCaptureImpl::OnError(
    VideoCapture* capture,
    int error_code) {
  if (handler_)
    handler_->OnError(capture, error_code);
}

void PepperPlatformVideoCaptureImpl::OnRemoved(VideoCapture* capture) {
  if (handler_)
    handler_->OnRemoved(capture);

  Release();  // Balance the AddRef() in StartCapture().
}

void PepperPlatformVideoCaptureImpl::OnBufferReady(
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

void PepperPlatformVideoCaptureImpl::OnDeviceInfoReceived(
    VideoCapture* capture,
    const media::VideoCaptureParams& device_info) {
  if (handler_)
    handler_->OnDeviceInfoReceived(capture, device_info);
}

PepperPlatformVideoCaptureImpl::~PepperPlatformVideoCaptureImpl() {
  if (video_capture_) {
    VideoCaptureImplManager* manager =
        RenderThreadImpl::current()->video_capture_impl_manager();
    manager->RemoveDevice(session_id_, handler_proxy_.get());
  }

  if (plugin_delegate_ && !label_.empty())
    plugin_delegate_->CloseDevice(label_);
}

void PepperPlatformVideoCaptureImpl::Initialize() {
  VideoCaptureImplManager* manager =
      RenderThreadImpl::current()->video_capture_impl_manager();
  video_capture_ = manager->AddDevice(session_id_, handler_proxy_.get());
}

void PepperPlatformVideoCaptureImpl::OnDeviceOpened(int request_id,
                                                    bool succeeded,
                                                    const std::string& label) {
  succeeded = succeeded && plugin_delegate_;
  if (succeeded) {
    label_ = label;
    session_id_ = plugin_delegate_->GetSessionID(PP_DEVICETYPE_DEV_VIDEOCAPTURE,
                                                 label);
    Initialize();
  }

  if (handler_)
    handler_->OnInitialized(this, succeeded);
}

}  // namespace content
