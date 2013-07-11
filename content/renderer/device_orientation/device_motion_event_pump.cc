// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_motion_event_pump.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "content/common/device_motion_messages.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/WebDeviceMotionListener.h"

namespace content {

const double DeviceMotionEventPump::kPumpDelayMillis = 40;

double DeviceMotionEventPump::GetDelayMillis() {
  return kPumpDelayMillis;
}

DeviceMotionEventPump::DeviceMotionEventPump()
    : listener_(0), state_(STOPPED) {
}

DeviceMotionEventPump::~DeviceMotionEventPump() {
}

bool DeviceMotionEventPump::SetListener(
    WebKit::WebDeviceMotionListener* listener) {
  listener_ = listener;
  if (listener_)
    return StartFetchingDeviceMotion();
  return StopFetchingDeviceMotion();
}

void DeviceMotionEventPump::SetDeviceMotionReader(
    scoped_ptr<DeviceMotionSharedMemoryReader> reader) {
  reader_.reset(reader.release());
}

bool DeviceMotionEventPump::StartFetchingDeviceMotion() {
  DVLOG(2) << "start fetching device motion";

  if (state_ != STOPPED)
    return false;

  DCHECK(!timer_.IsRunning());

  if (RenderThread::Get()->Send(new DeviceMotionHostMsg_StartPolling())) {
    state_ = PENDING_START;
    return true;
  }
  return false;
}

bool DeviceMotionEventPump::StopFetchingDeviceMotion() {
  DVLOG(2) << "stop fetching device motion";

  if (state_ == STOPPED)
    return true;

  DCHECK((state_ == PENDING_START && !timer_.IsRunning()) ||
      (state_ == RUNNING && timer_.IsRunning()));

  if (timer_.IsRunning())
    timer_.Stop();
  RenderThread::Get()->Send(new DeviceMotionHostMsg_StopPolling());
  state_ = STOPPED;
  return true;
}

void DeviceMotionEventPump::FireEvent() {
  DCHECK(listener_);
  WebKit::WebDeviceMotionData data;
  if (reader_->GetLatestData(&data) && data.allAvailableSensorsAreActive)
    listener_->didChangeDeviceMotion(data);
}

void DeviceMotionEventPump::Attach(RenderThread* thread) {
  if (!thread)
    return;
  thread->AddObserver(this);
}

void DeviceMotionEventPump::OnDidStartDeviceMotion(
     base::SharedMemoryHandle renderer_handle) {
  DVLOG(2) << "did start fetching device motion";

  if (state_ != PENDING_START)
    return;

  DCHECK(!timer_.IsRunning());
  if (!reader_)
    reader_.reset(new DeviceMotionSharedMemoryReader());

  if (reader_->Initialize(renderer_handle)) {
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kPumpDelayMillis),
                 this, &DeviceMotionEventPump::FireEvent);
    state_ = RUNNING;
  }
}

bool DeviceMotionEventPump::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceMotionEventPump, message)
    IPC_MESSAGE_HANDLER(DeviceMotionMsg_DidStartPolling,
        OnDidStartDeviceMotion)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace content
