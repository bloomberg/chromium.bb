// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_device_enumerator.h"
#include "device/serial/serial_service_impl.h"
#include "device/serial/test_serial_io_handler.h"
#include "extensions/renderer/api_test_base.h"
#include "grit/extensions_renderer_resources.h"

namespace extensions {

namespace {

class FakeSerialDeviceEnumerator : public device::SerialDeviceEnumerator {
  virtual mojo::Array<device::serial::DeviceInfoPtr> GetDevices() OVERRIDE {
    mojo::Array<device::serial::DeviceInfoPtr> result(3);
    result[0] = device::serial::DeviceInfo::New();
    result[0]->path = "device";
    result[0]->vendor_id = 1234;
    result[0]->has_vendor_id = true;
    result[0]->product_id = 5678;
    result[0]->has_product_id = true;
    result[0]->display_name = "foo";
    result[1] = device::serial::DeviceInfo::New();
    result[1]->path = "another_device";
    // These IDs should be ignored.
    result[1]->vendor_id = 1234;
    result[1]->product_id = 5678;
    result[2] = device::serial::DeviceInfo::New();
    result[2]->display_name = "";
    return result.Pass();
  }
};

enum OptionalValue {
  OPTIONAL_VALUE_UNSET,
  OPTIONAL_VALUE_FALSE,
  OPTIONAL_VALUE_TRUE,
};

device::serial::HostControlSignals GenerateControlSignals(OptionalValue dtr,
                                                          OptionalValue rts) {
  device::serial::HostControlSignals result;
  switch (dtr) {
    case OPTIONAL_VALUE_UNSET:
      break;
    case OPTIONAL_VALUE_FALSE:
      result.dtr = false;
      result.has_dtr = true;
      break;
    case OPTIONAL_VALUE_TRUE:
      result.dtr = true;
      result.has_dtr = true;
      break;
  }
  switch (rts) {
    case OPTIONAL_VALUE_UNSET:
      break;
    case OPTIONAL_VALUE_FALSE:
      result.rts = false;
      result.has_rts = true;
      break;
    case OPTIONAL_VALUE_TRUE:
      result.rts = true;
      result.has_rts = true;
      break;
  }
  return result;
}

device::serial::ConnectionOptions GenerateConnectionOptions(
    int bitrate,
    device::serial::DataBits data_bits,
    device::serial::ParityBit parity_bit,
    device::serial::StopBits stop_bits,
    OptionalValue cts_flow_control) {
  device::serial::ConnectionOptions result;
  result.bitrate = bitrate;
  result.data_bits = data_bits;
  result.parity_bit = parity_bit;
  result.stop_bits = stop_bits;
  switch (cts_flow_control) {
    case OPTIONAL_VALUE_UNSET:
      break;
    case OPTIONAL_VALUE_FALSE:
      result.cts_flow_control = false;
      result.has_cts_flow_control = true;
      break;
    case OPTIONAL_VALUE_TRUE:
      result.cts_flow_control = true;
      result.has_cts_flow_control = true;
      break;
  }
  return result;
}

class TestIoHandlerBase : public device::TestSerialIoHandler {
 public:
  TestIoHandlerBase() : calls_(0) {}

  size_t num_calls() const { return calls_; }

 protected:
  virtual ~TestIoHandlerBase() {}
  void record_call() const { calls_++; }

 private:
  mutable size_t calls_;

  DISALLOW_COPY_AND_ASSIGN(TestIoHandlerBase);
};

class SetControlSignalsTestIoHandler : public TestIoHandlerBase {
 public:
  SetControlSignalsTestIoHandler() {}

