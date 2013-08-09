// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "data_fetcher_shared_memory.h"

#include "base/logging.h"
#include "data_fetcher_impl_android.h"

namespace content {

DataFetcherSharedMemory::~DataFetcherSharedMemory() {
  if (started_)
    StopFetchingDeviceMotionData();
}

bool DataFetcherSharedMemory::NeedsPolling() {
  return false;
}

bool DataFetcherSharedMemory::FetchDeviceMotionDataIntoBuffer() {
  NOTREACHED();
  return false;
}

bool DataFetcherSharedMemory::StartFetchingDeviceMotionData(
    DeviceMotionHardwareBuffer* buffer) {
  DCHECK(buffer);
  device_motion_buffer_ = buffer;
  started_ = DataFetcherImplAndroid::GetInstance()->
      StartFetchingDeviceMotionData(buffer);
  return started_;
}

void DataFetcherSharedMemory::StopFetchingDeviceMotionData() {
  DataFetcherImplAndroid::GetInstance()->StopFetchingDeviceMotionData();
  started_ = false;
}

}  // namespace content
