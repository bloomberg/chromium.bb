// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/sensors/data_fetcher_shared_memory.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

namespace {

bool SetMotionBuffer(device::DeviceMotionHardwareBuffer* buffer, bool enabled) {
  if (!buffer)
    return false;
  buffer->seqlock.WriteBegin();
  buffer->data.all_available_sensors_are_active = enabled;
  buffer->seqlock.WriteEnd();
  return true;
}

bool SetOrientationBuffer(device::DeviceOrientationHardwareBuffer* buffer,
                          bool enabled) {
  if (!buffer)
    return false;
  buffer->seqlock.WriteBegin();
  buffer->data.all_available_sensors_are_active = enabled;
  buffer->seqlock.WriteEnd();
  return true;
}

}  // namespace

namespace device {

DataFetcherSharedMemory::DataFetcherSharedMemory() {}

DataFetcherSharedMemory::~DataFetcherSharedMemory() {}

bool DataFetcherSharedMemory::Start(ConsumerType consumer_type, void* buffer) {
  DCHECK(buffer);

  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      motion_buffer_ = static_cast<DeviceMotionHardwareBuffer*>(buffer);
      UMA_HISTOGRAM_BOOLEAN("InertialSensor.MotionDefaultAvailable", false);
      return SetMotionBuffer(motion_buffer_, true);
    case CONSUMER_TYPE_ORIENTATION:
      orientation_buffer_ =
          static_cast<DeviceOrientationHardwareBuffer*>(buffer);
      UMA_HISTOGRAM_BOOLEAN("InertialSensor.OrientationDefaultAvailable",
                            false);
      return SetOrientationBuffer(orientation_buffer_, true);
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      orientation_absolute_buffer_ =
          static_cast<DeviceOrientationHardwareBuffer*>(buffer);
      return SetOrientationBuffer(orientation_absolute_buffer_, true);
    default:
      NOTREACHED();
  }
  return false;
}

bool DataFetcherSharedMemory::Stop(ConsumerType consumer_type) {
  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      return SetMotionBuffer(motion_buffer_, false);
    case CONSUMER_TYPE_ORIENTATION:
      return SetOrientationBuffer(orientation_buffer_, false);
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      return SetOrientationBuffer(orientation_absolute_buffer_, false);
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace device
