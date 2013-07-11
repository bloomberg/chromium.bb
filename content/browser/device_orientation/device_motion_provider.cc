// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/browser/device_orientation/device_motion_provider.h"
#include "content/common/device_motion_hardware_buffer.h"

namespace content {

DeviceMotionProvider::DeviceMotionProvider()
    : is_started_(false) {
  size_t data_size = sizeof(DeviceMotionHardwareBuffer);
  bool res = device_motion_shared_memory_.CreateAndMapAnonymous(data_size);
  // TODO(timvolodine): consider not crashing the browser if the check fails.
  CHECK(res);
  DeviceMotionHardwareBuffer* hwbuf = SharedMemoryAsHardwareBuffer();
  memset(hwbuf, 0, sizeof(DeviceMotionHardwareBuffer));
}

DeviceMotionProvider::~DeviceMotionProvider() {
}

base::SharedMemoryHandle DeviceMotionProvider::GetSharedMemoryHandleForProcess(
    base::ProcessHandle process) {
  base::SharedMemoryHandle renderer_handle;
  device_motion_shared_memory_.ShareToProcess(process, &renderer_handle);
  return renderer_handle;
}

void DeviceMotionProvider::StartFetchingDeviceMotionData() {
  if (is_started_)
    return;
  // TODO(timvolodine): call data_fetcher_->StartFetchingDeviceMotionData(
  // SharedMemoryAsHardwareBuffer());
  is_started_ = true;
}

void DeviceMotionProvider::StopFetchingDeviceMotionData() {
  // TODO(timvolodine): call data_fetcher_->StopFetchingDeviceMotionData();
  is_started_ = false;
}

DeviceMotionHardwareBuffer* DeviceMotionProvider::
    SharedMemoryAsHardwareBuffer() {
  void* mem = device_motion_shared_memory_.memory();
  CHECK(mem);
  return static_cast<DeviceMotionHardwareBuffer*>(mem);
}

}  // namespace content
