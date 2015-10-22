// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/device_orientation_absolute_message_filter.h"

#include "content/browser/device_sensors/device_inertial_sensor_service.h"
#include "content/common/device_sensors/device_orientation_messages.h"

namespace content {

DeviceOrientationAbsoluteMessageFilter::DeviceOrientationAbsoluteMessageFilter()
    : BrowserMessageFilter(DeviceOrientationMsgStart),
      is_started_(false) {
}

DeviceOrientationAbsoluteMessageFilter::
    ~DeviceOrientationAbsoluteMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (is_started_) {
    DeviceInertialSensorService::GetInstance()->RemoveConsumer(
        CONSUMER_TYPE_ORIENTATION_ABSOLUTE);
  }
}

bool DeviceOrientationAbsoluteMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceOrientationAbsoluteMessageFilter, message)
    IPC_MESSAGE_HANDLER(DeviceOrientationAbsoluteHostMsg_StartPolling,
                        OnStartPolling)
    IPC_MESSAGE_HANDLER(DeviceOrientationAbsoluteHostMsg_StopPolling,
                        OnStopPolling)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceOrientationAbsoluteMessageFilter::OnStartPolling() {
  DCHECK(!is_started_);
  if (is_started_)
    return;
  is_started_ = true;
  DeviceInertialSensorService::GetInstance()->AddConsumer(
      CONSUMER_TYPE_ORIENTATION_ABSOLUTE);
  DidStartPolling();
}

void DeviceOrientationAbsoluteMessageFilter::OnStopPolling() {
  DCHECK(is_started_);
  if (!is_started_)
    return;
  is_started_ = false;
  DeviceInertialSensorService::GetInstance()->RemoveConsumer(
      CONSUMER_TYPE_ORIENTATION_ABSOLUTE);
}

void DeviceOrientationAbsoluteMessageFilter::DidStartPolling() {
  Send(new DeviceOrientationAbsoluteMsg_DidStartPolling(
      DeviceInertialSensorService::GetInstance()->
          GetSharedMemoryHandleForProcess(
              CONSUMER_TYPE_ORIENTATION_ABSOLUTE,
              PeerHandle())));
}

}  // namespace content
