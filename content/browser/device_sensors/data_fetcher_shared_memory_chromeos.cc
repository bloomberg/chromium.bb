// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/data_fetcher_shared_memory.h"

#include "content/browser/device_sensors/sensor_manager_chromeos.h"

namespace content {

DataFetcherSharedMemory::DataFetcherSharedMemory() {
}

DataFetcherSharedMemory::~DataFetcherSharedMemory() {
}

bool DataFetcherSharedMemory::Start(ConsumerType consumer_type, void* buffer) {
  DCHECK(buffer);
  if (!sensor_manager_)
    sensor_manager_.reset(new SensorManagerChromeOS);

  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      sensor_manager_->StartFetchingDeviceMotionData(
          static_cast<DeviceMotionHardwareBuffer*>(buffer));
      return true;
    case CONSUMER_TYPE_ORIENTATION:
      sensor_manager_->StartFetchingDeviceOrientationData(
          static_cast<DeviceOrientationHardwareBuffer*>(buffer));
      return true;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
    case CONSUMER_TYPE_LIGHT:
      NOTIMPLEMENTED();
      return false;
  }
  NOTREACHED();
  return false;
}

bool DataFetcherSharedMemory::Stop(ConsumerType consumer_type) {
  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      return sensor_manager_->StopFetchingDeviceMotionData();
    case CONSUMER_TYPE_ORIENTATION:
      return sensor_manager_->StopFetchingDeviceOrientationData();
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
    case CONSUMER_TYPE_LIGHT:
      NOTIMPLEMENTED();
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace content
