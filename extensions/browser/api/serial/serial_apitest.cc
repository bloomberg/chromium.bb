// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/browser_thread.h"
#include "device/serial/buffer.h"
#include "device/serial/test_serial_io_handler.h"
#include "extensions/browser/api/serial/serial_api.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/common/api/serial.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/device/public/interfaces/serial.mojom.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace extensions {
namespace {

class FakeSerialDeviceEnumerator
    : public device::mojom::SerialDeviceEnumerator {
 public:
  FakeSerialDeviceEnumerator() = default;
  ~FakeSerialDeviceEnumerator() override = default;

 private:
  // device::mojom::SerialDeviceEnumerator methods:
  void GetDevices(GetDevicesCallback callback) override {
    std::vector<device::mojom::SerialDeviceInfoPtr> devices;
    auto device0 = device::mojom::SerialDeviceInfo::New();
    device0->path = "/dev/fakeserialmojo";
    auto device1 = device::mojom::SerialDeviceInfo::New();
    device1->path = "\\\\COM800\\";
    devices.push_back(std::move(device0));
    devices.push_back(std::move(device1));
    std::move(callback).Run(std::move(devices));
  }

  DISALLOW_COPY_AND_ASSIGN(FakeSerialDeviceEnumerator);
};

class FakeSerialIoHandler : public device::mojom::SerialIoHandler {
 public:
  FakeSerialIoHandler() : test_io_handler_(new device::TestSerialIoHandler()) {
    test_io_handler_->device_control_signals()->dcd = true;
    test_io_handler_->device_control_signals()->cts = true;
    test_io_handler_->device_control_signals()->ri = true;
    test_io_handler_->device_control_signals()->dsr = true;
  }
  ~FakeSerialIoHandler() override = default;

 private:
  // device::mojom::SerialIoHandler methods:
  void Open(const std::string& port,
            device::mojom::SerialConnectionOptionsPtr options,
            OpenCallback callback) override {
    test_io_handler_->Open(
        port, *options,
        base::Bind([](OpenCallback callback,
                      bool success) { std::move(callback).Run(success); },
                   base::Passed(&callback)));
  }
  void Read(uint32_t bytes, ReadCallback callback) override {
    auto buffer =
        base::MakeRefCounted<net::IOBuffer>(static_cast<size_t>(bytes));
    test_io_handler_->Read(base::MakeUnique<device::ReceiveBuffer>(
        buffer, bytes,
        base::Bind(
            [](ReadCallback callback, scoped_refptr<net::IOBuffer> buffer,
               int bytes_read, device::mojom::SerialReceiveError error) {
              std::move(callback).Run(
                  std::vector<uint8_t>(buffer->data(),
                                       buffer->data() + bytes_read),
                  error);
            },
            base::Passed(&callback), buffer)));
  }
  void Write(const std::vector<uint8_t>& data,
             WriteCallback callback) override {
    test_io_handler_->Write(base::MakeUnique<device::SendBuffer>(
        std::vector<char>(data.data(), data.data() + data.size()),
        base::Bind(
            [](WriteCallback callback, int bytes_sent,
               device::mojom::SerialSendError error) {
              std::move(callback).Run(bytes_sent, error);
            },
            base::Passed(&callback))));
  }
  void CancelRead(device::mojom::SerialReceiveError reason) override {
    test_io_handler_->CancelRead(reason);
  }
  void CancelWrite(device::mojom::SerialSendError reason) override {
    test_io_handler_->CancelWrite(reason);
  }
  void Flush(FlushCallback callback) override {
    std::move(callback).Run(test_io_handler_->Flush());
  }
  void GetControlSignals(GetControlSignalsCallback callback) override {
    std::move(callback).Run(test_io_handler_->GetControlSignals());
  }
  void SetControlSignals(device::mojom::SerialHostControlSignalsPtr signals,
                         SetControlSignalsCallback callback) override {
    std::move(callback).Run(test_io_handler_->SetControlSignals(*signals));
  }
  void ConfigurePort(device::mojom::SerialConnectionOptionsPtr options,
                     ConfigurePortCallback callback) override {
    std::move(callback).Run(test_io_handler_->ConfigurePort(*options));
  }
  void GetPortInfo(GetPortInfoCallback callback) override {
    std::move(callback).Run(test_io_handler_->GetPortInfo());
  }
  void SetBreak(SetBreakCallback callback) override {
    std::move(callback).Run(test_io_handler_->SetBreak());
  }
  void ClearBreak(ClearBreakCallback callback) override {
    std::move(callback).Run(test_io_handler_->ClearBreak());
  }

  // TODO(leonhsl): Eliminate this dependency on device/serial.
  scoped_refptr<device::TestSerialIoHandler> test_io_handler_;

  DISALLOW_COPY_AND_ASSIGN(FakeSerialIoHandler);
};

void BindSerialDeviceEnumerator(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle,
    const service_manager::BindSourceInfo& source_info) {
  mojo::MakeStrongBinding(
      base::MakeUnique<FakeSerialDeviceEnumerator>(),
      device::mojom::SerialDeviceEnumeratorRequest(std::move(handle)));
}

void BindSerialIoHandler(const std::string& interface_name,
                         mojo::ScopedMessagePipeHandle handle,
                         const service_manager::BindSourceInfo& source_info) {
  mojo::MakeStrongBinding(
      base::MakeUnique<FakeSerialIoHandler>(),
      device::mojom::SerialIoHandlerRequest(std::move(handle)));
}

void DropBindRequest(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle handle,
                     const service_manager::BindSourceInfo& source_info) {}

class SerialApiTest : public ExtensionApiTest {
 public:
  SerialApiTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override { ExtensionApiTest::SetUpOnMainThread(); }

  void TearDownOnMainThread() override {
    ExtensionApiTest::TearDownOnMainThread();
  }

 protected:
  // Because Device Service also runs in this process(browser process), we can
  // set our binder to intercept requests for
  // SerialDeviceEnumerator/SerialIoHandler interfaces to it.
  void InterceptSerialDeviceEnumerator(
      const service_manager::BinderRegistryWithArgs<
          const service_manager::BindSourceInfo&>::Binder& binder) {
    service_manager::ServiceContext::SetGlobalBinderForTesting(
        device::mojom::kServiceName,
        device::mojom::SerialDeviceEnumerator::Name_, binder);
  }

  void InterceptSerialIoHandler(
      const service_manager::BinderRegistryWithArgs<
          const service_manager::BindSourceInfo&>::Binder& binder) {
    service_manager::ServiceContext::SetGlobalBinderForTesting(
        device::mojom::kServiceName, device::mojom::SerialIoHandler::Name_,
        binder);
  }
};

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
  InterceptSerialDeviceEnumerator(base::Bind(&BindSerialDeviceEnumerator));
  InterceptSerialIoHandler(base::Bind(&BindSerialIoHandler));
#endif

  ASSERT_TRUE(RunExtensionTest("serial/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialRealHardware) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  InterceptSerialDeviceEnumerator(base::Bind(&BindSerialDeviceEnumerator));
  ASSERT_TRUE(RunExtensionTest("serial/real_hardware")) << message_;
}

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialRealHardwareFail) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  // Intercept the request and then drop it, chrome.serial.getDevices() should
  // get an empty list.
  InterceptSerialDeviceEnumerator(base::Bind(&DropBindRequest));
  ASSERT_TRUE(RunExtensionTest("serial/real_hardware_fail")) << message_;
}

}  // namespace extensions
