// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_SENSOR_EVENT_PUMP_H_
#define CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_SENSOR_EVENT_PUMP_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/platform_event_observer.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/device/public/interfaces/sensor_provider.mojom.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

template <typename ListenerType>
class CONTENT_EXPORT DeviceSensorEventPump
    : public PlatformEventObserver<ListenerType> {
 public:
  // Default rate for firing events.
  static constexpr int kDefaultPumpFrequencyHz = 60;
  static constexpr int kDefaultPumpDelayMicroseconds =
      base::Time::kMicrosecondsPerSecond / kDefaultPumpFrequencyHz;

  // PlatformEventObserver:
  void Start(blink::WebPlatformEventListener* listener) override {
    DVLOG(2) << "requested start";

    if (state_ != PumpState::STOPPED)
      return;

    DCHECK(!timer_.IsRunning());

    state_ = PumpState::PENDING_START;
    PlatformEventObserver<ListenerType>::Start(listener);
  }

  // PlatformEventObserver:
  void Stop() override {
    DVLOG(2) << "requested stop";

    if (state_ == PumpState::STOPPED)
      return;

    DCHECK((state_ == PumpState::PENDING_START && !timer_.IsRunning()) ||
           (state_ == PumpState::RUNNING && timer_.IsRunning()));

    if (timer_.IsRunning())
      timer_.Stop();

    PlatformEventObserver<ListenerType>::Stop();
    state_ = PumpState::STOPPED;
  }

  void HandleSensorProviderError() { sensor_provider_.reset(); }

 protected:
  explicit DeviceSensorEventPump(RenderThread* thread)
      : PlatformEventObserver<ListenerType>(thread),
        state_(PumpState::STOPPED) {}

  ~DeviceSensorEventPump() override {
    PlatformEventObserver<ListenerType>::StopIfObserving();
  }

  virtual void FireEvent() = 0;

  struct SensorEntry : public device::mojom::SensorClient {
    SensorEntry(DeviceSensorEventPump* pump,
                device::mojom::SensorType sensor_type)
        : event_pump(pump), type(sensor_type), client_binding(this) {}

    ~SensorEntry() override {}

    // device::mojom::SensorClient:
    void RaiseError() override { HandleSensorError(); }

    // device::mojom::SensorClient:
    void SensorReadingChanged() override {
      // Since DeviceSensorEventPump::FireEvent is called in a fixed
      // frequency, the |shared_buffer| is read frequently, and
      // Sensor::ConfigureReadingChangeNotifications() is set to false,
      // so this method is not called and doesn't need to be implemented.
      NOTREACHED();
    }

    // Mojo callback for SensorProvider::GetSensor().
    void OnSensorCreated(device::mojom::SensorInitParamsPtr params,
                         device::mojom::SensorClientRequest client_request) {
      if (!params) {
        HandleSensorError();
        event_pump->DidStartIfPossible();
        return;
      }

      constexpr size_t kReadBufferSize =
          sizeof(device::SensorReadingSharedBuffer);

      DCHECK_EQ(0u, params->buffer_offset % kReadBufferSize);

      mode = params->mode;
      default_config = params->default_configuration;

      DCHECK(sensor.is_bound());
      client_binding.Bind(std::move(client_request));

      shared_buffer_handle = std::move(params->memory);
      DCHECK(!shared_buffer);
      shared_buffer = shared_buffer_handle->MapAtOffset(kReadBufferSize,
                                                        params->buffer_offset);
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

      default_config.set_frequency(kDefaultPumpFrequencyHz);

      sensor->ConfigureReadingChangeNotifications(false /* disabled */);
      sensor->AddConfiguration(
          default_config, base::Bind(&SensorEntry::OnSensorAddConfiguration,
                                     base::Unretained(this)));
    }

    // Mojo callback for Sensor::AddConfiguration().
    void OnSensorAddConfiguration(bool success) {
      if (!success)
        HandleSensorError();
      event_pump->DidStartIfPossible();
    }

    void HandleSensorError() {
      sensor.reset();
      shared_buffer_handle.reset();
      shared_buffer.reset();
      client_binding.Close();
    }

    bool SensorReadingCouldBeRead() {
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

    DeviceSensorEventPump* event_pump;
    device::mojom::SensorPtr sensor;
    device::mojom::SensorType type;
    device::mojom::ReportingMode mode;
    device::PlatformSensorConfiguration default_config;
    mojo::ScopedSharedBufferHandle shared_buffer_handle;
    mojo::ScopedSharedBufferMapping shared_buffer;
    std::unique_ptr<device::SensorReadingSharedBufferReader>
        shared_buffer_reader;
    device::SensorReading reading;
    mojo::Binding<device::mojom::SensorClient> client_binding;
  };

  friend struct SensorEntry;

  void GetSensor(SensorEntry* sensor_entry) {
    auto request = mojo::MakeRequest(&sensor_entry->sensor);
    sensor_provider_->GetSensor(sensor_entry->type, std::move(request),
                                base::Bind(&SensorEntry::OnSensorCreated,
                                           base::Unretained(sensor_entry)));
    sensor_entry->sensor.set_connection_error_handler(base::Bind(
        &SensorEntry::HandleSensorError, base::Unretained(sensor_entry)));
  }

  virtual void DidStartIfPossible() {
    DVLOG(2) << "did start sensor event pump";

    if (state_ != PumpState::PENDING_START)
      return;

    // After the DeviceSensorEventPump::SendStartMessage() is called and before
    // the DeviceSensorEventPump::SensorEntry::OnSensorCreated() callback has
    // been executed, it is possible that the |sensor| is already initialized
    // but its |shared_buffer| is not initialized yet. And in that case when
    // DeviceSensorEventPump::SendStartMessage() is called again,
    // SensorSharedBuffersReady() is used to make sure that the
    // DeviceSensorEventPump can not be started when |shared_buffer| is not
    // initialized.
    if (!SensorSharedBuffersReady())
      return;

    DCHECK(!timer_.IsRunning());

    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMicroseconds(kDefaultPumpDelayMicroseconds), this,
        &DeviceSensorEventPump::FireEvent);
    state_ = PumpState::RUNNING;
  }

  static RenderFrame* GetRenderFrame() {
    blink::WebLocalFrame* const web_frame =
        blink::WebLocalFrame::FrameForCurrentContext();

    return RenderFrame::FromWebFrame(web_frame);
  }

  device::mojom::SensorProviderPtr sensor_provider_;

 private:
  // The pump is a tri-state automaton with allowed transitions as follows:
  // STOPPED -> PENDING_START
  // PENDING_START -> RUNNING
  // PENDING_START -> STOPPED
  // RUNNING -> STOPPED
  enum class PumpState { STOPPED, RUNNING, PENDING_START };

  virtual bool SensorSharedBuffersReady() const = 0;

  PumpState state_;
  base::RepeatingTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSensorEventPump);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_SENSOR_EVENT_PUMP_H_
