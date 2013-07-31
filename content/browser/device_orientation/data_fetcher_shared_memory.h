// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_SHARED_MEMORY_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_SHARED_MEMORY_H_

#include "content/browser/device_orientation/device_data.h"
#include "content/common/device_motion_hardware_buffer.h"

namespace WebKit {
class WebDeviceMotionData;
}

namespace content {

class CONTENT_EXPORT DataFetcherSharedMemory {
 public:
  DataFetcherSharedMemory()
      : device_motion_buffer_(NULL),
        started_(false) { }
  virtual ~DataFetcherSharedMemory();

  // Returns true if this fetcher needs explicit calls to fetch the data.
  // Called from any thread.
  virtual bool NeedsPolling();

  // If this fetcher NeedsPolling() is true, this method will update the
  // buffer with the latest device motion data.
  // Returns true if there was any motion data to update the buffer with.
  // Called from the DeviceMotionProvider::PollingThread.
  virtual bool FetchDeviceMotionDataIntoBuffer();

  // Returns true if the relevant sensors could be successfully activated.
  // This method should be called before any calls to
  // FetchDeviceMotionDataIntoBuffer().
  // If NeedsPolling() is true this method should be called from the
  // PollingThread.
  virtual bool StartFetchingDeviceMotionData(
      DeviceMotionHardwareBuffer* buffer);

  // Indicates to the fetcher to stop fetching device data.
  // If NeedsPolling() is true this method should be called from the
  // PollingThread.
  virtual void StopFetchingDeviceMotionData();

 private:
  DeviceMotionHardwareBuffer* device_motion_buffer_;
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(DataFetcherSharedMemory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_SHARED_MEMORY_H_
