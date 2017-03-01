// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printer_detector/printer_detector.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/printer_detector/printer_detector_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/base/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::DictionaryBuilder;
using extensions::ListBuilder;

namespace chromeos {

namespace {

const uint8_t kPrinterInterfaceClass = 7;

const char kTestUserId[] = "test_user";

const char kPrinterAppExistsDelegateIDTemplate[] =
    "system.printer.printer_provider_exists/%s:%s";

const char kPrinterAppNotFoundDelegateIDTemplate[] =
    "system.printer.no_printer_provider_found/%s:%s";

std::unique_ptr<KeyedService> CreatePrinterDetector(
    content::BrowserContext* context) {
  return PrinterDetector::CreateLegacy(Profile::FromBrowserContext(context));
}

}  // namespace

// TODO(tbarzic): Rename this test.
class PrinterDetectorAppSearchEnabledTest : public testing::Test {
 public:
  PrinterDetectorAppSearchEnabledTest()
      : user_manager_(new chromeos::FakeChromeUserManager()),
        user_manager_enabler_(user_manager_) {}

  ~PrinterDetectorAppSearchEnabledTest() override = default;

  void SetUp() override {
    device_client_.GetUsbService();
    // Make sure the profile is created after adding the switch and setting up
    // device client.
    profile_.reset(new TestingProfile());
    chromeos::PrinterDetectorFactory::GetInstance()->SetTestingFactoryAndUse(
        profile_.get(), &CreatePrinterDetector);
    AddTestUser();
    SetExtensionSystemReady(profile_.get());
  }

 protected:
  void SetExtensionSystemReady(TestingProfile* profile) {
    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile));
    test_extension_system->SetReady();
    base::RunLoop().RunUntilIdle();
  }

  void AddTestUser() {
    const user_manager::User* user =
        user_manager_->AddUser(AccountId::FromUserEmail(kTestUserId));
    profile_->set_profile_name(kTestUserId);
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, profile_.get());
    chromeos::PrinterDetectorFactory::GetInstance()
        ->Get(profile_.get())
        ->SetNotificationUIManagerForTesting(&notification_ui_manager_);
  }

  void InvokeUsbAdded(uint16_t vendor_id,
                      uint16_t product_id,
                      uint8_t interface_class) {
    device::UsbConfigDescriptor config(1, false, false, 0);
    config.interfaces.emplace_back(1, 0, interface_class, 0, 0);
    device_client_.usb_service()->AddDevice(
        new device::MockUsbDevice(vendor_id, product_id, config));
  }

  // Creates a test extension with the provided permissions.
  scoped_refptr<extensions::Extension> CreateTestExtension(
      std::unique_ptr<base::ListValue> permissions_builder,
      std::unique_ptr<base::DictionaryValue> usb_printers_builder) {
    return extensions::ExtensionBuilder()
        .SetID("fake_extension_id")
        .SetManifest(
            DictionaryBuilder()
                .Set("name", "Printer provider extension")
                .Set("manifest_version", 2)
                .Set("version", "1.0")
                // Needed to enable usb API.
                .Set("app",
                     DictionaryBuilder()
                         .Set("background",
                              DictionaryBuilder()
                                  .Set("scripts",
                                       ListBuilder().Append("bg.js").Build())
                                  .Build())
                         .Build())
                .Set("permissions", std::move(permissions_builder))
                .Set("usb_printers", std::move(usb_printers_builder))
                .Build())
        .Build();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  StubNotificationUIManager notification_ui_manager_;
  chromeos::FakeChromeUserManager* user_manager_;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  device::MockDeviceClient device_client_;
  std::unique_ptr<TestingProfile> profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrinterDetectorAppSearchEnabledTest);
};

TEST_F(PrinterDetectorAppSearchEnabledTest, ShowFindAppNotification) {
  InvokeUsbAdded(123, 456, kPrinterInterfaceClass);

  ASSERT_EQ(1u, notification_ui_manager_.GetNotificationCount());
  const Notification& notification =
      notification_ui_manager_.GetNotificationAt(0);
  EXPECT_EQ("123:456", notification.tag());
  EXPECT_EQ(
      base::StringPrintf(kPrinterAppNotFoundDelegateIDTemplate, "123", "456"),
      notification.delegate_id());
}

