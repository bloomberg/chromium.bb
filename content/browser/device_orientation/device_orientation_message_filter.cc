// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/device_orientation_message_filter.h"

#include "content/browser/device_orientation/device_motion_service.h"
#include "content/common/device_orientation/device_orientation_messages.h"

namespace content {

DeviceOrientationMessageFilter::DeviceOrientationMessageFilter()
    : is_started_(false) {
}

DeviceOrientationMessageFilter::~DeviceOrientationMessageFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (is_started_) {
    // TODO(timvolodine): insert a proper call to DeviceSensorService here,
    // similar to DeviceMotionService::GetInstance()->RemoveConsumer();
  }
}

bool DeviceOrientationMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(DeviceOrientationMessageFilter,
                           message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(DeviceOrientationHostMsg_StartPolling,
                        OnDeviceOrientationStartPolling)
    IPC_MESSAGE_HANDLER(DeviceOrientationHostMsg_StopPolling,
                        OnDeviceOrientationStopPolling)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void DeviceOrientationMessageFilter::OnDeviceOrientationStartPolling() {
  NOTIMPLEMENTED();
  DCHECK(!is_started_);
  if (is_started_)
    return;
  is_started_ = true;
  // TODO(timvolodine): insert a proper call to DeviceSensorService here,
  // similar to DeviceMotionService::GetInstance()->AddConsumer();
  DidStartDeviceOrientationPolling();
}

void DeviceOrientationMessageFilter::OnDeviceOrientationStopPolling() {
  NOTIMPLEMENTED();
  DCHECK(is_started_);
  if (!is_started_)
    return;
  is_started_ = false;
  // TODO(timvolodine): insert a proper call to DeviceSensorService here,
  // similar to DeviceMotionService::GetInstance()->RemoveConsumer();
}

void DeviceOrientationMessageFilter::DidStartDeviceOrientationPolling() {
  NOTIMPLEMENTED();
  // TODO(timvolodine): insert a proper call to the generalized Service here,
  // similar to
  // Send(new DeviceOrientationMsg_DidStartPolling(
  //     DeviceMotionService::GetInstance()->GetSharedMemoryHandleForProcess(
  //        PeerHandle())));
}

}  // namespace content
