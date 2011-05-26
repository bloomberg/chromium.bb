// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_impl_manager.h"

#include "base/memory/singleton.h"
#include "content/common/child_process.h"
#include "content/common/child_thread.h"
#include "content/common/video_capture_messages.h"
#include "content/common/view_messages.h"
#include "media/base/message_loop_factory_impl.h"

namespace {

// VideoCaptureMessageFilterCreator is to be used as a singleton so we can get
// access to a shared VideoCaptureMessageFilter.
// Example usage:
//   VideoCaptureMessageFilter* filter =
//       VideoCaptureMessageFilterCreator::SharedFilter();

class VideoCaptureMessageFilterCreator {
 public:
  VideoCaptureMessageFilterCreator() {
    int routing_id;
    ChildThread::current()->Send(
        new ViewHostMsg_GenerateRoutingID(&routing_id));
    filter_ = new VideoCaptureMessageFilter(routing_id);
    filter_->AddFilter();
  }

  static VideoCaptureMessageFilter* SharedFilter() {
    return GetInstance()->filter_.get();
  }

  static VideoCaptureMessageFilterCreator* GetInstance() {
    return Singleton<VideoCaptureMessageFilterCreator>::get();
  }

 private:
  scoped_refptr<VideoCaptureMessageFilter> filter_;
};

}  // namespace

VideoCaptureImplManager::VideoCaptureImplManager() {
  ml_factory_.reset(new media::MessageLoopFactoryImpl());
  ml_proxy_ = ml_factory_->GetMessageLoopProxy("VC manager");
}

VideoCaptureImplManager::~VideoCaptureImplManager() {}

// static
media::VideoCapture* VideoCaptureImplManager::AddDevice(
    media::VideoCaptureSessionId id,
    media::VideoCapture::EventHandler* handler) {
  DCHECK(handler);
  VideoCaptureImplManager* manager = GetInstance();

  base::AutoLock auto_lock(manager->lock_);
  Devices::iterator it = manager->devices_.find(id);
  if (it == manager->devices_.end()) {
    VideoCaptureImpl* vc =
        new VideoCaptureImpl(id, manager->ml_proxy_,
                             VideoCaptureMessageFilterCreator::SharedFilter());
    manager->devices_[id] = Device(vc, handler);
    vc->Init();
    return vc;
  }

  manager->devices_[id].clients.push_front(handler);
  return it->second.vc;
}

// static
void VideoCaptureImplManager::RemoveDevice(
    media::VideoCaptureSessionId id,
    media::VideoCapture::EventHandler* handler) {
  DCHECK(handler);
  VideoCaptureImplManager* manager = GetInstance();

  base::AutoLock auto_lock(manager->lock_);
  Devices::iterator it = manager->devices_.find(id);
  if (it == manager->devices_.end())
    return;

  size_t size = it->second.clients.size();
  it->second.clients.remove(handler);

  if (size == it->second.clients.size() || size > 1)
    return;

  manager->devices_[id].vc->DeInit(NewRunnableMethod(manager,
      &VideoCaptureImplManager::FreeDevice, manager->devices_[id].vc));
  manager->devices_.erase(id);
  return;
}

// static
VideoCaptureImplManager* VideoCaptureImplManager::GetInstance() {
  return Singleton<VideoCaptureImplManager>::get();
}

void VideoCaptureImplManager::FreeDevice(VideoCaptureImpl* vc) {
  delete vc;
}

VideoCaptureImplManager::Device::Device() : vc(NULL) {}

VideoCaptureImplManager::Device::Device(
    VideoCaptureImpl* device,
    media::VideoCapture::EventHandler* handler)
    : vc(device) {
  clients.push_front(handler);
}

VideoCaptureImplManager::Device::~Device() {}

