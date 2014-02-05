// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_impl_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/media/video_capture_impl.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "media/base/bind_to_current_loop.h"

namespace content {

VideoCaptureHandle::VideoCaptureHandle(
    media::VideoCapture* impl, base::Closure destruction_cb)
    : impl_(impl), destruction_cb_(destruction_cb) {
}

VideoCaptureHandle::~VideoCaptureHandle() {
  destruction_cb_.Run();
}

void VideoCaptureHandle::StartCapture(
    EventHandler* handler,
    const media::VideoCaptureParams& params) {
  impl_->StartCapture(handler, params);
}

void VideoCaptureHandle::StopCapture(EventHandler* handler) {
  impl_->StopCapture(handler);
}

bool VideoCaptureHandle::CaptureStarted() {
  return impl_->CaptureStarted();
}

int VideoCaptureHandle::CaptureFrameRate() {
  return impl_->CaptureFrameRate();
}

void VideoCaptureHandle::GetDeviceSupportedFormats(
    const DeviceFormatsCallback& callback) {
  impl_->GetDeviceSupportedFormats(callback);
}

void VideoCaptureHandle::GetDeviceFormatsInUse(
    const DeviceFormatsInUseCallback& callback) {
  impl_->GetDeviceFormatsInUse(callback);
}

VideoCaptureImplManager::VideoCaptureImplManager()
    : filter_(new VideoCaptureMessageFilter()),
      weak_factory_(this) {
}

VideoCaptureImplManager::~VideoCaptureImplManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

scoped_ptr<VideoCaptureHandle> VideoCaptureImplManager::UseDevice(
    media::VideoCaptureSessionId id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VideoCaptureImpl* video_capture_device = NULL;
  VideoCaptureDeviceMap::iterator it = devices_.find(id);
  if (it == devices_.end()) {
    video_capture_device = CreateVideoCaptureImpl(id, filter_.get());
    devices_[id] =
        std::make_pair(1, linked_ptr<VideoCaptureImpl>(video_capture_device));
    video_capture_device->Init();
  } else {
    ++it->second.first;
    video_capture_device = it->second.second.get();
  }

  // This callback ensures UnrefDevice() happens on the render thread.
  return scoped_ptr<VideoCaptureHandle>(
      new VideoCaptureHandle(
          video_capture_device,
          media::BindToCurrentLoop(
              base::Bind(
                  &VideoCaptureImplManager::UnrefDevice,
                  weak_factory_.GetWeakPtr(),
                  id))));
}

VideoCaptureImpl* VideoCaptureImplManager::CreateVideoCaptureImpl(
    media::VideoCaptureSessionId id,
    VideoCaptureMessageFilter* filter) const {
  return new VideoCaptureImpl(id, filter);
}

void VideoCaptureImplManager::UnrefDevice(
    media::VideoCaptureSessionId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VideoCaptureDeviceMap::iterator it = devices_.find(id);
  DCHECK(it != devices_.end());

  DCHECK(it->second.first);
  --it->second.first;
  if (!it->second.first) {
    VideoCaptureImpl* impl = it->second.second.release();
    devices_.erase(id);
    impl->DeInit(base::Bind(&base::DeletePointer<VideoCaptureImpl>, impl));
  }
}

void VideoCaptureImplManager::SuspendDevices(bool suspend) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (VideoCaptureDeviceMap::iterator it = devices_.begin();
       it != devices_.end(); ++it)
    it->second.second->SuspendCapture(suspend);
}

}  // namespace content
