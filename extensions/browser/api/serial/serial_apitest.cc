// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/extension_apitest.h"
#include "device/serial/test_serial_io_handler.h"
#include "extensions/browser/api/serial/serial_api.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/serial.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace {

class SerialApiTest : public ExtensionApiTest {
 public:
  SerialApiTest() {}
};

}  // namespace

namespace extensions {

class FakeSerialGetDevicesFunction : public AsyncExtensionFunction {
 public:
  virtual bool RunAsync() OVERRIDE {
    base::ListValue* devices = new base::ListValue();
    base::DictionaryValue* device0 = new base::DictionaryValue();
    device0->SetString("path", "/dev/fakeserial");
    base::DictionaryValue* device1 = new base::DictionaryValue();
    device1->SetString("path", "\\\\COM800\\");
    devices->Append(device0);
    devices->Append(device1);
    SetResult(devices);
    SendResponse(true);
    return true;
  }

 protected:
  virtual ~FakeSerialGetDevicesFunction() {}
};

class FakeEchoSerialIoHandler : public device::TestSerialIoHandler {
 public:
  explicit FakeEchoSerialIoHandler() {
    device_control_signals()->dcd = true;
    device_control_signals()->cts = true;
    device_control_signals()->ri = true;
    device_control_signals()->dsr = true;
  }

  MOCK_METHOD1(SetControlSignals,
               bool(const device::serial::HostControlSignals&));

 protected:
  virtual ~FakeEchoSerialIoHandler() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeEchoSerialIoHandler);
};

class FakeSerialConnectFunction : public core_api::SerialConnectFunction {
 protected:
  virtual SerialConnection* CreateSerialConnection(
      const std::string& port,
      const std::string& owner_extension_id) const OVERRIDE {
    scoped_refptr<FakeEchoSerialIoHandler> io_handler =
        new FakeEchoSerialIoHandler;
    EXPECT_CALL(*io_handler.get(), SetControlSignals(_)).Times(1).WillOnce(
        Return(true));
    SerialConnection* serial_connection =
        new SerialConnection(port, owner_extension_id);
    serial_connection->SetIoHandlerForTest(io_handler);
    return serial_connection;
  }

 protected:
  virtual ~FakeSerialConnectFunction() {}
};

}  // namespace extensions

ExtensionFunction* FakeSerialGetDevicesFunctionFactory() {
  return new extensions::FakeSerialGetDevicesFunction();
}

ExtensionFunction* FakeSerialConnectFunctionFactory() {
  return new extensions::FakeSerialConnectFunction();
}

// Disable SIMULATE_SERIAL_PORTS only if all the following are true:
//
// 1. You have an Arduino or compatible board attached to your machine and
// properly appearing as the first virtual serial port ("first" is very loosely
// defined as whichever port shows up in serial.getPorts). We've tested only
// the Atmega32u4 Breakout Board and Arduino Leonardo; note that both these
// boards are based on the Atmel ATmega32u4, rather than the more common
// Arduino '328p with either FTDI or '8/16u2 USB interfaces. TODO: test more
// widely.
//
// 2. Your user has permission to read/write the port. For example, this might
// mean that your user is in the "tty" or "uucp" group on Ubuntu flavors of
// Linux, or else that the port's path (e.g., /dev/ttyACM0) has global
// read/write permissions.
//
// 3. You have uploaded a program to the board that does a byte-for-byte echo
// on the virtual serial port at 57600 bps. An example is at
// chrome/test/data/extensions/api_test/serial/api/serial_arduino_test.ino.
//
#define SIMULATE_SERIAL_PORTS (1)
IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialFakeHardware) {
  extensions::ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

#if SIMULATE_SERIAL_PORTS
  ASSERT_TRUE(extensions::ExtensionFunctionDispatcher::OverrideFunction(
      "serial.getDevices", FakeSerialGetDevicesFunctionFactory));
  ASSERT_TRUE(extensions::ExtensionFunctionDispatcher::OverrideFunction(
      "serial.connect", FakeSerialConnectFunctionFactory));
#endif

  ASSERT_TRUE(RunExtensionTest("serial/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialRealHardware) {
  extensions::ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ASSERT_TRUE(RunExtensionTest("serial/real_hardware")) << message_;
}
