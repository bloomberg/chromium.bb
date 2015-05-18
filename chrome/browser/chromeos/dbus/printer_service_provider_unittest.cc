// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/printer_service_provider.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/services/service_provider_test_helper.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "dbus/message.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

const char kPrinterAdded[] = "PrinterAdded";

const char kTestUserId[] = "test_user";

const char kPrinterAppExistsDelegateIDTemplate[] =
    "system.printer.printer_provider_exists/%s:%s";

const char kPrinterAppNotFoundDelegateIDTemplate[] =
    "system.printer.no_printer_provider_found/%s:%s";

class PrinterServiceProviderAppSearchEnabledTest : public testing::Test {
 public:
  PrinterServiceProviderAppSearchEnabledTest()
      : user_manager_(new user_manager::FakeUserManager()),
        user_manager_enabler_(user_manager_) {}

  ~PrinterServiceProviderAppSearchEnabledTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnablePrinterAppSearch);
    service_provider_.SetNotificationUIManagerForTesting(
        &notification_ui_manager_);
  }

 protected:
  void AddTestUser() {
    const user_manager::User* user = user_manager_->AddUser(kTestUserId);
    profile_.set_profile_name(kTestUserId);
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, &profile_);
  }

  bool InvokePrinterAdded(const std::string& vendor_id,
                          const std::string& product_id,
                          std::string* error) {
    *error = std::string();
    test_helper_.SetUp(kPrinterAdded, &service_provider_);

    dbus::MethodCall method_call(kLibCrosServiceInterface, kPrinterAdded);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(vendor_id);
    writer.AppendString(product_id);

    // Call the PrinterAdded method.
    scoped_ptr<dbus::Response> response(test_helper_.CallMethod(&method_call));

    // An empty response should be returned.
    bool success = true;
    if (response.get()) {
      dbus::MessageReader reader(response.get());
      if (reader.HasMoreData()) {
        *error = "Non empty response";
        success = false;
      }
    } else {
      *error = "No response.";
      success = false;
    }

    // Tear down the test helper so it can be reused in the test.
    test_helper_.TearDown();
    return success;
  }

  // Creates a test extension with the provided permissions.
  scoped_refptr<extensions::Extension> CreateTestExtension(
      extensions::ListBuilder* permissions_builder) {
    return extensions::ExtensionBuilder()
        .SetID("fake_extension_id")
        .SetManifest(
             extensions::DictionaryBuilder()
                 .Set("name", "Printer provider extension")
                 .Set("manifest_version", 2)
                 .Set("version", "1.0")
                 // Needed to enable usb API.
                 .Set("app",
                      extensions::DictionaryBuilder().Set(
                          "background",
                          extensions::DictionaryBuilder().Set(
                              "scripts",
                              extensions::ListBuilder().Append("bg.js"))))
                 .Set("permissions", *permissions_builder))
        .Build();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  StubNotificationUIManager notification_ui_manager_;
  TestingProfile profile_;
  user_manager::FakeUserManager* user_manager_;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;

  PrinterServiceProvider service_provider_;
  ServiceProviderTestHelper test_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrinterServiceProviderAppSearchEnabledTest);
};

TEST_F(PrinterServiceProviderAppSearchEnabledTest, ShowFindAppNotification) {
  AddTestUser();

  std::string error;
  ASSERT_TRUE(InvokePrinterAdded("123", "456", &error)) << error;

  ASSERT_EQ(1u, notification_ui_manager_.GetNotificationCount());
  const Notification& notification =
      notification_ui_manager_.GetNotificationAt(0);
  EXPECT_EQ("123:456", notification.tag());
  EXPECT_EQ(
      base::StringPrintf(kPrinterAppNotFoundDelegateIDTemplate, "123", "456"),
      notification.delegate_id());
}

TEST_F(PrinterServiceProviderAppSearchEnabledTest, ShowAppFoundNotification) {
  AddTestUser();

  scoped_refptr<extensions::Extension> extension = CreateTestExtension(
      &extensions::ListBuilder()
           .Append("usb")
           .Append("printerProvider")
           .Append(extensions::DictionaryBuilder().Set(
               "usbDevices", extensions::ListBuilder().Append(
                                 extensions::DictionaryBuilder()
                                     .Set("vendorId", 0x123)
                                     .Set("productId", 0x456)))));
  ASSERT_TRUE(
      extensions::ExtensionRegistry::Get(&profile_)->AddEnabled(extension));

  std::string error;
  ASSERT_TRUE(InvokePrinterAdded("123", "456", &error)) << error;

  ASSERT_EQ(1u, notification_ui_manager_.GetNotificationCount());
  const Notification& notification =
      notification_ui_manager_.GetNotificationAt(0);
  EXPECT_EQ("123:456", notification.tag());
  EXPECT_EQ(
      base::StringPrintf(kPrinterAppExistsDelegateIDTemplate, "123", "456"),
      notification.delegate_id());
}

