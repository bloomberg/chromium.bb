// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/device_sensors/device_light_event_pump.h"

#include "content/common/device_sensors/device_light_messages.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/WebDeviceLightListener.h"

namespace {
// Default delay between subsequent firing of DeviceLight events.
const int kDefaultLightPumpDelayMillis = 200;
}  // namespace

namespace content {

DeviceLightEventPump::DeviceLightEventPump()
    : DeviceSensorEventPump(kDefaultLightPumpDelayMillis),
      listener_(NULL),
      last_seen_data_(-1) {
}

DeviceLightEventPump::DeviceLightEventPump(int pump_delay_millis)
    : DeviceSensorEventPump(pump_delay_millis),
      listener_(NULL),
      last_seen_data_(-1) {
}

DeviceLightEventPump::~DeviceLightEventPump() {
}

bool DeviceLightEventPump::SetListener(
    blink::WebDeviceLightListener* listener) {
  listener_ = listener;
  return listener_ ? RequestStart() : Stop();
}

bool DeviceLightEventPump::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceLightEventPump, message)
  IPC_MESSAGE_HANDLER(DeviceLightMsg_DidStartPolling, OnDidStart)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceLightEventPump::FireEvent() {
  DCHECK(listener_);
  DeviceLightData data;
  bool did_return_light_data = reader_->GetLatestData(&data);
  if (did_return_light_data && data.value != last_seen_data_) {
    last_seen_data_ = data.value;
    listener_->didChangeDeviceLight(data.value);
  }
}

bool DeviceLightEventPump::InitializeReader(base::SharedMemoryHandle handle) {
  if (!reader_)
    reader_.reset(new DeviceLightSharedMemoryReader());
  return reader_->Initialize(handle);
}

bool DeviceLightEventPump::SendStartMessage() {
  return RenderThread::Get()->Send(new DeviceLightHostMsg_StartPolling());
}

bool DeviceLightEventPump::SendStopMessage() {
  last_seen_data_ = -1;
  return RenderThread::Get()->Send(new DeviceLightHostMsg_StopPolling());
}

}  // namespace content
