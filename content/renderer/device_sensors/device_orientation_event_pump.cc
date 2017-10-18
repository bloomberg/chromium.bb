// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_orientation_event_pump.h"

#include <string.h>

#include <cmath>

#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceOrientationListener.h"

namespace content {

const double DeviceOrientationEventPumpBase::kOrientationThreshold = 0.1;

DeviceOrientationEventPumpBase::DeviceOrientationEventPumpBase(
    RenderThread* thread)
    : DeviceSensorEventPump<blink::WebDeviceOrientationListener>(thread) {}

DeviceOrientationEventPumpBase::~DeviceOrientationEventPumpBase() {}

void DeviceOrientationEventPumpBase::FireEvent() {
  DCHECK(listener());
  device::OrientationData data;
  if (reader_->GetLatestData(&data) && ShouldFireEvent(data)) {
    memcpy(&data_, &data, sizeof(data));
    listener()->DidChangeDeviceOrientation(data);
  }
}

static bool IsSignificantlyDifferent(bool hasAngle1, double angle1,
    bool hasAngle2, double angle2) {
  if (hasAngle1 != hasAngle2)
    return true;
  return (hasAngle1 &&
          std::fabs(angle1 - angle2) >=
              DeviceOrientationEventPumpBase::kOrientationThreshold);
}

bool DeviceOrientationEventPumpBase::ShouldFireEvent(
    const device::OrientationData& data) const {
  if (!data.all_available_sensors_are_active)
    return false;

  if (!data.has_alpha && !data.has_beta && !data.has_gamma) {
    // no data can be provided, this is an all-null event.
    return true;
  }

  return IsSignificantlyDifferent(data_.has_alpha, data_.alpha, data.has_alpha,
                                  data.alpha) ||
         IsSignificantlyDifferent(data_.has_beta, data_.beta, data.has_beta,
                                  data.beta) ||
         IsSignificantlyDifferent(data_.has_gamma, data_.gamma, data.has_gamma,
                                  data.gamma);
}

bool DeviceOrientationEventPumpBase::InitializeReader(
    base::SharedMemoryHandle handle) {
  memset(&data_, 0, sizeof(data_));
  if (!reader_)
    reader_.reset(new DeviceOrientationSharedMemoryReader());
  return reader_->Initialize(handle);
}

void DeviceOrientationEventPumpBase::SendFakeDataForTesting(void* fake_data) {
  if (!listener())
    return;

  device::OrientationData data =
      *static_cast<device::OrientationData*>(fake_data);
  listener()->DidChangeDeviceOrientation(data);
}

}  // namespace content
