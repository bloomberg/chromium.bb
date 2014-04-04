// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_profile_win.h"

#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_device_win.h"
#include "device/bluetooth/bluetooth_service_record.h"
#include "device/bluetooth/bluetooth_socket_win.h"

namespace device {

BluetoothProfileWin::BluetoothProfileWin(const BluetoothUUID& uuid,
                                         const std::string& name)
    : BluetoothProfile(), uuid_(uuid), name_(name) {
}

BluetoothProfileWin::~BluetoothProfileWin() {
}

void BluetoothProfileWin::Unregister() {
  delete this;
}

void BluetoothProfileWin::SetConnectionCallback(
    const ConnectionCallback& callback) {
  connection_callback_ = callback;
}

bool BluetoothProfileWin::Connect(const BluetoothDeviceWin* device) {
  if (connection_callback_.is_null())
    return false;

  const BluetoothServiceRecord* record = device->GetServiceRecord(uuid_);
  if (record) {
    scoped_refptr<BluetoothSocket> socket(
        BluetoothSocketWin::CreateBluetoothSocket(*record));
    if (socket.get() != NULL) {
      connection_callback_.Run(device, socket);
      return true;
    }
  }
  return false;
}

}  // namespace device
