// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"
#include "chrome/browser/extensions/api/serial/serial_api.h"
#include "chrome/browser/extensions/api/serial/serial_connection.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

using content::BrowserThread;

namespace {

class SerialApiTest : public PlatformAppApiTest {
 public:
  SerialApiTest() {}
};

}  // namespace

namespace extensions {

class FakeSerialGetPortsFunction : public AsyncExtensionFunction {
 public:
  virtual bool RunImpl() {
    ListValue* ports = new ListValue();
    ports->Append(Value::CreateStringValue("/dev/fakeserial"));
    ports->Append(Value::CreateStringValue("\\\\COM800\\"));
    SetResult(ports);
    SendResponse(true);
    return true;
  }
 protected:
  virtual ~FakeSerialGetPortsFunction() {}
};

class FakeEchoSerialConnection : public SerialConnection {
 public:
  explicit FakeEchoSerialConnection(
      const std::string& port,
      int bitrate,
      const std::string& owner_extension_id,
      ApiResourceEventNotifier* event_notifier)
      : SerialConnection(port, bitrate, owner_extension_id, event_notifier),
        opened_(true) {
    Flush();
    opened_ = false;
  }

  virtual ~FakeEchoSerialConnection() {
  }

  virtual bool Open() {
    DCHECK(!opened_);
    opened_ = true;
    return true;
  }

  virtual void Close() {
    DCHECK(opened_);
  }

  virtual void Flush() {
    DCHECK(opened_);
    buffer_.clear();
  }

  virtual int Read(scoped_refptr<net::IOBufferWithSize> io_buffer) {
    DCHECK(io_buffer->data());

    if (buffer_.empty()) {
      return 0;
    }
    char *data = io_buffer->data();
    int bytes_to_copy = io_buffer->size();
    while (bytes_to_copy-- && !buffer_.empty()) {
      *data++ = buffer_.front();
      buffer_.pop_front();
    }
    return io_buffer->size();
  }

  virtual int Write(scoped_refptr<net::IOBuffer> io_buffer, int byte_count) {
    DCHECK(io_buffer.get());
    DCHECK(byte_count >= 0);

    char *data = io_buffer->data();
    int count = byte_count;
    while (count--)
      buffer_.push_back(*data++);
    return byte_count;
  }

  MOCK_METHOD1(GetControlSignals, bool(ControlSignals &));
  MOCK_METHOD1(SetControlSignals, bool(const ControlSignals &));

 private:
  bool opened_;
  std::deque<char> buffer_;

  DISALLOW_COPY_AND_ASSIGN(FakeEchoSerialConnection);
};

class FakeSerialOpenFunction : public SerialOpenFunction {
 protected:
  virtual SerialConnection* CreateSerialConnection(
      const std::string& port,
      int bitrate,
      const std::string& owner_extension_id,
      ApiResourceEventNotifier* event_notifier) OVERRIDE {
    FakeEchoSerialConnection* serial_connection =
        new FakeEchoSerialConnection(port, bitrate, owner_extension_id,
                                     event_notifier);
    EXPECT_CALL(*serial_connection, GetControlSignals(_)).
        Times(1).WillOnce(Return(true));
    EXPECT_CALL(*serial_connection, SetControlSignals(_)).
        Times(1).WillOnce(Return(true));
    return serial_connection;
  }
  virtual bool DoesPortExist(const std::string& port) OVERRIDE {
    return true;
  }

 protected:
  virtual ~FakeSerialOpenFunction() {}
};

}  // namespace extensions

ExtensionFunction* FakeSerialGetPortsFunctionFactory() {
  return new extensions::FakeSerialGetPortsFunction();
}

ExtensionFunction* FakeSerialOpenFunctionFactory() {
  return new extensions::FakeSerialOpenFunction();
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
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

#if SIMULATE_SERIAL_PORTS
  ASSERT_TRUE(ExtensionFunctionDispatcher::OverrideFunction(
      "serial.getPorts",
      FakeSerialGetPortsFunctionFactory));
  ASSERT_TRUE(ExtensionFunctionDispatcher::OverrideFunction(
      "serial.open",
      FakeSerialOpenFunctionFactory));
#endif

  ASSERT_TRUE(RunExtensionTest("serial/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialRealHardware) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ASSERT_TRUE(RunExtensionTest("serial/real_hardware")) << message_;
}
