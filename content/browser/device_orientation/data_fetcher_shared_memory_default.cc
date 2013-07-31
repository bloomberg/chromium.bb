// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "data_fetcher_shared_memory.h"

#include "base/logging.h"

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
  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.allAvailableSensorsAreActive = true;
  device_motion_buffer_->seqlock.WriteEnd();
  started_ = true;
  return true;
}

void DataFetcherSharedMemory::StopFetchingDeviceMotionData() {
  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.allAvailableSensorsAreActive = false;
  device_motion_buffer_->seqlock.WriteEnd();
  started_ = false;
}

}  // namespace content
