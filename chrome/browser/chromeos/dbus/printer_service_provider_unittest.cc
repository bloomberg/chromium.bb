// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/printer_service_provider.h"

#include "chrome/browser/chromeos/dbus/service_provider_test_helper.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

const char kPrinterAdded[] = "PrinterAdded";

class MockPrinterServiceProvider : public PrinterServiceProvider {
 public:
  MOCK_METHOD2(ShowCloudPrintHelp,
               void(const std::string& vendor, const std::string& product));
};

class PrinterServiceProviderTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    test_helper_.SetUp(kPrinterAdded, &service_provider_);
  }

  virtual void TearDown() OVERRIDE {
    test_helper_.TearDown();
  }

 protected:
  MockPrinterServiceProvider service_provider_;
  ServiceProviderTestHelper test_helper_;
};

TEST_F(PrinterServiceProviderTest, ShowCloudPrintHelp) {
  dbus::MethodCall method_call(kLibCrosServiceInterface, kPrinterAdded);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("123");
  writer.AppendString("456");

  EXPECT_CALL(service_provider_, ShowCloudPrintHelp("123", "456"))
      .Times(1);

  // Call the PrinterAdded method.
  scoped_ptr<dbus::Response> response(test_helper_.CallMethod(&method_call));

  // An empty response should be returned.
  ASSERT_TRUE(response.get());
  dbus::MessageReader reader(response.get());
  ASSERT_FALSE(reader.HasMoreData());
}

}  // namespace chromeos

