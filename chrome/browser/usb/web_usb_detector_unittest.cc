// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/usb/web_usb_detector.h"
#include "device/base/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "url/gurl.h"

// These tests are disabled because WebUsbDetector::Initialize is a noop on
// Windows due to jank and hangs caused by enumerating devices.
// https://crbug.com/656702
#if !defined(OS_WIN)
namespace {

// USB device product name.
const char* kProductName_1 = "Google Product A";
const char* kProductName_2 = "Google Product B";
const char* kProductName_3 = "Google Product C";

// USB device landing page.
const char* kLandingPage_1 = "https://www.google.com/A";
const char* kLandingPage_2 = "https://www.google.com/B";
const char* kLandingPage_3 = "https://www.google.com/C";

}  // namespace

class WebUsbDetectorTest : public testing::Test {
 public:
  WebUsbDetectorTest() {}
  ~WebUsbDetectorTest() override = default;

  void SetUp() override {
    message_center::MessageCenter::Initialize();
    message_center_ = message_center::MessageCenter::Get();
    ASSERT_TRUE(message_center_ != nullptr);
  }

  void TearDown() override { message_center::MessageCenter::Shutdown(); }

 protected:
  device::MockDeviceClient device_client_;
  message_center::MessageCenter* message_center_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebUsbDetectorTest);
};

TEST_F(WebUsbDetectorTest, UsbDeviceAddedAndRemoved) {
  base::string16 product_name = base::UTF8ToUTF16(kProductName_1);
  GURL landing_page(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page));
  std::string guid = device->guid();

  WebUsbDetector web_usb_detector;
  web_usb_detector.Initialize();

  device_client_.usb_service()->AddDevice(device);
  message_center::Notification* notification =
      message_center_->FindVisibleNotificationById(guid);
  ASSERT_TRUE(notification != nullptr);
  base::string16 expected_title =
      base::ASCIIToUTF16("Google Product A detected");
  EXPECT_EQ(expected_title, notification->title());
  base::string16 expected_message =
      base::ASCIIToUTF16("Go to www.google.com/A to connect.");
  EXPECT_EQ(expected_message, notification->message());
  EXPECT_TRUE(notification->delegate() != nullptr);

  device_client_.usb_service()->RemoveDevice(device);
  // Device is removed, so notification should be removed from the
  // message_center too.
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid));
}

TEST_F(WebUsbDetectorTest, UsbDeviceWithoutProductNameAddedAndRemoved) {
  std::string product_name = "";
  GURL landing_page(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", product_name, "002", landing_page));
  std::string guid = device->guid();

  WebUsbDetector web_usb_detector;
  web_usb_detector.Initialize();

  device_client_.usb_service()->AddDevice(device);
  // For device without product name, no notification is generated.
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid));

  device_client_.usb_service()->RemoveDevice(device);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid));
}

TEST_F(WebUsbDetectorTest, UsbDeviceWithoutLandingPageAddedAndRemoved) {
  GURL landing_page("");
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page));
  std::string guid = device->guid();

  WebUsbDetector web_usb_detector;
  web_usb_detector.Initialize();

  device_client_.usb_service()->AddDevice(device);
  // For device without landing page, no notification is generated.
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid));

  device_client_.usb_service()->RemoveDevice(device);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid));
}

TEST_F(WebUsbDetectorTest, UsbDeviceWasThereBeforeAndThenRemoved) {
  GURL landing_page(kLandingPage_1);
  scoped_refptr<device::MockUsbDevice> device(new device::MockUsbDevice(
      0, 1, "Google", kProductName_1, "002", landing_page));
  std::string guid = device->guid();

  // USB device was added before web_usb_detector was created.
  device_client_.usb_service()->AddDevice(device);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid));

  WebUsbDetector web_usb_detector;
  web_usb_detector.Initialize();

  device_client_.usb_service()->RemoveDevice(device);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid));
}

TEST_F(
    WebUsbDetectorTest,
    ThreeUsbDevicesWereThereBeforeAndThenRemovedBeforeWebUsbDetectorWasCreated) {
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

  // Three usb devices were added and removed before web_usb_detector was
  // created.
  device_client_.usb_service()->AddDevice(device_1);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_1));
  device_client_.usb_service()->AddDevice(device_2);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_2));
  device_client_.usb_service()->AddDevice(device_3);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_3));

  device_client_.usb_service()->RemoveDevice(device_1);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_1));
  device_client_.usb_service()->RemoveDevice(device_2);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_2));
  device_client_.usb_service()->RemoveDevice(device_3);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_3));

  WebUsbDetector web_usb_detector;
  web_usb_detector.Initialize();
}

TEST_F(
    WebUsbDetectorTest,
    ThreeUsbDevicesWereThereBeforeAndThenRemovedAfterWebUsbDetectorWasCreated) {
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

  // Three usb devices were added before web_usb_detector was created.
  device_client_.usb_service()->AddDevice(device_1);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_1));
  device_client_.usb_service()->AddDevice(device_2);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_2));
  device_client_.usb_service()->AddDevice(device_3);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_3));

  WebUsbDetector web_usb_detector;
  web_usb_detector.Initialize();

  device_client_.usb_service()->RemoveDevice(device_1);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_1));
  device_client_.usb_service()->RemoveDevice(device_2);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_2));
  device_client_.usb_service()->RemoveDevice(device_3);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_3));
}

