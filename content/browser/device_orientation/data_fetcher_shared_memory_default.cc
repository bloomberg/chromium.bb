// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "data_fetcher_shared_memory.h"

#include "base/logging.h"
#include "content/common/device_motion_hardware_buffer.h"
#include "content/common/device_orientation/device_orientation_hardware_buffer.h"

namespace {

static bool SetMotionBuffer(content::DeviceMotionHardwareBuffer* buffer,
    bool enabled) {
  DCHECK(buffer);
  buffer->seqlock.WriteBegin();
  buffer->data.allAvailableSensorsAreActive = enabled;
  buffer->seqlock.WriteEnd();
  return true;
}

}

namespace content {

DataFetcherSharedMemory::DataFetcherSharedMemory() {
}

DataFetcherSharedMemory::~DataFetcherSharedMemory() {
}

bool DataFetcherSharedMemory::Start(ConsumerType consumer_type) {
  void* buffer = GetSharedMemoryBuffer(consumer_type);
  DCHECK(buffer);

  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      return SetMotionBuffer(
          static_cast<DeviceMotionHardwareBuffer*>(buffer), true);
    case CONSUMER_TYPE_ORIENTATION:
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED();
  }
  return false;
}

bool DataFetcherSharedMemory::Stop(ConsumerType consumer_type) {
  void* buffer = GetSharedMemoryBuffer(consumer_type);
  DCHECK(buffer);

  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      return SetMotionBuffer(
          static_cast<DeviceMotionHardwareBuffer*>(buffer), false);
    case CONSUMER_TYPE_ORIENTATION:
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace content
