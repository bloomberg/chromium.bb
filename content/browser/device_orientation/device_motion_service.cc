// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/device_motion_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "content/browser/device_orientation/device_motion_provider.h"
#include "content/public/browser/render_process_host.h"

namespace content {

DeviceMotionService::DeviceMotionService()
    : num_readers_(0),
      is_shutdown_(false) {
}

DeviceMotionService::~DeviceMotionService() {
}

DeviceMotionService* DeviceMotionService::GetInstance() {
  return Singleton<DeviceMotionService,
                   LeakySingletonTraits<DeviceMotionService> >::get();
}

void DeviceMotionService::AddConsumer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_shutdown_)
    return;

  num_readers_++;
  DCHECK(num_readers_ > 0);
  if (!provider_.get())
    provider_.reset(new DeviceMotionProvider);
  provider_->StartFetchingDeviceMotionData();
}

void DeviceMotionService::RemoveConsumer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_shutdown_)
    return;

  --num_readers_;
  DCHECK(num_readers_ >= 0);

  if (num_readers_ == 0) {
    LOG(INFO) << "ACTIVE service stop fetching";
    provider_->StopFetchingDeviceMotionData();
  }
}

void DeviceMotionService::Shutdown() {
  provider_.reset();
  is_shutdown_ = true;
}

base::SharedMemoryHandle DeviceMotionService::GetSharedMemoryHandleForProcess(
    base::ProcessHandle handle) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return provider_->GetSharedMemoryHandleForProcess(handle);
}

}  // namespace content
