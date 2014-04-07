// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_SOCKET_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_SOCKET_H_

#include <string>

#include "device/bluetooth/bluetooth_socket.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

class DrainableIOBuffer;
class GrowableIOBuffer;

}  // namespace net

namespace device {

class MockBluetoothSocket : public BluetoothSocket {
 public:
  MockBluetoothSocket();
  MOCK_METHOD1(Receive, bool(net::GrowableIOBuffer*));
  MOCK_METHOD1(Send, bool(net::DrainableIOBuffer*));
  MOCK_CONST_METHOD0(GetLastErrorMessage, std::string());

 protected:
  virtual ~MockBluetoothSocket();
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_SOCKET_H_
