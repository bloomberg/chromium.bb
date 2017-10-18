// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/device_sensors/device_motion_event_pump.h"

#include "base/memory/ptr_util.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceMotionListener.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

DeviceMotionEventPump::DeviceMotionEventPump(RenderThread* thread)
    : PlatformEventObserver<blink::WebDeviceMotionListener>(thread),
      accelerometer_(this, device::mojom::SensorType::ACCELEROMETER),
      linear_acceleration_sensor_(
          this,
          device::mojom::SensorType::LINEAR_ACCELERATION),
      gyroscope_(this, device::mojom::SensorType::GYROSCOPE),
      state_(PumpState::STOPPED) {}

DeviceMotionEventPump::~DeviceMotionEventPump() {
  PlatformEventObserver<blink::WebDeviceMotionListener>::StopIfObserving();
}

void DeviceMotionEventPump::Start(blink::WebPlatformEventListener* listener) {
  DVLOG(2) << "requested start";

  if (state_ != PumpState::STOPPED)
    return;

  DCHECK(!timer_.IsRunning());

  state_ = PumpState::PENDING_START;
  PlatformEventObserver<blink::WebDeviceMotionListener>::Start(listener);
}

void DeviceMotionEventPump::Stop() {
  DVLOG(2) << "requested stop";

  if (state_ == PumpState::STOPPED)
    return;

  DCHECK((state_ == PumpState::PENDING_START && !timer_.IsRunning()) ||
         (state_ == PumpState::RUNNING && timer_.IsRunning()));

  if (timer_.IsRunning())
    timer_.Stop();

  PlatformEventObserver<blink::WebDeviceMotionListener>::Stop();
  state_ = PumpState::STOPPED;
}

