// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(youngki): Implement this file.

#include "device/bluetooth/bluetooth_socket_win.h"

#include "base/logging.h"

namespace device {

BluetoothSocketWin::BluetoothSocketWin() {
}

BluetoothSocketWin::~BluetoothSocketWin() {
}

int BluetoothSocketWin::fd() const {
  NOTIMPLEMENTED();
  return -1;
}

}  // namespace device
