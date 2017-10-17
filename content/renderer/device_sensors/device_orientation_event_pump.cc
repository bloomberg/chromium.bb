// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/device_sensors/device_orientation_event_pump.h"

#include <cmath>

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_thread_impl.h"
#include "services/device/public/interfaces/sensor.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace {

bool IsAngleDifferentThreshold(bool has_angle1,
                               double angle1,
                               bool has_angle2,
                               double angle2) {
  if (has_angle1 != has_angle2)
    return true;

  return (has_angle1 &&
          std::fabs(angle1 - angle2) >=
              content::DeviceOrientationEventPump::kOrientationThreshold);
}

bool IsSignificantlyDifferent(const device::OrientationData& data1,
                              const device::OrientationData& data2) {
  return IsAngleDifferentThreshold(data1.has_alpha, data1.alpha,
                                   data2.has_alpha, data2.alpha) ||
         IsAngleDifferentThreshold(data1.has_beta, data1.beta, data2.has_beta,
                                   data2.beta) ||
         IsAngleDifferentThreshold(data1.has_gamma, data1.gamma,
                                   data2.has_gamma, data2.gamma);
}

}  // namespace

namespace content {

const double DeviceOrientationEventPump::kOrientationThreshold = 0.1;

DeviceOrientationEventPump::DeviceOrientationEventPump(RenderThread* thread,
                                                       bool absolute)
    : DeviceSensorEventPump<blink::WebDeviceOrientationListener>(thread),
      relative_orientation_sensor_(
          this,
          device::mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES),
      absolute_orientation_sensor_(
          this,
          device::mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES),
      absolute_(absolute),
      fall_back_to_absolute_orientation_sensor_(!absolute) {}

DeviceOrientationEventPump::~DeviceOrientationEventPump() {}

void DeviceOrientationEventPump::SendStartMessage() {
  // When running layout tests, those observers should not listen to the
  // actual hardware changes. In order to make that happen, don't connect
  // the other end of the mojo pipe to anything.
  //
  // TODO(sammc): Remove this when JS layout test support for shared buffers
  // is ready and the layout tests are converted to use that for mocking.
  // https://crbug.com/774183
  if (!RenderThreadImpl::current() ||
      RenderThreadImpl::current()->layout_test_mode()) {
    return;
  }

  if (!absolute_orientation_sensor_.sensor &&
      !relative_orientation_sensor_.sensor) {
    if (!sensor_provider_) {
      RenderFrame* const render_frame = GetRenderFrame();
      if (!render_frame)
        return;

      CHECK(render_frame->GetRemoteInterfaces());

      render_frame->GetRemoteInterfaces()->GetInterface(
          mojo::MakeRequest(&sensor_provider_));
      sensor_provider_.set_connection_error_handler(
          base::Bind(&DeviceSensorEventPump::HandleSensorProviderError,
                     base::Unretained(this)));
    }
    if (absolute_) {
      GetSensor(&absolute_orientation_sensor_);
    } else {
      fall_back_to_absolute_orientation_sensor_ = true;
      GetSensor(&relative_orientation_sensor_);
    }
  } else {
    if (relative_orientation_sensor_.sensor)
      relative_orientation_sensor_.sensor->Resume();

    if (absolute_orientation_sensor_.sensor)
      absolute_orientation_sensor_.sensor->Resume();

    DidStartIfPossible();
  }
}

void DeviceOrientationEventPump::SendStopMessage() {
  // SendStopMessage() gets called both when the page visibility changes and if
  // all device orientation event listeners are unregistered. Since removing
  // the event listener is more rare than the page visibility changing,
  // Sensor::Suspend() is used to optimize this case for not doing extra work.
  if (relative_orientation_sensor_.sensor)
    relative_orientation_sensor_.sensor->Suspend();

  if (absolute_orientation_sensor_.sensor)
    absolute_orientation_sensor_.sensor->Suspend();
}

void DeviceOrientationEventPump::SendFakeDataForTesting(void* fake_data) {
  if (!listener())
    return;

  device::OrientationData data =
      *static_cast<device::OrientationData*>(fake_data);
  listener()->DidChangeDeviceOrientation(data);
}

void DeviceOrientationEventPump::FireEvent() {
  device::OrientationData data;

  DCHECK(listener());

  GetDataFromSharedMemory(&data);

  if (ShouldFireEvent(data)) {
    data_ = data;
    listener()->DidChangeDeviceOrientation(data);
  }
}

void DeviceOrientationEventPump::DidStartIfPossible() {
  if (!absolute_ && !relative_orientation_sensor_.sensor &&
      fall_back_to_absolute_orientation_sensor_) {
    // When relative orientation sensor is not available fall back to using
    // the absolute orientation sensor but only on the first failure.
    fall_back_to_absolute_orientation_sensor_ = false;
    GetSensor(&absolute_orientation_sensor_);
    return;
  }
  DeviceSensorEventPump::DidStartIfPossible();
}

bool DeviceOrientationEventPump::SensorSharedBuffersReady() const {
  if (relative_orientation_sensor_.sensor &&
      !relative_orientation_sensor_.shared_buffer) {
    return false;
  }

  if (absolute_orientation_sensor_.sensor &&
      !absolute_orientation_sensor_.shared_buffer) {
    return false;
  }

  // At most one sensor can be successfully initialized.
  DCHECK(!relative_orientation_sensor_.sensor ||
         !absolute_orientation_sensor_.sensor);

  return true;
}

void DeviceOrientationEventPump::GetDataFromSharedMemory(
    device::OrientationData* data) {
  if (!absolute_ && relative_orientation_sensor_.SensorReadingCouldBeRead()) {
    // For DeviceOrientation Event, this provides relative orientation data.
    data->alpha = relative_orientation_sensor_.reading.orientation_euler.z;
    data->beta = relative_orientation_sensor_.reading.orientation_euler.x;
    data->gamma = relative_orientation_sensor_.reading.orientation_euler.y;
    data->has_alpha = !std::isnan(
        relative_orientation_sensor_.reading.orientation_euler.z.value());
    data->has_beta = !std::isnan(
        relative_orientation_sensor_.reading.orientation_euler.x.value());
    data->has_gamma = !std::isnan(
        relative_orientation_sensor_.reading.orientation_euler.y.value());
    data->absolute = false;
  } else if (absolute_orientation_sensor_.SensorReadingCouldBeRead()) {
    // For DeviceOrientationAbsolute Event, this provides absolute orientation
    // data.
    //
    // For DeviceOrientation Event, this provides absolute orientation data if
    // relative orientation data is not available.
    data->alpha = absolute_orientation_sensor_.reading.orientation_euler.z;
    data->beta = absolute_orientation_sensor_.reading.orientation_euler.x;
    data->gamma = absolute_orientation_sensor_.reading.orientation_euler.y;
    data->has_alpha = !std::isnan(
        absolute_orientation_sensor_.reading.orientation_euler.z.value());
    data->has_beta = !std::isnan(
        absolute_orientation_sensor_.reading.orientation_euler.x.value());
    data->has_gamma = !std::isnan(
        absolute_orientation_sensor_.reading.orientation_euler.y.value());
    data->absolute = true;
  } else {
    data->absolute = absolute_;
  }
}

bool DeviceOrientationEventPump::ShouldFireEvent(
    const device::OrientationData& data) const {
  if (!data.has_alpha && !data.has_beta && !data.has_gamma) {
    // no data can be provided, this is an all-null event.
    return true;
  }

  return IsSignificantlyDifferent(data_, data);
}

}  // namespace content