TEST_F(WebUsbDetectorTest,
       TwoUsbDevicesWereThereBeforeAndThenRemovedAndNewUsbDeviceAdded) {
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

  // Two usb devices were added before web_usb_detector was created.
  device_client_.usb_service()->AddDevice(device_1);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_1));
  device_client_.usb_service()->AddDevice(device_3);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_3));

  WebUsbDetector web_usb_detector;
  web_usb_detector.Initialize();

  device_client_.usb_service()->RemoveDevice(device_1);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_1));

  device_client_.usb_service()->AddDevice(device_2);
  message_center::Notification* notification =
      message_center_->FindVisibleNotificationById(guid_2);
  ASSERT_TRUE(notification != nullptr);
  base::string16 expected_title =
      base::ASCIIToUTF16("Google Product B detected");
  EXPECT_EQ(expected_title, notification->title());
  base::string16 expected_message =
      base::ASCIIToUTF16("Go to www.google.com/B to connect.");
  EXPECT_EQ(expected_message, notification->message());
  EXPECT_TRUE(notification->delegate() != nullptr);

  device_client_.usb_service()->RemoveDevice(device_3);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_3));

  device_client_.usb_service()->RemoveDevice(device_2);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_2));
}

TEST_F(WebUsbDetectorTest, ThreeUsbDevicesAddedAndRemoved) {
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

  WebUsbDetector web_usb_detector;
  web_usb_detector.Initialize();

  device_client_.usb_service()->AddDevice(device_1);
  message_center::Notification* notification_1 =
      message_center_->FindVisibleNotificationById(guid_1);
  ASSERT_TRUE(notification_1 != nullptr);
  base::string16 expected_title_1 =
      base::ASCIIToUTF16("Google Product A detected");
  EXPECT_EQ(expected_title_1, notification_1->title());
  base::string16 expected_message_1 =
      base::ASCIIToUTF16("Go to www.google.com/A to connect.");
  EXPECT_EQ(expected_message_1, notification_1->message());
  EXPECT_TRUE(notification_1->delegate() != nullptr);

  device_client_.usb_service()->RemoveDevice(device_1);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_1));

  device_client_.usb_service()->AddDevice(device_2);
  message_center::Notification* notification_2 =
      message_center_->FindVisibleNotificationById(guid_2);
  ASSERT_TRUE(notification_2 != nullptr);
  base::string16 expected_title_2 =
      base::ASCIIToUTF16("Google Product B detected");
  EXPECT_EQ(expected_title_2, notification_2->title());
  base::string16 expected_message_2 =
      base::ASCIIToUTF16("Go to www.google.com/B to connect.");
  EXPECT_EQ(expected_message_2, notification_2->message());
  EXPECT_TRUE(notification_2->delegate() != nullptr);

  device_client_.usb_service()->RemoveDevice(device_2);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_2));

  device_client_.usb_service()->AddDevice(device_3);
  message_center::Notification* notification_3 =
      message_center_->FindVisibleNotificationById(guid_3);
  ASSERT_TRUE(notification_3 != nullptr);
  base::string16 expected_title_3 =
      base::ASCIIToUTF16("Google Product C detected");
  EXPECT_EQ(expected_title_3, notification_3->title());
  base::string16 expected_message_3 =
      base::ASCIIToUTF16("Go to www.google.com/C to connect.");
  EXPECT_EQ(expected_message_3, notification_3->message());
  EXPECT_TRUE(notification_3->delegate() != nullptr);

  device_client_.usb_service()->RemoveDevice(device_3);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_3));
}

TEST_F(WebUsbDetectorTest, ThreeUsbDeviceAddedAndRemovedDifferentOrder) {
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

  WebUsbDetector web_usb_detector;
  web_usb_detector.Initialize();

  device_client_.usb_service()->AddDevice(device_1);
  message_center::Notification* notification_1 =
      message_center_->FindVisibleNotificationById(guid_1);
  ASSERT_TRUE(notification_1 != nullptr);
  base::string16 expected_title_1 =
      base::ASCIIToUTF16("Google Product A detected");
  EXPECT_EQ(expected_title_1, notification_1->title());
  base::string16 expected_message_1 =
      base::ASCIIToUTF16("Go to www.google.com/A to connect.");
  EXPECT_EQ(expected_message_1, notification_1->message());
  EXPECT_TRUE(notification_1->delegate() != nullptr);

  device_client_.usb_service()->AddDevice(device_2);
  message_center::Notification* notification_2 =
      message_center_->FindVisibleNotificationById(guid_2);
  ASSERT_TRUE(notification_2 != nullptr);
  base::string16 expected_title_2 =
      base::ASCIIToUTF16("Google Product B detected");
  EXPECT_EQ(expected_title_2, notification_2->title());
  base::string16 expected_message_2 =
      base::ASCIIToUTF16("Go to www.google.com/B to connect.");
  EXPECT_EQ(expected_message_2, notification_2->message());
  EXPECT_TRUE(notification_2->delegate() != nullptr);

  device_client_.usb_service()->RemoveDevice(device_2);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_2));

  device_client_.usb_service()->AddDevice(device_3);
  message_center::Notification* notification_3 =
      message_center_->FindVisibleNotificationById(guid_3);
  ASSERT_TRUE(notification_3 != nullptr);
  base::string16 expected_title_3 =
      base::ASCIIToUTF16("Google Product C detected");
  EXPECT_EQ(expected_title_3, notification_3->title());
  base::string16 expected_message_3 =
      base::ASCIIToUTF16("Go to www.google.com/C to connect.");
  EXPECT_EQ(expected_message_3, notification_3->message());
  EXPECT_TRUE(notification_3->delegate() != nullptr);

  device_client_.usb_service()->RemoveDevice(device_1);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_1));

  device_client_.usb_service()->RemoveDevice(device_3);
  EXPECT_EQ(nullptr, message_center_->FindVisibleNotificationById(guid_3));
}
#endif  // !OS_WIN
