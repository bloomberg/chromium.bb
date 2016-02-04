// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/core/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"

using device::MockUsbDevice;

class UsbChooserContextTest : public testing::Test {
 public:
  UsbChooserContextTest() {}
  ~UsbChooserContextTest() override {}

 protected:
  Profile* profile() { return &profile_; }

  device::MockDeviceClient device_client_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

TEST_F(UsbChooserContextTest, CheckGrantAndRevokePermission) {
  GURL origin("https://www.google.com");
  scoped_refptr<MockUsbDevice> device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "123ABC");
  device_client_.usb_service()->AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());

  base::DictionaryValue object_dict;
  object_dict.SetString("name", "Gizmo");
  object_dict.SetInteger("vendor-id", 0);
  object_dict.SetInteger("product-id", 0);
  object_dict.SetString("serial-number", "123ABC");

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
  store->GrantDevicePermission(origin, origin, device->guid());
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, device->guid()));
  std::vector<scoped_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  EXPECT_TRUE(object_dict.Equals(objects[0].get()));
  std::vector<scoped_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
  EXPECT_EQ(origin, all_origin_objects[0]->requesting_origin);
  EXPECT_EQ(origin, all_origin_objects[0]->embedding_origin);
  EXPECT_TRUE(object_dict.Equals(&all_origin_objects[0]->object));
  EXPECT_FALSE(all_origin_objects[0]->incognito);

  store->RevokeDevicePermission(origin, origin, device->guid());
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, CheckGrantAndRevokeEphemeralPermission) {
  GURL origin("https://www.google.com");
  scoped_refptr<MockUsbDevice> device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  scoped_refptr<MockUsbDevice> other_device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  device_client_.usb_service()->AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());

  base::DictionaryValue object_dict;
  object_dict.SetString("name", "Gizmo");
  object_dict.SetString("ephemeral-guid", device->guid());

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
  store->GrantDevicePermission(origin, origin, device->guid());
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, device->guid()));
  EXPECT_FALSE(
      store->HasDevicePermission(origin, origin, other_device->guid()));
  std::vector<scoped_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  EXPECT_TRUE(object_dict.Equals(objects[0].get()));
  std::vector<scoped_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
  EXPECT_EQ(origin, all_origin_objects[0]->requesting_origin);
  EXPECT_EQ(origin, all_origin_objects[0]->embedding_origin);
  EXPECT_TRUE(object_dict.Equals(&all_origin_objects[0]->object));
  EXPECT_FALSE(all_origin_objects[0]->incognito);

  store->RevokeDevicePermission(origin, origin, device->guid());
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, DisconnectDeviceWithPermission) {
  GURL origin("https://www.google.com");
  scoped_refptr<MockUsbDevice> device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "123ABC");
  device_client_.usb_service()->AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
  store->GrantDevicePermission(origin, origin, device->guid());
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, device->guid()));
  std::vector<scoped_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  std::vector<scoped_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());

  device_client_.usb_service()->RemoveDevice(device);
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());

  scoped_refptr<MockUsbDevice> reconnected_device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "123ABC");
  device_client_.usb_service()->AddDevice(reconnected_device);
  EXPECT_TRUE(
      store->HasDevicePermission(origin, origin, reconnected_device->guid()));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, DisconnectDeviceWithEphemeralPermission) {
  GURL origin("https://www.google.com");
  scoped_refptr<MockUsbDevice> device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  device_client_.usb_service()->AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
  store->GrantDevicePermission(origin, origin, device->guid());
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, device->guid()));
  std::vector<scoped_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  std::vector<scoped_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());

  device_client_.usb_service()->RemoveDevice(device);
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device->guid()));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());

  scoped_refptr<MockUsbDevice> reconnected_device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  device_client_.usb_service()->AddDevice(reconnected_device);
  EXPECT_FALSE(
      store->HasDevicePermission(origin, origin, reconnected_device->guid()));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, GrantPermissionInIncognito) {
  GURL origin("https://www.google.com");
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());
  UsbChooserContext* incognito_store = UsbChooserContextFactory::GetForProfile(
      profile()->GetOffTheRecordProfile());

  scoped_refptr<MockUsbDevice> device1 =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  scoped_refptr<MockUsbDevice> device2 =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  device_client_.usb_service()->AddDevice(device1);
  device_client_.usb_service()->AddDevice(device2);

  store->GrantDevicePermission(origin, origin, device1->guid());
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, device1->guid()));
  EXPECT_FALSE(
      incognito_store->HasDevicePermission(origin, origin, device1->guid()));

  incognito_store->GrantDevicePermission(origin, origin, device2->guid());
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, device1->guid()));
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device2->guid()));
  EXPECT_FALSE(
      incognito_store->HasDevicePermission(origin, origin, device1->guid()));
  EXPECT_TRUE(
      incognito_store->HasDevicePermission(origin, origin, device2->guid()));

  {
    std::vector<scoped_ptr<base::DictionaryValue>> objects =
        store->GetGrantedObjects(origin, origin);
    EXPECT_EQ(1u, objects.size());
    std::vector<scoped_ptr<ChooserContextBase::Object>> all_origin_objects =
        store->GetAllGrantedObjects();
    ASSERT_EQ(1u, all_origin_objects.size());
    EXPECT_FALSE(all_origin_objects[0]->incognito);
  }
  {
    std::vector<scoped_ptr<base::DictionaryValue>> objects =
        incognito_store->GetGrantedObjects(origin, origin);
    EXPECT_EQ(1u, objects.size());
    std::vector<scoped_ptr<ChooserContextBase::Object>> all_origin_objects =
        incognito_store->GetAllGrantedObjects();
    ASSERT_EQ(1u, all_origin_objects.size());
    EXPECT_TRUE(all_origin_objects[0]->incognito);
  }
}
