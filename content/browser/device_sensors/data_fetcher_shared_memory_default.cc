// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "data_fetcher_shared_memory.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"

namespace {

static bool SetMotionBuffer(content::DeviceMotionHardwareBuffer* buffer,
    bool enabled) {
  if (!buffer)
    return false;
  buffer->seqlock.WriteBegin();
  buffer->data.allAvailableSensorsAreActive = enabled;
  buffer->seqlock.WriteEnd();
  return true;
}

static bool SetOrientationBuffer(
    content::DeviceOrientationHardwareBuffer* buffer, bool enabled) {
  if (!buffer)
    return false;
  buffer->seqlock.WriteBegin();
  buffer->data.allAvailableSensorsAreActive = enabled;
  buffer->seqlock.WriteEnd();
  return true;
}

static bool SetLightBuffer(content::DeviceLightHardwareBuffer* buffer,
                           double lux) {
  if (!buffer)
    return false;
  buffer->seqlock.WriteBegin();
  buffer->data.value = lux;
  buffer->seqlock.WriteEnd();
  return true;
}

}  // namespace

namespace content {

DataFetcherSharedMemory::DataFetcherSharedMemory()
    : motion_buffer_(NULL), orientation_buffer_(NULL), light_buffer_(NULL) {
}

DataFetcherSharedMemory::~DataFetcherSharedMemory() {
}

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
    case CONSUMER_TYPE_LIGHT:
      light_buffer_ = static_cast<DeviceLightHardwareBuffer*>(buffer);
      return SetLightBuffer(light_buffer_,
                            std::numeric_limits<double>::infinity());
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
    case CONSUMER_TYPE_LIGHT:
      return SetLightBuffer(light_buffer_, -1);
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace content
