// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_impl_manager.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "content/renderer/media/video_capture_impl.h"
#include "content/renderer/media/video_capture_message_filter.h"

namespace content {

VideoCaptureImplManager::VideoCaptureImplManager()
    : thread_("VC manager") {
  thread_.Start();
  message_loop_proxy_ = thread_.message_loop_proxy();
  filter_ = new VideoCaptureMessageFilter();
}

media::VideoCapture* VideoCaptureImplManager::AddDevice(
    media::VideoCaptureSessionId id,
    media::VideoCapture::EventHandler* handler) {
  DCHECK(handler);

  base::AutoLock auto_lock(lock_);
  Devices::iterator it = devices_.find(id);
  if (it == devices_.end()) {
    VideoCaptureImpl* vc =
        new VideoCaptureImpl(id, message_loop_proxy_, filter_);
    devices_[id] = new Device(vc, handler);
    vc->Init();
    return vc;
  }

  devices_[id]->clients.push_front(handler);
  return it->second->vc;
}

void VideoCaptureImplManager::RemoveDevice(
    media::VideoCaptureSessionId id,
    media::VideoCapture::EventHandler* handler) {
  DCHECK(handler);

  base::AutoLock auto_lock(lock_);
  Devices::iterator it = devices_.find(id);
  if (it == devices_.end())
    return;

  size_t size = it->second->clients.size();
  it->second->clients.remove(handler);

  if (size == it->second->clients.size() || size > 1)
    return;

  devices_[id]->vc->DeInit(base::Bind(&VideoCaptureImplManager::FreeDevice,
                                      this, devices_[id]->vc));
  delete devices_[id];
  devices_.erase(id);
}

void VideoCaptureImplManager::FreeDevice(VideoCaptureImpl* vc) {
  delete vc;
}

VideoCaptureImplManager::~VideoCaptureImplManager() {
  thread_.Stop();
  // TODO(wjia): uncomment the line below after collecting enough info for
  // crbug.com/152418.
  // STLDeleteContainerPairSecondPointers(devices_.begin(), devices_.end());
}

VideoCaptureImplManager::Device::Device(
    VideoCaptureImpl* device,
    media::VideoCapture::EventHandler* handler)
    : vc(device) {
  clients.push_front(handler);
}

VideoCaptureImplManager::Device::~Device() {}

}  // namespace content