  virtual bool SetControlSignals(
      const device::serial::HostControlSignals& signals) OVERRIDE {
    static const device::serial::HostControlSignals expected_signals[] = {
        GenerateControlSignals(OPTIONAL_VALUE_UNSET, OPTIONAL_VALUE_UNSET),
        GenerateControlSignals(OPTIONAL_VALUE_FALSE, OPTIONAL_VALUE_UNSET),
        GenerateControlSignals(OPTIONAL_VALUE_TRUE, OPTIONAL_VALUE_UNSET),
        GenerateControlSignals(OPTIONAL_VALUE_UNSET, OPTIONAL_VALUE_FALSE),
        GenerateControlSignals(OPTIONAL_VALUE_FALSE, OPTIONAL_VALUE_FALSE),
        GenerateControlSignals(OPTIONAL_VALUE_TRUE, OPTIONAL_VALUE_FALSE),
        GenerateControlSignals(OPTIONAL_VALUE_UNSET, OPTIONAL_VALUE_TRUE),
        GenerateControlSignals(OPTIONAL_VALUE_FALSE, OPTIONAL_VALUE_TRUE),
        GenerateControlSignals(OPTIONAL_VALUE_TRUE, OPTIONAL_VALUE_TRUE),
    };
    if (num_calls() >= arraysize(expected_signals))
      return false;

    EXPECT_EQ(expected_signals[num_calls()].has_dtr, signals.has_dtr);
    EXPECT_EQ(expected_signals[num_calls()].dtr, signals.dtr);
    EXPECT_EQ(expected_signals[num_calls()].has_rts, signals.has_rts);
    EXPECT_EQ(expected_signals[num_calls()].rts, signals.rts);
    record_call();
    return true;
  }

 private:
  virtual ~SetControlSignalsTestIoHandler() {}

  DISALLOW_COPY_AND_ASSIGN(SetControlSignalsTestIoHandler);
};

class GetControlSignalsTestIoHandler : public TestIoHandlerBase {
 public:
  GetControlSignalsTestIoHandler() {}

  virtual device::serial::DeviceControlSignalsPtr GetControlSignals()
      const OVERRIDE {
    device::serial::DeviceControlSignalsPtr signals(
        device::serial::DeviceControlSignals::New());
    signals->dcd = num_calls() & 1;
    signals->cts = num_calls() & 2;
    signals->ri = num_calls() & 4;
    signals->dsr = num_calls() & 8;
    record_call();
    return signals.Pass();
  }

 private:
  virtual ~GetControlSignalsTestIoHandler() {}

  DISALLOW_COPY_AND_ASSIGN(GetControlSignalsTestIoHandler);
};

class ConfigurePortTestIoHandler : public TestIoHandlerBase {
 public:
  ConfigurePortTestIoHandler() {}
  virtual bool ConfigurePort(
      const device::serial::ConnectionOptions& options) OVERRIDE {
    static const device::serial::ConnectionOptions expected_options[] = {
        GenerateConnectionOptions(9600,
                                  device::serial::DATA_BITS_EIGHT,
                                  device::serial::PARITY_BIT_NO,
                                  device::serial::STOP_BITS_ONE,
                                  OPTIONAL_VALUE_FALSE),
        GenerateConnectionOptions(57600,
                                  device::serial::DATA_BITS_NONE,
                                  device::serial::PARITY_BIT_NONE,
                                  device::serial::STOP_BITS_NONE,
                                  OPTIONAL_VALUE_UNSET),
        GenerateConnectionOptions(0,
                                  device::serial::DATA_BITS_SEVEN,
                                  device::serial::PARITY_BIT_NONE,
                                  device::serial::STOP_BITS_NONE,
                                  OPTIONAL_VALUE_UNSET),
        GenerateConnectionOptions(0,
                                  device::serial::DATA_BITS_EIGHT,
                                  device::serial::PARITY_BIT_NONE,
                                  device::serial::STOP_BITS_NONE,
                                  OPTIONAL_VALUE_UNSET),
        GenerateConnectionOptions(0,
                                  device::serial::DATA_BITS_NONE,
                                  device::serial::PARITY_BIT_NO,
                                  device::serial::STOP_BITS_NONE,
                                  OPTIONAL_VALUE_UNSET),
        GenerateConnectionOptions(0,
                                  device::serial::DATA_BITS_NONE,
                                  device::serial::PARITY_BIT_ODD,
                                  device::serial::STOP_BITS_NONE,
                                  OPTIONAL_VALUE_UNSET),
        GenerateConnectionOptions(0,
                                  device::serial::DATA_BITS_NONE,
                                  device::serial::PARITY_BIT_EVEN,
                                  device::serial::STOP_BITS_NONE,
                                  OPTIONAL_VALUE_UNSET),
        GenerateConnectionOptions(0,
                                  device::serial::DATA_BITS_NONE,
                                  device::serial::PARITY_BIT_NONE,
                                  device::serial::STOP_BITS_ONE,
                                  OPTIONAL_VALUE_UNSET),
        GenerateConnectionOptions(0,
                                  device::serial::DATA_BITS_NONE,
                                  device::serial::PARITY_BIT_NONE,
                                  device::serial::STOP_BITS_TWO,
                                  OPTIONAL_VALUE_UNSET),
        GenerateConnectionOptions(0,
                                  device::serial::DATA_BITS_NONE,
                                  device::serial::PARITY_BIT_NONE,
                                  device::serial::STOP_BITS_NONE,
                                  OPTIONAL_VALUE_FALSE),
        GenerateConnectionOptions(0,
                                  device::serial::DATA_BITS_NONE,
                                  device::serial::PARITY_BIT_NONE,
                                  device::serial::STOP_BITS_NONE,
                                  OPTIONAL_VALUE_TRUE),
    };
    if (num_calls() >= arraysize(expected_options))
      return false;

    EXPECT_EQ(expected_options[num_calls()].bitrate, options.bitrate);
    EXPECT_EQ(expected_options[num_calls()].data_bits, options.data_bits);
    EXPECT_EQ(expected_options[num_calls()].parity_bit, options.parity_bit);
    EXPECT_EQ(expected_options[num_calls()].stop_bits, options.stop_bits);
    EXPECT_EQ(expected_options[num_calls()].has_cts_flow_control,
              options.has_cts_flow_control);
    EXPECT_EQ(expected_options[num_calls()].cts_flow_control,
              options.cts_flow_control);
    record_call();
    return TestSerialIoHandler::ConfigurePort(options);
  }

