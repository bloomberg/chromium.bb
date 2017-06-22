// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_motion_event_pump.h"

#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceMotionListener.h"

namespace content {

DeviceMotionEventPump::DeviceMotionEventPump(RenderThread* thread)
    : DeviceSensorMojoClientMixin<
          DeviceSensorEventPump<blink::WebDeviceMotionListener>,
          device::mojom::MotionSensor>(thread) {}

DeviceMotionEventPump::~DeviceMotionEventPump() {
}

void DeviceMotionEventPump::FireEvent() {
  DCHECK(listener());
  device::MotionData data;
  if (reader_->GetLatestData(&data) && data.all_available_sensors_are_active)
    listener()->DidChangeDeviceMotion(data);
}

bool DeviceMotionEventPump::InitializeReader(base::SharedMemoryHandle handle) {
  if (!reader_)
    reader_.reset(new DeviceMotionSharedMemoryReader());
  return reader_->Initialize(handle);
}

void DeviceMotionEventPump::SendFakeDataForTesting(void* fake_data) {
  device::MotionData data = *static_cast<device::MotionData*>(fake_data);

  listener()->DidChangeDeviceMotion(data);
}

}  // namespace content
