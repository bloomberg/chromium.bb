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

class FailToOpenIoHandler : public TestSerialIoHandler {
 public:
  virtual void Open(const std::string& port,
                    const OpenCompleteCallback& callback) OVERRIDE {
    callback.Run(false);
  }

 protected:
  virtual ~FailToOpenIoHandler() {}
};

}  // namespace

class SerialServiceTest : public testing::Test, public mojo::ErrorHandler {
 public:
  SerialServiceTest() : connected_(false), expecting_error_(false) {}

  void StoreDevices(mojo::Array<serial::DeviceInfoPtr> devices) {
    devices_ = devices.Pass();
    StopMessageLoop();
  }

  virtual void OnConnectionError() OVERRIDE {
    StopMessageLoop();
    EXPECT_TRUE(expecting_error_);
  }

  void RunMessageLoop() {
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
  }

  void StopMessageLoop() {
    ASSERT_TRUE(run_loop_);
    message_loop_.PostTask(FROM_HERE, run_loop_->QuitClosure());
  }

  void OnGotInfo(serial::ConnectionInfoPtr options) {
    connected_ = true;
    StopMessageLoop();
  }

  scoped_refptr<SerialIoHandler> ReturnIoHandler() { return io_handler_; }

  void RunConnectTest(const std::string& path, bool expecting_success) {
    if (!io_handler_.get())
      io_handler_ = new TestSerialIoHandler;
    mojo::InterfacePtr<serial::SerialService> service;
    mojo::BindToProxy(
        new SerialServiceImpl(
            new SerialConnectionFactory(
                base::Bind(&SerialServiceTest::ReturnIoHandler,
                           base::Unretained(this)),
                base::MessageLoopProxy::current()),
            scoped_ptr<SerialDeviceEnumerator>(new FakeSerialDeviceEnumerator)),
        &service);
    mojo::InterfacePtr<serial::Connection> connection;
    mojo::InterfacePtr<serial::DataSink> sink;
    mojo::InterfacePtr<serial::DataSource> source;
    service->Connect(path,
                     serial::ConnectionOptions::New(),
                     mojo::Get(&connection),
                     mojo::Get(&sink),
                     mojo::Get(&source));
    connection.set_error_handler(this);
    expecting_error_ = !expecting_success;
    connection->GetInfo(
        base::Bind(&SerialServiceTest::OnGotInfo, base::Unretained(this)));
    RunMessageLoop();
    EXPECT_EQ(!expecting_success, connection.encountered_error());
    EXPECT_EQ(expecting_success, connected_);
    connection.reset();
  }

  base::MessageLoop message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  mojo::Array<serial::DeviceInfoPtr> devices_;
  scoped_refptr<TestSerialIoHandler> io_handler_;
  bool connected_;
  bool expecting_error_;
  serial::ConnectionInfoPtr info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerialServiceTest);
};

TEST_F(SerialServiceTest, GetDevices) {
  mojo::InterfacePtr<serial::SerialService> service;
  SerialServiceImpl::Create(NULL, mojo::Get(&service));
  service.set_error_handler(this);
  mojo::Array<serial::DeviceInfoPtr> result;
  service->GetDevices(
      base::Bind(&SerialServiceTest::StoreDevices, base::Unretained(this)));
  RunMessageLoop();

  // Because we're running on unknown hardware, only check that we received a
  // non-null result.
  EXPECT_TRUE(devices_);
}

TEST_F(SerialServiceTest, Connect) {
  RunConnectTest("device", true);
}

TEST_F(SerialServiceTest, ConnectInvalidPath) {
  RunConnectTest("invalid_path", false);
}

TEST_F(SerialServiceTest, ConnectOpenFailed) {
  io_handler_ = new FailToOpenIoHandler;
  RunConnectTest("device", false);
}

}  // namespace device