TEST_F(PrinterServiceProviderAppSearchEnabledTest,
       UsbHandlerExists_NotPrinterProvider) {
  AddTestUser();

  scoped_refptr<extensions::Extension> extension =
      CreateTestExtension(&extensions::ListBuilder().Append("usb").Append(
          extensions::DictionaryBuilder().Set(
              "usbDevices",
              extensions::ListBuilder().Append(extensions::DictionaryBuilder()
                                                   .Set("vendorId", 0x123)
                                                   .Set("productId", 0xf56)))));
  ASSERT_TRUE(
      extensions::ExtensionRegistry::Get(&profile_)->AddEnabled(extension));

  std::string error;
  ASSERT_TRUE(InvokePrinterAdded("123", "f56", &error)) << error;

  ASSERT_EQ(1u, notification_ui_manager_.GetNotificationCount());
  const Notification& notification =
      notification_ui_manager_.GetNotificationAt(0);
  EXPECT_EQ("123:F56", notification.tag());
  EXPECT_EQ(
      base::StringPrintf(kPrinterAppNotFoundDelegateIDTemplate, "123", "F56"),
      notification.delegate_id());
}

TEST_F(PrinterServiceProviderAppSearchEnabledTest,
       PrinterProvider_DifferentUsbProductId) {
  AddTestUser();

  scoped_refptr<extensions::Extension> extension = CreateTestExtension(
      &extensions::ListBuilder()
           .Append("usb")
           .Append("printerProvider")
           .Append(extensions::DictionaryBuilder().Set(
               "usbDevices", extensions::ListBuilder().Append(
                                 extensions::DictionaryBuilder()
                                     .Set("vendorId", 0x123)
                                     .Set("productId", 0x001)))));
  ASSERT_TRUE(
      extensions::ExtensionRegistry::Get(&profile_)->AddEnabled(extension));

  std::string error;
  ASSERT_TRUE(InvokePrinterAdded("123", "456", &error)) << error;

  ASSERT_EQ(1u, notification_ui_manager_.GetNotificationCount());
  const Notification& notification =
      notification_ui_manager_.GetNotificationAt(0);
  EXPECT_EQ("123:456", notification.tag());
  EXPECT_EQ(
      base::StringPrintf(kPrinterAppNotFoundDelegateIDTemplate, "123", "456"),
      notification.delegate_id());
}

TEST_F(PrinterServiceProviderAppSearchEnabledTest, VendorIdOutOfBounds) {
  AddTestUser();

  std::string error;
  ASSERT_TRUE(InvokePrinterAdded("1F123", "456", &error)) << error;

  EXPECT_EQ(0u, notification_ui_manager_.GetNotificationCount());
}

TEST_F(PrinterServiceProviderAppSearchEnabledTest, ProductIdNaN) {
  AddTestUser();

  std::string error;
  ASSERT_TRUE(InvokePrinterAdded("123", "xxx", &error)) << error;

  EXPECT_EQ(0u, notification_ui_manager_.GetNotificationCount());
}

TEST_F(PrinterServiceProviderAppSearchEnabledTest, VendorIdNaN) {
  AddTestUser();

  std::string error;
  ASSERT_TRUE(InvokePrinterAdded("xxxfoo", "456", &error)) << error;

  EXPECT_EQ(0u, notification_ui_manager_.GetNotificationCount());
}

TEST_F(PrinterServiceProviderAppSearchEnabledTest, ProductIdOutOfBounds) {
  AddTestUser();

  std::string error;
  ASSERT_TRUE(InvokePrinterAdded("123", "1F456", &error)) << error;

  EXPECT_EQ(0u, notification_ui_manager_.GetNotificationCount());
}

TEST_F(PrinterServiceProviderAppSearchEnabledTest, NegativeProductId) {
  AddTestUser();

  std::string error;
  ASSERT_TRUE(InvokePrinterAdded("123", "-1", &error)) << error;

  EXPECT_EQ(0u, notification_ui_manager_.GetNotificationCount());
}

TEST_F(PrinterServiceProviderAppSearchEnabledTest, PrintersAddedWithNoIdArgs) {
  AddTestUser();

  test_helper_.SetUp(kPrinterAdded, &service_provider_);
  dbus::MethodCall method_call(kLibCrosServiceInterface, kPrinterAdded);
  dbus::MessageWriter writer(&method_call);

  // Call the PrinterAdded method.
  scoped_ptr<dbus::Response> response(test_helper_.CallMethod(&method_call));

  // An empty response should be returned.
  ASSERT_TRUE(response.get());
  dbus::MessageReader reader(response.get());
  ASSERT_FALSE(reader.HasMoreData());
  test_helper_.TearDown();

  EXPECT_EQ(0u, notification_ui_manager_.GetNotificationCount());
}

}  // namespace chromeos

