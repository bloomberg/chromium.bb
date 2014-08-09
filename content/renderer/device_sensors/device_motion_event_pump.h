// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_MOTION_EVENT_PUMP_H_
#define CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_MOTION_EVENT_PUMP_H_

#include "base/memory/scoped_ptr.h"
#include "content/renderer/device_sensors/device_sensor_event_pump.h"
#include "content/renderer/shared_memory_seqlock_reader.h"
#include "third_party/WebKit/public/platform/WebDeviceMotionData.h"

namespace blink {
class WebDeviceMotionListener;
}

namespace content {

typedef SharedMemorySeqLockReader<blink::WebDeviceMotionData>
    DeviceMotionSharedMemoryReader;

class CONTENT_EXPORT DeviceMotionEventPump
    : public DeviceSensorEventPump<blink::WebDeviceMotionListener> {
 public:
  explicit DeviceMotionEventPump(RenderThread* thread);
  virtual ~DeviceMotionEventPump();

  // // PlatformEventObserver.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void SendFakeDataForTesting(void* fake_data) OVERRIDE;

 protected:
  virtual void FireEvent() OVERRIDE;
  virtual bool InitializeReader(base::SharedMemoryHandle handle) OVERRIDE;

  // PlatformEventObserver.
  virtual void SendStartMessage() OVERRIDE;
  virtual void SendStopMessage() OVERRIDE;

  scoped_ptr<DeviceMotionSharedMemoryReader> reader_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMotionEventPump);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_MOTION_EVENT_PUMP_H_
