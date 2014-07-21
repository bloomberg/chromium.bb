// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "device/serial/serial.mojom.h"
#include "device/serial/serial_service_impl.h"
#include "device/serial/test_serial_io_handler.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

class FakeSerialDeviceEnumerator : public SerialDeviceEnumerator {
  virtual mojo::Array<serial::DeviceInfoPtr> GetDevices() OVERRIDE {
    mojo::Array<serial::DeviceInfoPtr> devices(1);
    devices[0] = serial::DeviceInfo::New();
    devices[0]->path = "device";
    return devices.Pass();
  }
};

}  // namespace

class SerialConnectionTest : public testing::Test, public mojo::ErrorHandler {
 public:
  SerialConnectionTest() : connected_(false), success_(false) {}

  virtual void SetUp() OVERRIDE {
    message_loop_.reset(new base::MessageLoop);
    mojo::InterfacePtr<serial::SerialService> service;
    mojo::BindToProxy(
        new SerialServiceImpl(
            new SerialConnectionFactory(
                base::Bind(&SerialConnectionTest::CreateIoHandler,
                           base::Unretained(this)),
                base::MessageLoopProxy::current()),
            scoped_ptr<SerialDeviceEnumerator>(new FakeSerialDeviceEnumerator)),
        &service);
    service.set_error_handler(this);
    service->Connect(
        "device", serial::ConnectionOptions::New(), mojo::Get(&connection_));
    connection_.set_error_handler(this);
    connection_->GetInfo(
        base::Bind(&SerialConnectionTest::StoreInfo, base::Unretained(this)));
    RunMessageLoop();
    ASSERT_TRUE(io_handler_);
  }

  void StoreInfo(serial::ConnectionInfoPtr options) {
    info_ = options.Pass();
    StopMessageLoop();
  }

  void StoreControlSignals(serial::DeviceControlSignalsPtr signals) {
    signals_ = signals.Pass();
    StopMessageLoop();
  }

  void StoreSuccess(bool success) {
    success_ = success;
    StopMessageLoop();
  }

  void RunMessageLoop() {
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
  }

  void StopMessageLoop() {
    ASSERT_TRUE(message_loop_);
    ASSERT_TRUE(run_loop_);
    message_loop_->PostTask(FROM_HERE, run_loop_->QuitClosure());
  }

  scoped_refptr<SerialIoHandler> CreateIoHandler() {
    io_handler_ = new TestSerialIoHandler;
    return io_handler_;
  }

  virtual void OnConnectionError() OVERRIDE {
    StopMessageLoop();
    FAIL() << "Connection error";
  }

  mojo::Array<serial::DeviceInfoPtr> devices_;
  serial::ConnectionInfoPtr info_;
  serial::DeviceControlSignalsPtr signals_;
  bool connected_;
  bool success_;

  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  mojo::InterfacePtr<serial::Connection> connection_;
  scoped_refptr<TestSerialIoHandler> io_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerialConnectionTest);
};

TEST_F(SerialConnectionTest, GetInfo) {
  // |info_| is filled in during SetUp().
  ASSERT_TRUE(info_);
  EXPECT_EQ(9600u, info_->bitrate);
  EXPECT_EQ(serial::DATA_BITS_EIGHT, info_->data_bits);
  EXPECT_EQ(serial::PARITY_BIT_NO, info_->parity_bit);
  EXPECT_EQ(serial::STOP_BITS_ONE, info_->stop_bits);
  EXPECT_FALSE(info_->cts_flow_control);
}

TEST_F(SerialConnectionTest, SetOptions) {
  serial::ConnectionOptionsPtr options(serial::ConnectionOptions::New());
  options->bitrate = 12345;
  options->data_bits = serial::DATA_BITS_SEVEN;
  options->has_cts_flow_control = true;
  options->cts_flow_control = true;
  connection_->SetOptions(
      options.Pass(),
      base::Bind(&SerialConnectionTest::StoreSuccess, base::Unretained(this)));
  RunMessageLoop();
  ASSERT_TRUE(success_);
  serial::ConnectionInfo* info = io_handler_->connection_info();
  EXPECT_EQ(12345u, info->bitrate);
  EXPECT_EQ(serial::DATA_BITS_SEVEN, info->data_bits);
  EXPECT_EQ(serial::PARITY_BIT_NO, info->parity_bit);
  EXPECT_EQ(serial::STOP_BITS_ONE, info->stop_bits);
  EXPECT_TRUE(info->cts_flow_control);
}

TEST_F(SerialConnectionTest, GetControlSignals) {
  connection_->GetControlSignals(base::Bind(
      &SerialConnectionTest::StoreControlSignals, base::Unretained(this)));
  serial::DeviceControlSignals* signals = io_handler_->device_control_signals();
  signals->dcd = true;
  signals->dsr = true;

  RunMessageLoop();
  ASSERT_TRUE(signals_);
  EXPECT_TRUE(signals_->dcd);
  EXPECT_FALSE(signals_->cts);
  EXPECT_FALSE(signals_->ri);
  EXPECT_TRUE(signals_->dsr);
}

TEST_F(SerialConnectionTest, SetControlSignals) {
  serial::HostControlSignalsPtr signals(serial::HostControlSignals::New());
  signals->has_dtr = true;
  signals->dtr = true;
  signals->has_rts = true;
  signals->rts = true;

  connection_->SetControlSignals(
      signals.Pass(),
      base::Bind(&SerialConnectionTest::StoreSuccess, base::Unretained(this)));
  RunMessageLoop();
  ASSERT_TRUE(success_);
  EXPECT_TRUE(io_handler_->dtr());
  EXPECT_TRUE(io_handler_->rts());
}

TEST_F(SerialConnectionTest, Flush) {
  ASSERT_EQ(0, io_handler_->flushes());
  connection_->Flush(
      base::Bind(&SerialConnectionTest::StoreSuccess, base::Unretained(this)));
  RunMessageLoop();
  ASSERT_TRUE(success_);
  EXPECT_EQ(1, io_handler_->flushes());
}

TEST_F(SerialConnectionTest, Disconnect) {
  connection_.reset();
  message_loop_.reset();
  EXPECT_TRUE(io_handler_->HasOneRef());
}

}  // namespace device