 private:
  virtual ~ConfigurePortTestIoHandler() {}

  DISALLOW_COPY_AND_ASSIGN(ConfigurePortTestIoHandler);
};

class FlushTestIoHandler : public TestIoHandlerBase {
 public:
  FlushTestIoHandler() {}

  virtual bool Flush() const OVERRIDE {
    record_call();
    return true;
  }

 private:
  virtual ~FlushTestIoHandler() {}

  DISALLOW_COPY_AND_ASSIGN(FlushTestIoHandler);
};

class FailToConnectTestIoHandler : public TestIoHandlerBase {
 public:
  FailToConnectTestIoHandler() {}
  virtual void Open(const std::string& port,
                    const OpenCompleteCallback& callback) OVERRIDE {
    callback.Run(false);
    return;
  }

 private:
  virtual ~FailToConnectTestIoHandler() {}

  DISALLOW_COPY_AND_ASSIGN(FailToConnectTestIoHandler);
};

class FailToGetInfoTestIoHandler : public TestIoHandlerBase {
 public:
  explicit FailToGetInfoTestIoHandler(int times_to_succeed)
      : times_to_succeed_(times_to_succeed) {}
  virtual device::serial::ConnectionInfoPtr GetPortInfo() const OVERRIDE {
    if (times_to_succeed_-- > 0)
      return device::TestSerialIoHandler::GetPortInfo();
    return device::serial::ConnectionInfoPtr();
  }

 private:
  virtual ~FailToGetInfoTestIoHandler() {}

  mutable int times_to_succeed_;

  DISALLOW_COPY_AND_ASSIGN(FailToGetInfoTestIoHandler);
};

}  // namespace

class SerialApiTest : public ApiTestBase {
 public:
  SerialApiTest() {}

  virtual void SetUp() OVERRIDE {
    ApiTestBase::SetUp();
    env()->RegisterModule("serial", IDR_SERIAL_CUSTOM_BINDINGS_JS);
    env()->RegisterModule("serial_service", IDR_SERIAL_SERVICE_JS);
    env()->RegisterModule("device/serial/serial.mojom", IDR_SERIAL_MOJOM_JS);
    service_provider()->AddService<device::serial::SerialService>(base::Bind(
        &SerialApiTest::CreateSerialService, base::Unretained(this)));
  }

  virtual void TearDown() OVERRIDE {
    if (io_handler_)
      EXPECT_TRUE(io_handler_->HasOneRef());
    ApiTestBase::TearDown();
  }

  scoped_refptr<TestIoHandlerBase> io_handler_;

 private:
  scoped_refptr<device::SerialIoHandler> GetIoHandler() {
    if (!io_handler_)
      io_handler_ = new TestIoHandlerBase;
    return io_handler_;
  }

