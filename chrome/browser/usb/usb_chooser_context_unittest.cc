// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/core/device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"

using device::MockUsbDevice;
using device::UsbDevice;

namespace {

class TestDeviceClient : public device::DeviceClient {
 public:
  TestDeviceClient() {}
  ~TestDeviceClient() override {}

  device::MockUsbService& usb_service() { return usb_service_; }

 private:
  device::UsbService* GetUsbService() override { return &usb_service_; }

  device::MockUsbService usb_service_;
};

}  // namespace

class UsbChooserContextTest : public testing::Test {
 public:
  UsbChooserContextTest() {}
  ~UsbChooserContextTest() override {}

 protected:
  Profile* profile() { return &profile_; }
  device::MockUsbService& usb_service() { return device_client_.usb_service(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestDeviceClient device_client_;
  TestingProfile profile_;
};

TEST_F(UsbChooserContextTest, CheckGrantAndRevokePermission) {
  GURL origin("https://www.google.com");
  scoped_refptr<UsbDevice> device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "123ABC");
  usb_service().AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
  store->GrantDevicePermission(origin, origin, device->guid());
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, device->guid()));
  store->RevokeDevicePermission(origin, origin, device->guid());
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
}

TEST_F(UsbChooserContextTest, CheckGrantAndRevokeEphemeralPermission) {
  GURL origin("https://www.google.com");
  scoped_refptr<UsbDevice> device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  scoped_refptr<UsbDevice> other_device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  usb_service().AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
  store->GrantDevicePermission(origin, origin, device->guid());
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, device->guid()));
  EXPECT_FALSE(
      store->HasDevicePermission(origin, origin, other_device->guid()));
  store->RevokeDevicePermission(origin, origin, device->guid());
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
}

TEST_F(UsbChooserContextTest, DisconnectDeviceWithPermission) {
  GURL origin("https://www.google.com");
  scoped_refptr<UsbDevice> device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "123ABC");
  usb_service().AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
  store->GrantDevicePermission(origin, origin, device->guid());
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, device->guid()));
  usb_service().RemoveDevice(device);
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));

  scoped_refptr<UsbDevice> reconnected_device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "123ABC");
  usb_service().AddDevice(reconnected_device);
  EXPECT_TRUE(
      store->HasDevicePermission(origin, origin, reconnected_device->guid()));
}

TEST_F(UsbChooserContextTest, DisconnectDeviceWithEphemeralPermission) {
  GURL origin("https://www.google.com");
  scoped_refptr<UsbDevice> device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  usb_service().AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
  store->GrantDevicePermission(origin, origin, device->guid());
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, device->guid()));
  usb_service().RemoveDevice(device);
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));

  scoped_refptr<UsbDevice> reconnected_device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  usb_service().AddDevice(reconnected_device);
  EXPECT_FALSE(
      store->HasDevicePermission(origin, origin, reconnected_device->guid()));
}
