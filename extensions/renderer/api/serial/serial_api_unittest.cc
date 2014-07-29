// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_device_enumerator.h"
#include "device/serial/serial_service_impl.h"
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

}  // namespace

void CreateSerialService(
    mojo::InterfaceRequest<device::serial::SerialService> request) {
  mojo::BindToRequest(
      new device::SerialServiceImpl(
          new device::SerialConnectionFactory(
              device::SerialConnectionFactory::IoHandlerFactory(),
              base::MessageLoopProxy::current()),
          scoped_ptr<device::SerialDeviceEnumerator>(
              new FakeSerialDeviceEnumerator)),
      &request);
}

class SerialApiTest : public ApiTestBase {
 public:
  virtual void SetUp() OVERRIDE {
    ApiTestBase::SetUp();
    env()->RegisterModule("serial", IDR_SERIAL_CUSTOM_BINDINGS_JS);
    env()->RegisterModule("serial_service", IDR_SERIAL_SERVICE_JS);
    env()->RegisterModule("device/serial/serial.mojom", IDR_SERIAL_MOJOM_JS);
    service_provider()->AddService<device::serial::SerialService>(
        base::Bind(CreateSerialService));
  }
};

TEST_F(SerialApiTest, GetDevices) {
  RunTest("serial_unittest.js", "testGetDevices");
}

}  // namespace extensions
