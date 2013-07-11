// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_DEVICE_MOTION_PROVIDER_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_DEVICE_MOTION_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "content/common/content_export.h"
#include "content/common/device_motion_hardware_buffer.h"

namespace content {

// TODO(timvolodine): Add data fetcher to this class.

class CONTENT_EXPORT DeviceMotionProvider {
 public:
  DeviceMotionProvider();
  virtual ~DeviceMotionProvider();

  // Returns the shared memory handle of the device motion data duplicated
  // into the given process.
  base::SharedMemoryHandle GetSharedMemoryHandleForProcess(
      base::ProcessHandle renderer_process);

  // Pause and resume the background polling thread. Can be called from any
  // thread.
  void StartFetchingDeviceMotionData();
  void StopFetchingDeviceMotionData();

 private:
  base::SharedMemory device_motion_shared_memory_;

  DeviceMotionHardwareBuffer* SharedMemoryAsHardwareBuffer();

  // TODO(timvolodine): add member variable,
  // scoped_ptr<DataFetcherSharedMemory> data_fetcher_;

  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMotionProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_DEVICE_MOTION_PROVIDER_H_
