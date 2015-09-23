// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_webusb_browser_client.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/webusb/webusb_detector.h"
#include "device/core/device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "url/gurl.h"

namespace {

// usb device product name
const char* kProductName_1 = "Google Product A";
const char* kProductName_2 = "Google Product B";
const char* kProductName_3 = "Google Product C";

// usb device landing page
const char* kLandingPage_1 = "https://www.google.com/A";
const char* kLandingPage_2 = "https://www.google.com/B";
const char* kLandingPage_3 = "https://www.google.com/C";

class TestDeviceClient : public device::DeviceClient {
 public:
  TestDeviceClient() : device::DeviceClient() {}
  ~TestDeviceClient() override {}

  device::MockUsbService& mock_usb_service() { return usb_service_; }

 private:
  device::UsbService* GetUsbService() override { return &usb_service_; }

  device::MockUsbService usb_service_;
};

}  // namespace

class ChromeWebUsbBrowserClientTest : public testing::Test {
 public:
  ChromeWebUsbBrowserClientTest() {}

  ~ChromeWebUsbBrowserClientTest() override = default;

  void SetUp() override { message_center::MessageCenter::Initialize(); }

  void TearDown() override { message_center::MessageCenter::Shutdown(); }

 protected:
  TestDeviceClient device_client_;
  ChromeWebUsbBrowserClient chrome_webusb_browser_client_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebUsbBrowserClientTest);
};

TEST_F(ChromeWebUsbBrowserClientTest, UsbDeviceAddedAndRemoved) {
  GURL landing_page(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page));
  std::string guid = device->guid();

  webusb::WebUsbDetector webusb_detector(&chrome_webusb_browser_client_);

  device_client_.mock_usb_service().AddDevice(device);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center != nullptr);

  message_center::Notification* notification =
      message_center->FindVisibleNotificationById(guid);
  ASSERT_TRUE(notification != nullptr);

  base::string16 expected_title =
      base::ASCIIToUTF16("Google Product A detected");
  EXPECT_EQ(expected_title, notification->title());

  base::string16 expected_message =
      base::ASCIIToUTF16("Go to www.google.com/A to connect.");
  EXPECT_EQ(expected_message, notification->message());

  EXPECT_TRUE(notification->delegate() != nullptr);

  device_client_.mock_usb_service().RemoveDevice(device);

  // device is removed, so notification should be removed from the
  // message_center too
  EXPECT_EQ(nullptr, message_center->FindVisibleNotificationById(guid));
}

TEST_F(ChromeWebUsbBrowserClientTest,
       UsbDeviceWithoutProductNameAddedAndRemoved) {
  std::string product_name = "";
  GURL landing_page(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", product_name, "002", landing_page));
  std::string guid = device->guid();

  webusb::WebUsbDetector webusb_detector(&chrome_webusb_browser_client_);

  device_client_.mock_usb_service().AddDevice(device);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center != nullptr);

  // for device without product name, no notification is generated
  EXPECT_EQ(nullptr, message_center->FindVisibleNotificationById(guid));

  device_client_.mock_usb_service().RemoveDevice(device);

  EXPECT_EQ(nullptr, message_center->FindVisibleNotificationById(guid));
}

TEST_F(ChromeWebUsbBrowserClientTest,
       UsbDeviceWithoutLandingPageAddedAndRemoved) {
  GURL landing_page("");
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page));
  std::string guid = device->guid();

  webusb::WebUsbDetector webusb_detector(&chrome_webusb_browser_client_);

  device_client_.mock_usb_service().AddDevice(device);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center != nullptr);

  // for device without landing page, no notification is generated
  EXPECT_EQ(nullptr, message_center->FindVisibleNotificationById(guid));

  device_client_.mock_usb_service().RemoveDevice(device);

  EXPECT_EQ(nullptr, message_center->FindVisibleNotificationById(guid));
}

