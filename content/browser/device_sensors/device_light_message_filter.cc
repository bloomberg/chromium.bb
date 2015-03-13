// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/device_light_message_filter.h"

#include "content/browser/device_sensors/device_inertial_sensor_service.h"
#include "content/common/device_sensors/device_light_messages.h"

namespace content {

DeviceLightMessageFilter::DeviceLightMessageFilter()
    : BrowserMessageFilter(DeviceLightMsgStart), is_started_(false) {
}

DeviceLightMessageFilter::~DeviceLightMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (is_started_) {
    DeviceInertialSensorService::GetInstance()->RemoveConsumer(
        CONSUMER_TYPE_LIGHT);
  }
}

bool DeviceLightMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DeviceLightMessageFilter, message)
  IPC_MESSAGE_HANDLER(DeviceLightHostMsg_StartPolling,
                      OnDeviceLightStartPolling)
  IPC_MESSAGE_HANDLER(DeviceLightHostMsg_StopPolling, OnDeviceLightStopPolling)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DeviceLightMessageFilter::OnDeviceLightStartPolling() {
  DCHECK(!is_started_);
  if (is_started_)
    return;
  is_started_ = true;
  DeviceInertialSensorService::GetInstance()->AddConsumer(CONSUMER_TYPE_LIGHT);
  DidStartDeviceLightPolling();
}

void DeviceLightMessageFilter::OnDeviceLightStopPolling() {
  DCHECK(is_started_);
  if (!is_started_)
    return;
  is_started_ = false;
  DeviceInertialSensorService::GetInstance()->RemoveConsumer(
      CONSUMER_TYPE_LIGHT);
}

void DeviceLightMessageFilter::DidStartDeviceLightPolling() {
  Send(new DeviceLightMsg_DidStartPolling(
      DeviceInertialSensorService::GetInstance()
          ->GetSharedMemoryHandleForProcess(CONSUMER_TYPE_LIGHT,
                                            PeerHandle())));
}

}  // namespace content
