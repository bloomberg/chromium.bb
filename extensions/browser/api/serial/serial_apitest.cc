// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/browser_thread.h"
#include "device/serial/serial_device_enumerator.h"
#include "device/serial/test_serial_io_handler.h"
#include "extensions/browser/api/serial/serial_api.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/common/api/serial.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace extensions {
namespace {

class FakeSerialGetDevicesFunction : public AsyncExtensionFunction {
 public:
  bool RunAsync() override {
    std::unique_ptr<base::ListValue> devices(new base::ListValue());
    std::unique_ptr<base::DictionaryValue> device0(new base::DictionaryValue());
    device0->SetString("path", "/dev/fakeserial");
    std::unique_ptr<base::DictionaryValue> device1(new base::DictionaryValue());
    device1->SetString("path", "\\\\COM800\\");
    devices->Append(std::move(device0));
    devices->Append(std::move(device1));
    SetResult(std::move(devices));
    SendResponse(true);
    return true;
  }

 protected:
  ~FakeSerialGetDevicesFunction() override {}
};

class FakeSerialDeviceEnumerator : public device::SerialDeviceEnumerator {
 public:
  ~FakeSerialDeviceEnumerator() override {}

  std::vector<device::mojom::SerialDeviceInfoPtr> GetDevices() override {
    std::vector<device::mojom::SerialDeviceInfoPtr> devices;
    device::mojom::SerialDeviceInfoPtr device0(
        device::mojom::SerialDeviceInfo::New());
    device0->path = "/dev/fakeserialmojo";
    device::mojom::SerialDeviceInfoPtr device1(
        device::mojom::SerialDeviceInfo::New());
    device1->path = "\\\\COM800\\";
    devices.push_back(std::move(device0));
    devices.push_back(std::move(device1));
    return devices;
  }
};

class FakeEchoSerialIoHandler : public device::TestSerialIoHandler {
 public:
  FakeEchoSerialIoHandler() {
    device_control_signals()->dcd = true;
    device_control_signals()->cts = true;
    device_control_signals()->ri = true;
    device_control_signals()->dsr = true;
    EXPECT_CALL(*this, SetControlSignals(_)).Times(1).WillOnce(Return(true));
  }

  static scoped_refptr<device::SerialIoHandler> Create() {
    return new FakeEchoSerialIoHandler();
  }

  MOCK_METHOD1(SetControlSignals,
               bool(const device::mojom::SerialHostControlSignals&));

 protected:
  ~FakeEchoSerialIoHandler() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeEchoSerialIoHandler);
};

class FakeSerialConnectFunction : public api::SerialConnectFunction {
 protected:
  SerialConnection* CreateSerialConnection(
      const std::string& port,
      const std::string& owner_extension_id) const override {
    scoped_refptr<FakeEchoSerialIoHandler> io_handler =
        new FakeEchoSerialIoHandler;
    SerialConnection* serial_connection =
        new SerialConnection(port, owner_extension_id);
    serial_connection->SetIoHandlerForTest(io_handler);
    return serial_connection;
  }

 protected:
  ~FakeSerialConnectFunction() override {}
};

class SerialApiTest : public ExtensionApiTest {
 public:
  SerialApiTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void TearDownOnMainThread() override {
    ExtensionApiTest::TearDownOnMainThread();
  }
};

ExtensionFunction* FakeSerialGetDevicesFunctionFactory() {
  return new FakeSerialGetDevicesFunction();
}

ExtensionFunction* FakeSerialConnectFunctionFactory() {
  return new FakeSerialConnectFunction();
}

bool OverrideFunction(const std::string& name,
                      ExtensionFunctionFactory factory) {
  return ExtensionFunctionRegistry::GetInstance()->OverrideFunctionForTesting(
      name, factory);
}

}  // namespace

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
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

#if SIMULATE_SERIAL_PORTS
  ASSERT_TRUE(OverrideFunction("serial.getDevices",
                               FakeSerialGetDevicesFunctionFactory));
  ASSERT_TRUE(
      OverrideFunction("serial.connect", FakeSerialConnectFunctionFactory));
#endif

  ASSERT_TRUE(RunExtensionTest("serial/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialRealHardware) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ASSERT_TRUE(RunExtensionTest("serial/real_hardware")) << message_;
}

}  // namespace extensions
