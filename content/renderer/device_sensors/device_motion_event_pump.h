// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_MOTION_EVENT_PUMP_H_
#define CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_MOTION_EVENT_PUMP_H_

#include "base/macros.h"
#include "content/renderer/device_sensors/device_sensor_event_pump.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceMotionListener.h"

namespace device {
class MotionData;
}

namespace content {

class RenderThread;

class CONTENT_EXPORT DeviceMotionEventPump
    : public DeviceSensorEventPump<blink::WebDeviceMotionListener> {
 public:
  explicit DeviceMotionEventPump(RenderThread* thread);
  ~DeviceMotionEventPump() override;

  // PlatformEventObserver:
  void SendStartMessage() override;
  void SendStopMessage() override;
  void SendFakeDataForTesting(void* fake_data) override;

 protected:
  // DeviceSensorEventPump:
  void FireEvent() override;

  SensorEntry accelerometer_;
  SensorEntry linear_acceleration_sensor_;
  SensorEntry gyroscope_;

 private:
  FRIEND_TEST_ALL_PREFIXES(DeviceMotionEventPumpTest,
                           SensorInitializedButItsSharedBufferIsNot);

  // DeviceSensorEventPump:
  bool SensorSharedBuffersReady() const override;

  void GetDataFromSharedMemory(device::MotionData* data);

  DISALLOW_COPY_AND_ASSIGN(DeviceMotionEventPump);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_MOTION_EVENT_PUMP_H_
