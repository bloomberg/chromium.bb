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

class DataFetcherSharedMemory {
 public:
  DataFetcherSharedMemory() : device_motion_buffer_(NULL) { }
  virtual ~DataFetcherSharedMemory();

  // Returns true if this fetcher needs explicit calls to fetch the data.
  virtual bool NeedsPolling();

  // If this fetcher NeedsPolling() is true, this method will update the
  // buffer with the latest device motion data.
  // This method will do nothing if NeedsPolling() is false.
  // Returns true if there was any motion data to update the buffer with.
  virtual bool FetchDeviceMotionDataIntoBuffer();

  // Returns true if the relevant sensors could be successfully activated.
  // This method should be called before any calls to
  // FetchDeviceMotionDataIntoBuffer().
  virtual bool StartFetchingDeviceMotionData(
      DeviceMotionHardwareBuffer* buffer);

  // Indicates to the fetcher to stop fetching device data.
  virtual void StopFetchingDeviceMotionData();

 private:
  DeviceMotionHardwareBuffer* device_motion_buffer_;

  DISALLOW_COPY_AND_ASSIGN(DataFetcherSharedMemory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_SHARED_MEMORY_H_