  void CreateSerialService(
      mojo::InterfaceRequest<device::serial::SerialService> request) {
    mojo::BindToRequest(new device::SerialServiceImpl(
                            new device::SerialConnectionFactory(
                                base::Bind(&SerialApiTest::GetIoHandler,
                                           base::Unretained(this)),
                                base::MessageLoopProxy::current()),
                            scoped_ptr<device::SerialDeviceEnumerator>(
                                new FakeSerialDeviceEnumerator)),
                        &request);
  }

  DISALLOW_COPY_AND_ASSIGN(SerialApiTest);
};

TEST_F(SerialApiTest, GetDevices) {
  RunTest("serial_unittest.js", "testGetDevices");
}

TEST_F(SerialApiTest, ConnectFail) {
  io_handler_ = new FailToConnectTestIoHandler;
  RunTest("serial_unittest.js", "testConnectFail");
}

TEST_F(SerialApiTest, GetInfoFailOnConnect) {
  io_handler_ = new FailToGetInfoTestIoHandler(0);
  RunTest("serial_unittest.js", "testGetInfoFailOnConnect");
}

TEST_F(SerialApiTest, Connect) {
  RunTest("serial_unittest.js", "testConnect");
}

TEST_F(SerialApiTest, ConnectDefaultOptions) {
  RunTest("serial_unittest.js", "testConnectDefaultOptions");
}

TEST_F(SerialApiTest, ConnectInvalidBitrate) {
  RunTest("serial_unittest.js", "testConnectInvalidBitrate");
}

TEST_F(SerialApiTest, GetInfo) {
  RunTest("serial_unittest.js", "testGetInfo");
}

TEST_F(SerialApiTest, GetInfoFailToGetPortInfo) {
  io_handler_ = new FailToGetInfoTestIoHandler(1);
  RunTest("serial_unittest.js", "testGetInfoFailToGetPortInfo");
}

TEST_F(SerialApiTest, GetConnections) {
  RunTest("serial_unittest.js", "testGetConnections");
}

TEST_F(SerialApiTest, GetControlSignals) {
  io_handler_ = new GetControlSignalsTestIoHandler;
  RunTest("serial_unittest.js", "testGetControlSignals");
  EXPECT_EQ(16u, io_handler_->num_calls());
}

TEST_F(SerialApiTest, SetControlSignals) {
  io_handler_ = new SetControlSignalsTestIoHandler;
  RunTest("serial_unittest.js", "testSetControlSignals");
  EXPECT_EQ(9u, io_handler_->num_calls());
}

TEST_F(SerialApiTest, Update) {
  io_handler_ = new ConfigurePortTestIoHandler;
  RunTest("serial_unittest.js", "testUpdate");
  EXPECT_EQ(11u, io_handler_->num_calls());
}

TEST_F(SerialApiTest, UpdateInvalidBitrate) {
  io_handler_ = new ConfigurePortTestIoHandler;
  RunTest("serial_unittest.js", "testUpdateInvalidBitrate");
  EXPECT_EQ(1u, io_handler_->num_calls());
}

TEST_F(SerialApiTest, Flush) {
  io_handler_ = new FlushTestIoHandler;
  RunTest("serial_unittest.js", "testFlush");
  EXPECT_EQ(1u, io_handler_->num_calls());
}

TEST_F(SerialApiTest, SetPaused) {
  RunTest("serial_unittest.js", "testSetPaused");
}

TEST_F(SerialApiTest, DisconnectUnknownConnectionId) {
  RunTest("serial_unittest.js", "testDisconnectUnknownConnectionId");
}

TEST_F(SerialApiTest, GetInfoUnknownConnectionId) {
  RunTest("serial_unittest.js", "testGetInfoUnknownConnectionId");
}

TEST_F(SerialApiTest, UpdateUnknownConnectionId) {
  RunTest("serial_unittest.js", "testUpdateUnknownConnectionId");
}

TEST_F(SerialApiTest, SetControlSignalsUnknownConnectionId) {
  RunTest("serial_unittest.js", "testSetControlSignalsUnknownConnectionId");
}

TEST_F(SerialApiTest, GetControlSignalsUnknownConnectionId) {
  RunTest("serial_unittest.js", "testGetControlSignalsUnknownConnectionId");
}

TEST_F(SerialApiTest, FlushUnknownConnectionId) {
  RunTest("serial_unittest.js", "testFlushUnknownConnectionId");
}

TEST_F(SerialApiTest, SetPausedUnknownConnectionId) {
  RunTest("serial_unittest.js", "testSetPausedUnknownConnectionId");
}

}  // namespace extensions
