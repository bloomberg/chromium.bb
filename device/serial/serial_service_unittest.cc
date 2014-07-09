// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "device/serial/serial.mojom.h"
#include "device/serial/serial_service_impl.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class SerialServiceTest : public testing::Test, public mojo::ErrorHandler {
 public:
  SerialServiceTest() {}

  void StoreDevices(mojo::Array<serial::DeviceInfoPtr> devices) {
    devices_ = devices.Pass();
    message_loop_.PostTask(FROM_HERE, run_loop_.QuitClosure());
  }

  virtual void OnConnectionError() OVERRIDE {
    message_loop_.PostTask(FROM_HERE, run_loop_.QuitClosure());
    FAIL() << "Connection error";
  }

  base::MessageLoop message_loop_;
  base::RunLoop run_loop_;
  mojo::Array<serial::DeviceInfoPtr> devices_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerialServiceTest);
};

TEST_F(SerialServiceTest, GetDevices) {
  mojo::InterfacePtr<serial::SerialService> service;
  SerialServiceImpl::Create(mojo::Get(&service));
  service.set_error_handler(this);
  mojo::Array<serial::DeviceInfoPtr> result;
  service->GetDevices(
      base::Bind(&SerialServiceTest::StoreDevices, base::Unretained(this)));
  run_loop_.Run();

  // Because we're running on unknown hardware, only check that we received a
  // non-null result.
  EXPECT_TRUE(devices_);
}

}  // namespace device