void DeviceMotionEventPump::SendStartMessage() {
  // When running layout tests, those observers should not listen to the
  // actual hardware changes. In order to make that happen, don't connect
  // the other end of the mojo pipe to anything.
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
          base::BindOnce(&DeviceMotionEventPump::HandleSensorProviderError,
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

DeviceMotionEventPump::SensorEntry::SensorEntry(
    DeviceMotionEventPump* pump,
    device::mojom::SensorType sensor_type)
    : event_pump(pump), type(sensor_type), client_binding(this) {}

DeviceMotionEventPump::SensorEntry::~SensorEntry() {}

void DeviceMotionEventPump::SensorEntry::RaiseError() {
  HandleSensorError();
}

void DeviceMotionEventPump::SensorEntry::SensorReadingChanged() {
  // Since DeviceMotionEventPump::FireEvent is called in a fixed
  // frequency, the |shared_buffer| is read frequently, and
  // Sensor::ConfigureReadingChangeNotifications() is set to false,
  // so this method is not called and doesn't need to be implemented.
  NOTREACHED();
}

void DeviceMotionEventPump::SensorEntry::OnSensorCreated(
    device::mojom::SensorInitParamsPtr params,
    device::mojom::SensorClientRequest client_request) {
  if (!params) {
    HandleSensorError();
    event_pump->DidStartIfPossible();
    return;
  }

  constexpr size_t kReadBufferSize = sizeof(device::SensorReadingSharedBuffer);

  DCHECK_EQ(0u, params->buffer_offset % kReadBufferSize);

  mode = params->mode;
  default_config = params->default_configuration;

  DCHECK(sensor.is_bound());
  client_binding.Bind(std::move(client_request));

  shared_buffer_handle = std::move(params->memory);
  DCHECK(!shared_buffer);
  shared_buffer =
      shared_buffer_handle->MapAtOffset(kReadBufferSize, params->buffer_offset);

  if (!shared_buffer) {
    HandleSensorError();
    event_pump->DidStartIfPossible();
    return;
  }

  const device::SensorReadingSharedBuffer* buffer =
      static_cast<const device::SensorReadingSharedBuffer*>(
          shared_buffer.get());
  shared_buffer_reader.reset(
      new device::SensorReadingSharedBufferReader(buffer));

  DCHECK_GT(params->minimum_frequency, 0.0);
  DCHECK_GE(params->maximum_frequency, params->minimum_frequency);
  DCHECK_GE(device::GetSensorMaxAllowedFrequency(type),
            params->maximum_frequency);

  default_config.set_frequency(kDefaultPumpFrequencyHz);

  sensor->ConfigureReadingChangeNotifications(false /* disabled */);
  sensor->AddConfiguration(
      default_config, base::BindOnce(&SensorEntry::OnSensorAddConfiguration,
                                     base::Unretained(this)));
}

void DeviceMotionEventPump::SensorEntry::OnSensorAddConfiguration(
    bool success) {
  if (!success)
    HandleSensorError();
  event_pump->DidStartIfPossible();
}

void DeviceMotionEventPump::SensorEntry::HandleSensorError() {
  sensor.reset();
  shared_buffer_handle.reset();
  shared_buffer.reset();
  client_binding.Close();
}

bool DeviceMotionEventPump::SensorEntry::SensorReadingCouldBeRead() {
  if (!sensor)
    return false;

  DCHECK(shared_buffer);

  if (!shared_buffer_handle->is_valid() ||
      !shared_buffer_reader->GetReading(&reading)) {
    HandleSensorError();
    return false;
  }

  return true;
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

void DeviceMotionEventPump::DidStartIfPossible() {
  DVLOG(2) << "did start sensor event pump";

  if (state_ != PumpState::PENDING_START)
    return;

  // After the DeviceMotionEventPump::SendStartMessage() is called and before
  // the DeviceMotionEventPump::SensorEntry::OnSensorCreated() callback has
  // been executed, it is possible that the |sensor| is already initialized
  // but its |shared_buffer| is not initialized yet. And in that case when
  // DeviceMotionEventPump::SendStartMessage() is called again,
  // SensorSharedBuffersReady() is used to make sure that the
  // DeviceMotionEventPump can not be started when |shared_buffer| is not
  // initialized.
  if (!SensorSharedBuffersReady())
    return;

  DCHECK(!timer_.IsRunning());

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMicroseconds(kDefaultPumpDelayMicroseconds),
               this, &DeviceMotionEventPump::FireEvent);
  state_ = PumpState::RUNNING;
}

RenderFrame* DeviceMotionEventPump::GetRenderFrame() const {
  blink::WebLocalFrame* const web_frame =
      blink::WebLocalFrame::FrameForCurrentContext();

  return RenderFrame::FromWebFrame(web_frame);
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
    data->has_acceleration_including_gravity_x = true;
    data->has_acceleration_including_gravity_y = true;
    data->has_acceleration_including_gravity_z = true;
  }

  if (linear_acceleration_sensor_.SensorReadingCouldBeRead()) {
    data->acceleration_x = linear_acceleration_sensor_.reading.accel.x;
    data->acceleration_y = linear_acceleration_sensor_.reading.accel.y;
    data->acceleration_z = linear_acceleration_sensor_.reading.accel.z;
    data->has_acceleration_x = true;
    data->has_acceleration_y = true;
    data->has_acceleration_z = true;
  }

  if (gyroscope_.SensorReadingCouldBeRead()) {
    data->rotation_rate_alpha = gyroscope_.reading.gyro.x;
    data->rotation_rate_beta = gyroscope_.reading.gyro.y;
    data->rotation_rate_gamma = gyroscope_.reading.gyro.z;
    data->has_rotation_rate_alpha = true;
    data->has_rotation_rate_beta = true;
    data->has_rotation_rate_gamma = true;
  }
}

void DeviceMotionEventPump::GetSensor(SensorEntry* sensor_entry) {
  auto request = mojo::MakeRequest(&sensor_entry->sensor);
  sensor_provider_->GetSensor(sensor_entry->type, std::move(request),
                              base::BindOnce(&SensorEntry::OnSensorCreated,
                                             base::Unretained(sensor_entry)));
  sensor_entry->sensor.set_connection_error_handler(base::BindOnce(
      &SensorEntry::HandleSensorError, base::Unretained(sensor_entry)));
}

void DeviceMotionEventPump::HandleSensorProviderError() {
  sensor_provider_.reset();
}

}  // namespace content
