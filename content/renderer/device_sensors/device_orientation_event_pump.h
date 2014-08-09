// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_ORIENTATION_EVENT_PUMP_H_
#define CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_ORIENTATION_EVENT_PUMP_H_

#include "base/memory/scoped_ptr.h"
#include "content/renderer/device_sensors/device_sensor_event_pump.h"
#include "content/renderer/shared_memory_seqlock_reader.h"
#include "third_party/WebKit/public/platform/WebDeviceOrientationData.h"

namespace blink {
class WebDeviceOrientationListener;
}

namespace content {

typedef SharedMemorySeqLockReader<blink::WebDeviceOrientationData>
    DeviceOrientationSharedMemoryReader;

class CONTENT_EXPORT DeviceOrientationEventPump
    : public DeviceSensorEventPump<blink::WebDeviceOrientationListener> {
 public:
  // Angle threshold beyond which two orientation events are considered
  // sufficiently different.
  static const double kOrientationThreshold;

  explicit DeviceOrientationEventPump(RenderThread* thread);
  virtual ~DeviceOrientationEventPump();

  // PlatformEventObserver.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void SendFakeDataForTesting(void* data) OVERRIDE;

 protected:
  virtual void FireEvent() OVERRIDE;
  virtual bool InitializeReader(base::SharedMemoryHandle handle) OVERRIDE;

  // PlatformEventObserver.
  virtual void SendStartMessage() OVERRIDE;
  virtual void SendStopMessage() OVERRIDE;

  bool ShouldFireEvent(const blink::WebDeviceOrientationData& data) const;

  blink::WebDeviceOrientationData data_;
  scoped_ptr<DeviceOrientationSharedMemoryReader> reader_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationEventPump);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_ORIENTATION_EVENT_PUMP_H_