TEST_F(PrinterDetectorAppSearchEnabledTest, ShowAppFoundNotification) {
  scoped_refptr<extensions::Extension> extension = CreateTestExtension(
      ListBuilder()
          .Append("usb")
          .Append("printerProvider")
          .Append(DictionaryBuilder()
                      .Set("usbDevices", ListBuilder()
                                             .Append(DictionaryBuilder()
                                                         .Set("vendorId", 123)
                                                         .Set("productId", 456)
                                                         .Build())
                                             .Build())
                      .Build())
          .Build(),
      DictionaryBuilder().Set("filters", ListBuilder().Build()).Build());
  ASSERT_TRUE(extensions::ExtensionRegistry::Get(profile_.get())
                  ->AddEnabled(extension));

  InvokeUsbAdded(123, 456, kPrinterInterfaceClass);

  ASSERT_EQ(1u, notification_ui_manager_.GetNotificationCount());
  const Notification& notification =
      notification_ui_manager_.GetNotificationAt(0);
  EXPECT_EQ("123:456", notification.tag());
  EXPECT_EQ(
      base::StringPrintf(kPrinterAppExistsDelegateIDTemplate, "123", "456"),
      notification.delegate_id());
}

TEST_F(PrinterDetectorAppSearchEnabledTest,
       UsbHandlerExists_NotPrinterProvider) {
  scoped_refptr<extensions::Extension> extension = CreateTestExtension(
      ListBuilder()
          .Append("usb")
          .Append(DictionaryBuilder()
                      .Set("usbDevices", ListBuilder()
                                             .Append(DictionaryBuilder()
                                                         .Set("vendorId", 123)
                                                         .Set("productId", 756)
                                                         .Build())
                                             .Build())
                      .Build())
          .Build(),
      DictionaryBuilder().Set("filters", ListBuilder().Build()).Build());
  ASSERT_TRUE(extensions::ExtensionRegistry::Get(profile_.get())
                  ->AddEnabled(extension));

  InvokeUsbAdded(123, 756, kPrinterInterfaceClass);

  ASSERT_EQ(1u, notification_ui_manager_.GetNotificationCount());
  const Notification& notification =
      notification_ui_manager_.GetNotificationAt(0);
  EXPECT_EQ("123:756", notification.tag());
  EXPECT_EQ(
      base::StringPrintf(kPrinterAppNotFoundDelegateIDTemplate, "123", "756"),
      notification.delegate_id());
}

TEST_F(PrinterDetectorAppSearchEnabledTest,
       PrinterProvider_DifferentUsbProductId) {
  scoped_refptr<extensions::Extension> extension = CreateTestExtension(
      ListBuilder()
          .Append("usb")
          .Append("printerProvider")
          .Append(DictionaryBuilder()
                      .Set("usbDevices", ListBuilder()
                                             .Append(DictionaryBuilder()
                                                         .Set("vendorId", 123)
                                                         .Set("productId", 001)
                                                         .Build())
                                             .Build())
                      .Build())
          .Build(),
      DictionaryBuilder().Set("filters", ListBuilder().Build()).Build());
  ASSERT_TRUE(extensions::ExtensionRegistry::Get(profile_.get())
                  ->AddEnabled(extension));

  InvokeUsbAdded(123, 456, kPrinterInterfaceClass);

  ASSERT_EQ(1u, notification_ui_manager_.GetNotificationCount());
  const Notification& notification =
      notification_ui_manager_.GetNotificationAt(0);
  EXPECT_EQ("123:456", notification.tag());
  EXPECT_EQ(
      base::StringPrintf(kPrinterAppNotFoundDelegateIDTemplate, "123", "456"),
      notification.delegate_id());
}

