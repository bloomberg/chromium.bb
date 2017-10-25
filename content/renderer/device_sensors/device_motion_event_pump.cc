// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/device_sensors/device_motion_event_pump.h"

#include <cmath>

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_thread_impl.h"
#include "device/sensors/public/cpp/motion_data.h"
#include "services/device/public/interfaces/sensor.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

DeviceMotionEventPump::DeviceMotionEventPump(RenderThread* thread)
    : DeviceSensorEventPump<blink::WebDeviceMotionListener>(thread),
      accelerometer_(this, device::mojom::SensorType::ACCELEROMETER),
      linear_acceleration_sensor_(
          this,
          device::mojom::SensorType::LINEAR_ACCELERATION),
      gyroscope_(this, device::mojom::SensorType::GYROSCOPE) {}

DeviceMotionEventPump::~DeviceMotionEventPump() {}

void DeviceMotionEventPump::SendStartMessage() {
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

  if (!accelerometer_.sensor && !linear_acceleration_sensor_.sensor &&
      !gyroscope_.sensor) {
    if (!sensor_provider_) {
      RenderFrame* const render_frame = GetRenderFrame();
      if (!render_frame)
        return;

      CHECK(render_frame->GetRemoteInterfaces());

      render_frame->GetRemoteInterfaces()->GetInterface(
          mojo::MakeRequest(&sensor_provider_));
      sensor_provider_.set_connection_error_handler(
          base::BindOnce(&DeviceSensorEventPump::HandleSensorProviderError,
                         base::Unretained(this)));
    }
    GetSensor(&accelerometer_);
    GetSensor(&linear_acceleration_sensor_);
    GetSensor(&gyroscope_);
  } else {
    if (accelerometer_.sensor)
      accelerometer_.sensor->Resume();

    if (linear_acceleration_sensor_.sensor)
      linear_acceleration_sensor_.sensor->Resume();

    if (gyroscope_.sensor)
      gyroscope_.sensor->Resume();

    DidStartIfPossible();
  }
}

void DeviceMotionEventPump::SendStopMessage() {
  // SendStopMessage() gets called both when the page visibility changes and if
  // all device motion event listeners are unregistered. Since removing the
  // event listener is more rare than the page visibility changing,
  // Sensor::Suspend() is used to optimize this case for not doing extra work.
  if (accelerometer_.sensor)
    accelerometer_.sensor->Suspend();

  if (linear_acceleration_sensor_.sensor)
    linear_acceleration_sensor_.sensor->Suspend();

  if (gyroscope_.sensor)
    gyroscope_.sensor->Suspend();
}

void DeviceMotionEventPump::SendFakeDataForTesting(void* fake_data) {
  if (!listener())
    return;

  device::MotionData data = *static_cast<device::MotionData*>(fake_data);
  listener()->DidChangeDeviceMotion(data);
}

void DeviceMotionEventPump::FireEvent() {
  device::MotionData data;
  // The device orientation spec states that interval should be in milliseconds.
  // https://w3c.github.io/deviceorientation/spec-source-orientation.html#devicemotion
  data.interval = kDefaultPumpDelayMicroseconds / 1000;

  DCHECK(listener());

  GetDataFromSharedMemory(&data);
  listener()->DidChangeDeviceMotion(data);
}

bool DeviceMotionEventPump::SensorSharedBuffersReady() const {
  if (accelerometer_.sensor && !accelerometer_.shared_buffer)
    return false;

  if (linear_acceleration_sensor_.sensor &&
      !linear_acceleration_sensor_.shared_buffer) {
    return false;
  }

  if (gyroscope_.sensor && !gyroscope_.shared_buffer)
    return false;

  return true;
}

void DeviceMotionEventPump::GetDataFromSharedMemory(device::MotionData* data) {
  if (accelerometer_.SensorReadingCouldBeRead()) {
    data->acceleration_including_gravity_x = accelerometer_.reading.accel.x;
    data->acceleration_including_gravity_y = accelerometer_.reading.accel.y;
    data->acceleration_including_gravity_z = accelerometer_.reading.accel.z;
    data->has_acceleration_including_gravity_x =
        !std::isnan(accelerometer_.reading.accel.x.value());
    data->has_acceleration_including_gravity_y =
        !std::isnan(accelerometer_.reading.accel.y.value());
    data->has_acceleration_including_gravity_z =
        !std::isnan(accelerometer_.reading.accel.z.value());
  }

  if (linear_acceleration_sensor_.SensorReadingCouldBeRead()) {
    data->acceleration_x = linear_acceleration_sensor_.reading.accel.x;
    data->acceleration_y = linear_acceleration_sensor_.reading.accel.y;
    data->acceleration_z = linear_acceleration_sensor_.reading.accel.z;
    data->has_acceleration_x =
        !std::isnan(linear_acceleration_sensor_.reading.accel.x.value());
    data->has_acceleration_y =
        !std::isnan(linear_acceleration_sensor_.reading.accel.y.value());
    data->has_acceleration_z =
        !std::isnan(linear_acceleration_sensor_.reading.accel.z.value());
  }

  if (gyroscope_.SensorReadingCouldBeRead()) {
    data->rotation_rate_alpha = gyroscope_.reading.gyro.x;
    data->rotation_rate_beta = gyroscope_.reading.gyro.y;
    data->rotation_rate_gamma = gyroscope_.reading.gyro.z;
    data->has_rotation_rate_alpha =
        !std::isnan(gyroscope_.reading.gyro.x.value());
    data->has_rotation_rate_beta =
        !std::isnan(gyroscope_.reading.gyro.y.value());
    data->has_rotation_rate_gamma =
        !std::isnan(gyroscope_.reading.gyro.z.value());
  }
}

}  // namespace content
