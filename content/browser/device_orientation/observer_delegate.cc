// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/observer_delegate.h"

#include "base/logging.h"
#include "content/browser/device_orientation/device_data.h"
#include "content/browser/device_orientation/motion.h"
#include "content/browser/device_orientation/orientation.h"
#include "ipc/ipc_sender.h"

namespace content {

ObserverDelegate::ObserverDelegate(DeviceData::Type device_data_type,
                                   Provider* provider, int render_view_id,
                                   IPC::Sender* sender)
    : Observer(device_data_type),
      provider_(provider),
      render_view_id_(render_view_id),
      sender_(sender) {
  provider_->AddObserver(this);
}

ObserverDelegate::~ObserverDelegate() {
  provider_->RemoveObserver(this);
}

void ObserverDelegate::OnDeviceDataUpdate(
    const DeviceData* device_data, DeviceData::Type device_data_type) {
  scoped_refptr<const DeviceData> new_device_data(device_data);
  if (!new_device_data)
    new_device_data = EmptyDeviceData(device_data_type);

  sender_->Send(new_device_data->CreateIPCMessage(render_view_id_));
}

DeviceData* ObserverDelegate::EmptyDeviceData(DeviceData::Type type) {
  switch (type) {
    case DeviceData::kTypeMotion:
      return new Motion();
    case DeviceData::kTypeOrientation:
      return new Orientation();
    case DeviceData::kTypeTest:
      NOTREACHED();
  }
  NOTREACHED();
  return NULL;
}

}  // namespace content