TEST_F(PrinterDetectorAppSearchEnabledTest,
       PrinterProvider_UsbPrinters_NotFound) {
  scoped_refptr<extensions::Extension> extension = CreateTestExtension(
      ListBuilder().Append("usb").Append("printerProvider").Build(),
      DictionaryBuilder()
          .Set("filters", ListBuilder()
                              .Append(DictionaryBuilder()
                                          .Set("vendorId", 123)
                                          .Set("productId", 001)
                                          .Build())
                              .Build())
          .Build());
  ASSERT_TRUE(extensions::ExtensionRegistry::Get(profile_.get())
                  ->AddEnabled(extension));

  InvokeUsbAdded(123, 456, kPrinterInterfaceClass);

  ASSERT_EQ(1u, notification_ui_manager_.GetNotificationCount());
  const Notification& notification =
      notification_ui_manager_.GetNotificationAt(0);
  EXPECT_EQ("123:456", notification.tag());
  EXPECT_EQ(
      base::StringPrintf(kPrinterAppNotFoundDelegateIDTemplate, "123", "456"),
      notification.delegate_id());
}

TEST_F(PrinterDetectorAppSearchEnabledTest,
       PrinterProvider_UsbPrinters_WithProductId) {
  scoped_refptr<extensions::Extension> extension = CreateTestExtension(
      ListBuilder().Append("usb").Append("printerProvider").Build(),
      DictionaryBuilder()
          .Set("filters", ListBuilder()
                              .Append(DictionaryBuilder()
                                          .Set("vendorId", 123)
                                          .Set("productId", 456)
                                          .Build())
                              .Build())
          .Build());
  ASSERT_TRUE(extensions::ExtensionRegistry::Get(profile_.get())
                  ->AddEnabled(extension));

  InvokeUsbAdded(123, 456, kPrinterInterfaceClass);

  ASSERT_EQ(1u, notification_ui_manager_.GetNotificationCount());
  const Notification& notification =
      notification_ui_manager_.GetNotificationAt(0);
  EXPECT_EQ("123:456", notification.tag());
  EXPECT_EQ(
      base::StringPrintf(kPrinterAppExistsDelegateIDTemplate, "123", "456"),
      notification.delegate_id());
}

TEST_F(PrinterDetectorAppSearchEnabledTest,
       PrinterProvider_UsbPrinters_WithInterfaceClass) {
  scoped_refptr<extensions::Extension> extension = CreateTestExtension(
      ListBuilder().Append("usb").Append("printerProvider").Build(),
      DictionaryBuilder()
          .Set("filters",
               ListBuilder()
                   .Append(DictionaryBuilder()
                               .Set("vendorId", 123)
                               .Set("interfaceClass", kPrinterInterfaceClass)
                               .Build())
                   .Build())
          .Build());
  ASSERT_TRUE(extensions::ExtensionRegistry::Get(profile_.get())
                  ->AddEnabled(extension));

  InvokeUsbAdded(123, 456, kPrinterInterfaceClass);

  ASSERT_EQ(1u, notification_ui_manager_.GetNotificationCount());
  const Notification& notification =
      notification_ui_manager_.GetNotificationAt(0);
  EXPECT_EQ("123:456", notification.tag());
  EXPECT_EQ(
      base::StringPrintf(kPrinterAppExistsDelegateIDTemplate, "123", "456"),
      notification.delegate_id());
}

TEST_F(PrinterDetectorAppSearchEnabledTest, IgnoreNonPrinters) {
  scoped_refptr<extensions::Extension> extension = CreateTestExtension(
      ListBuilder().Append("usb").Append("printerProvider").Build(),
      DictionaryBuilder()
          .Set("filters",
               ListBuilder()
                   .Append(DictionaryBuilder()
                               .Set("vendorId", 123)
                               .Set("interfaceClass", kPrinterInterfaceClass)
                               .Build())
                   .Build())
          .Build());
  ASSERT_TRUE(extensions::ExtensionRegistry::Get(profile_.get())
                  ->AddEnabled(extension));

  InvokeUsbAdded(123, 456, 1);

  ASSERT_EQ(0u, notification_ui_manager_.GetNotificationCount());
}

}  // namespace chromeos
