// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(youngki): Implement this file.

#include "device/bluetooth/bluetooth_socket_win.h"

#include <string>

#include "base/logging.h"
#include "net/base/io_buffer.h"

namespace device {

BluetoothSocketWin::BluetoothSocketWin() {
}

BluetoothSocketWin::~BluetoothSocketWin() {
}

int BluetoothSocketWin::fd() const {
  NOTIMPLEMENTED();
  return -1;
}

bool BluetoothSocketWin::Receive(net::GrowableIOBuffer* buffer) {
  return false;
}

bool BluetoothSocketWin::Send(net::DrainableIOBuffer* buffer) {
  return false;
}

std::string BluetoothSocketWin::GetLastErrorMessage() const {
  return "";
}

}  // namespace device
