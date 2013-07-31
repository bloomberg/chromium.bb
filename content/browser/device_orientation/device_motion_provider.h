// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_DEVICE_MOTION_PROVIDER_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_DEVICE_MOTION_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "content/common/content_export.h"
#include "content/common/device_motion_hardware_buffer.h"

namespace content {
class DataFetcherSharedMemory;

// This class owns the shared memory buffer for Device Motion and makes
// sure the data is fetched into that buffer.
// When DataFetcherSharedMemory::NeedsPolling() is true, it starts a
// background polling thread to make sure the data is fetched at regular
// intervals.
class CONTENT_EXPORT DeviceMotionProvider {
 public:
  DeviceMotionProvider();

  // Creates provider with a custom fetcher. Used for testing.
  explicit DeviceMotionProvider(scoped_ptr<DataFetcherSharedMemory> fetcher);

  virtual ~DeviceMotionProvider();

  // Returns the shared memory handle of the device motion data duplicated
  // into the given process.
  base::SharedMemoryHandle GetSharedMemoryHandleForProcess(
      base::ProcessHandle renderer_process);

  void StartFetchingDeviceMotionData();
  void StopFetchingDeviceMotionData();

 private:
  class PollingThread;

  void Initialize();
  void CreateAndStartPollingThread();

  DeviceMotionHardwareBuffer* SharedMemoryAsHardwareBuffer();

  base::SharedMemory device_motion_shared_memory_;
  scoped_ptr<DataFetcherSharedMemory> data_fetcher_;
  scoped_ptr<PollingThread> polling_thread_;
  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMotionProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_DEVICE_MOTION_PROVIDER_H_