TEST_F(ChromeWebUsbBrowserClientTest, UsbDeviceWasThereBeforeAndThenRemoved) {
  GURL landing_page(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page));
  std::string guid = device->guid();

  webusb::WebUsbDetector webusb_detector(&chrome_webusb_browser_client_);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center != nullptr);

  EXPECT_EQ(nullptr, message_center->FindVisibleNotificationById(guid));

  device_client_.mock_usb_service().RemoveDevice(device);

  EXPECT_EQ(nullptr, message_center->FindVisibleNotificationById(guid));
}

TEST_F(ChromeWebUsbBrowserClientTest, ThreeUsbDevicesAddedAndRemoved) {
  base::string16 product_name_1 = base::UTF8ToUTF16(kProductName_1);
  GURL landing_page_1(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device_1(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page_1));
  std::string guid_1 = device_1->guid();

  base::string16 product_name_2 = base::UTF8ToUTF16(kProductName_2);
  GURL landing_page_2(kLandingPage_2);
  scoped_refptr<device::MockUsbDevice> device_2(new device::MockUsbDevice(
      3, 4, "Google", kProductName_2, "005", landing_page_2));
  std::string guid_2 = device_2->guid();

  base::string16 product_name_3 = base::UTF8ToUTF16(kProductName_3);
  GURL landing_page_3(kLandingPage_3);
  scoped_refptr<device::MockUsbDevice> device_3(new device::MockUsbDevice(
      6, 7, "Google", kProductName_3, "008", landing_page_3));
  std::string guid_3 = device_3->guid();

  webusb::WebUsbDetector webusb_detector(&chrome_webusb_browser_client_);

  device_client_.mock_usb_service().AddDevice(device_1);
  device_client_.mock_usb_service().AddDevice(device_2);
  device_client_.mock_usb_service().AddDevice(device_3);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  ASSERT_TRUE(message_center != nullptr);

  message_center::Notification* notification_1 =
      message_center->FindVisibleNotificationById(guid_1);
  ASSERT_TRUE(notification_1 != nullptr);
  base::string16 expected_title_1 =
      base::ASCIIToUTF16("Google Product A detected");
  EXPECT_EQ(expected_title_1, notification_1->title());
  base::string16 expected_message_1 =
      base::ASCIIToUTF16("Go to www.google.com/A to connect.");
  EXPECT_EQ(expected_message_1, notification_1->message());

  message_center::Notification* notification_2 =
      message_center->FindVisibleNotificationById(guid_2);
  ASSERT_TRUE(notification_2 != nullptr);
  base::string16 expected_title_2 =
      base::ASCIIToUTF16("Google Product B detected");
  EXPECT_EQ(expected_title_2, notification_2->title());
  base::string16 expected_message_2 =
      base::ASCIIToUTF16("Go to www.google.com/B to connect.");
  EXPECT_EQ(expected_message_2, notification_2->message());

  message_center::Notification* notification_3 =
      message_center->FindVisibleNotificationById(guid_3);
  ASSERT_TRUE(notification_3 != nullptr);
  base::string16 expected_title_3 =
      base::ASCIIToUTF16("Google Product C detected");
  EXPECT_EQ(expected_title_3, notification_3->title());
  base::string16 expected_message_3 =
      base::ASCIIToUTF16("Go to www.google.com/C to connect.");
  EXPECT_EQ(expected_message_3, notification_3->message());

  EXPECT_TRUE(notification_1->delegate() != nullptr);
  EXPECT_TRUE(notification_2->delegate() != nullptr);
  EXPECT_TRUE(notification_3->delegate() != nullptr);

  device_client_.mock_usb_service().RemoveDevice(device_1);
  device_client_.mock_usb_service().RemoveDevice(device_2);
  device_client_.mock_usb_service().RemoveDevice(device_3);

  // devices are removed, so notifications should be removed from the
  // message_center too
  EXPECT_EQ(nullptr, message_center->FindVisibleNotificationById(guid_1));
  EXPECT_EQ(nullptr, message_center->FindVisibleNotificationById(guid_2));
  EXPECT_EQ(nullptr, message_center->FindVisibleNotificationById(guid_3));
}
