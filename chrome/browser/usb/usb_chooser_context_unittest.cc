// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/base/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/public/mojom/device.mojom.h"

using device::MockUsbDevice;
using device::UsbDevice;

namespace {

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

void ExpectDevicesAndThen(
    const std::set<std::string>& expected_guids,
    base::OnceClosure continuation,
    std::vector<device::mojom::UsbDeviceInfoPtr> results) {
  EXPECT_EQ(expected_guids.size(), results.size());
  std::set<std::string> actual_guids;
  for (size_t i = 0; i < results.size(); ++i)
    actual_guids.insert(results[i]->guid);
  EXPECT_EQ(expected_guids, actual_guids);
  std::move(continuation).Run();
}

}  // namespace

TEST_F(UsbChooserContextTest, CheckGrantAndRevokePermission) {
  GURL origin("https://www.google.com");
  scoped_refptr<UsbDevice> device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "123ABC");
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  DCHECK(device_info);
  device_client_.usb_service()->AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());

  base::DictionaryValue object_dict;
  object_dict.SetString("name", "Gizmo");
  object_dict.SetInteger("vendor-id", 0);
  object_dict.SetInteger("product-id", 0);
  object_dict.SetString("serial-number", "123ABC");

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  store->GrantDevicePermission(origin, origin, *device_info);
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info));
  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(origin, origin);
  ASSERT_EQ(1u, objects.size());
  EXPECT_TRUE(object_dict.Equals(objects[0].get()));
  std::vector<std::unique_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  ASSERT_EQ(1u, all_origin_objects.size());
  EXPECT_EQ(origin, all_origin_objects[0]->requesting_origin);
  EXPECT_EQ(origin, all_origin_objects[0]->embedding_origin);
  EXPECT_TRUE(object_dict.Equals(&all_origin_objects[0]->object));
  EXPECT_FALSE(all_origin_objects[0]->incognito);

  store->RevokeObjectPermission(origin, origin, *objects[0]);
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, CheckGrantAndRevokeEphemeralPermission) {
  GURL origin("https://www.google.com");
  scoped_refptr<UsbDevice> device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  scoped_refptr<UsbDevice> other_device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  DCHECK(device_info);
  auto other_device_info = device::mojom::UsbDeviceInfo::From(*other_device);
  DCHECK(other_device_info);

  device_client_.usb_service()->AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());

  base::DictionaryValue object_dict;
  object_dict.SetString("name", "Gizmo");
  object_dict.SetString("ephemeral-guid", device->guid());

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  store->GrantDevicePermission(origin, origin, *device_info);
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info));
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *other_device_info));
  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  EXPECT_TRUE(object_dict.Equals(objects[0].get()));
  std::vector<std::unique_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
  EXPECT_EQ(origin, all_origin_objects[0]->requesting_origin);
  EXPECT_EQ(origin, all_origin_objects[0]->embedding_origin);
  EXPECT_TRUE(object_dict.Equals(&all_origin_objects[0]->object));
  EXPECT_FALSE(all_origin_objects[0]->incognito);

  store->RevokeObjectPermission(origin, origin, *objects[0]);
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, DisconnectDeviceWithPermission) {
  GURL origin("https://www.google.com");
  scoped_refptr<UsbDevice> device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "123ABC");
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  DCHECK(device_info);

  device_client_.usb_service()->AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());
  {
    // Call GetDevices once to make sure the connection with DeviceManager has
    // been set up, so that it can be notified when device is removed.
    std::set<std::string> guids;
    guids.insert(device->guid());
    base::RunLoop loop;
    store->GetDevices(
        base::BindOnce(&ExpectDevicesAndThen, guids, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  store->GrantDevicePermission(origin, origin, *device_info);
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info));
  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  std::vector<std::unique_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());

  device_client_.usb_service()->RemoveDevice(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());

  scoped_refptr<UsbDevice> reconnected_device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "123ABC");
  auto reconnected_device_info =
      device::mojom::UsbDeviceInfo::From(*reconnected_device);
  DCHECK(reconnected_device_info);

  device_client_.usb_service()->AddDevice(reconnected_device);
  EXPECT_TRUE(
      store->HasDevicePermission(origin, origin, *reconnected_device_info));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, DisconnectDeviceWithEphemeralPermission) {
  GURL origin("https://www.google.com");
  scoped_refptr<UsbDevice> device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  DCHECK(device_info);

  device_client_.usb_service()->AddDevice(device);
  UsbChooserContext* store = UsbChooserContextFactory::GetForProfile(profile());
  {
    // Call GetDevices once to make sure the connection with DeviceManager has
    // been set up, so that it can be notified when device is removed.
    std::set<std::string> guids;
    guids.insert(device->guid());
    base::RunLoop loop;
    store->GetDevices(
        base::BindOnce(&ExpectDevicesAndThen, guids, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  store->GrantDevicePermission(origin, origin, *device_info);
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info));
  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, objects.size());
  std::vector<std::unique_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());

  device_client_.usb_service()->RemoveDevice(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info));
  objects = store->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());

  scoped_refptr<UsbDevice> reconnected_device =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  auto reconnected_device_info =
      device::mojom::UsbDeviceInfo::From(*reconnected_device);
  DCHECK(reconnected_device_info);

  device_client_.usb_service()->AddDevice(reconnected_device);
  EXPECT_FALSE(
      store->HasDevicePermission(origin, origin, *reconnected_device_info));
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

  scoped_refptr<UsbDevice> device1 =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  scoped_refptr<UsbDevice> device2 =
      new MockUsbDevice(0, 0, "Google", "Gizmo", "");
  auto device_info_1 = device::mojom::UsbDeviceInfo::From(*device1);
  DCHECK(device_info_1);
  auto device_info_2 = device::mojom::UsbDeviceInfo::From(*device2);
  DCHECK(device_info_2);
  device_client_.usb_service()->AddDevice(device1);
  device_client_.usb_service()->AddDevice(device2);

  store->GrantDevicePermission(origin, origin, *device_info_1);
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info_1));
  EXPECT_FALSE(
      incognito_store->HasDevicePermission(origin, origin, *device_info_1));

  incognito_store->GrantDevicePermission(origin, origin, *device_info_2);
  EXPECT_TRUE(store->HasDevicePermission(origin, origin, *device_info_1));
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, *device_info_2));
  EXPECT_FALSE(
      incognito_store->HasDevicePermission(origin, origin, *device_info_1));
  EXPECT_TRUE(
      incognito_store->HasDevicePermission(origin, origin, *device_info_2));

  {
    std::vector<std::unique_ptr<base::DictionaryValue>> objects =
        store->GetGrantedObjects(origin, origin);
    EXPECT_EQ(1u, objects.size());
    std::vector<std::unique_ptr<ChooserContextBase::Object>>
        all_origin_objects = store->GetAllGrantedObjects();
    ASSERT_EQ(1u, all_origin_objects.size());
    EXPECT_FALSE(all_origin_objects[0]->incognito);
  }
  {
    std::vector<std::unique_ptr<base::DictionaryValue>> objects =
        incognito_store->GetGrantedObjects(origin, origin);
    EXPECT_EQ(1u, objects.size());
    std::vector<std::unique_ptr<ChooserContextBase::Object>>
        all_origin_objects = incognito_store->GetAllGrantedObjects();
    ASSERT_EQ(1u, all_origin_objects.size());
    EXPECT_TRUE(all_origin_objects[0]->incognito);
  }
}

TEST_F(UsbChooserContextTest, UsbGuardPermission) {
  const GURL kFooOrigin("https://foo.com");
  const GURL kBarOrigin("https://bar.com");
  scoped_refptr<UsbDevice> device =
      base::MakeRefCounted<MockUsbDevice>(0, 0, "Google", "Gizmo", "ABC123");
  scoped_refptr<UsbDevice> ephemeral_device =
      base::MakeRefCounted<MockUsbDevice>(0, 0, "Google", "Gizmo", "");
  device_client_.usb_service()->AddDevice(device);
  device_client_.usb_service()->AddDevice(ephemeral_device);
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  DCHECK(device_info);
  auto ephemeral_device_info =
      device::mojom::UsbDeviceInfo::From(*ephemeral_device);
  DCHECK(ephemeral_device_info);

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(kFooOrigin, kFooOrigin,
                                     CONTENT_SETTINGS_TYPE_USB_GUARD,
                                     std::string(), CONTENT_SETTING_BLOCK);

  auto* store = UsbChooserContextFactory::GetForProfile(profile());
  store->GrantDevicePermission(kFooOrigin, kFooOrigin, *device_info);
  store->GrantDevicePermission(kFooOrigin, kFooOrigin, *ephemeral_device_info);
  store->GrantDevicePermission(kBarOrigin, kBarOrigin, *device_info);
  store->GrantDevicePermission(kBarOrigin, kBarOrigin, *ephemeral_device_info);

  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      store->GetGrantedObjects(kFooOrigin, kFooOrigin);
  EXPECT_EQ(0u, objects.size());

  objects = store->GetGrantedObjects(kBarOrigin, kBarOrigin);
  EXPECT_EQ(2u, objects.size());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> all_origin_objects =
      store->GetAllGrantedObjects();
  for (const auto& object : all_origin_objects) {
    EXPECT_EQ(object->requesting_origin, kBarOrigin);
    EXPECT_EQ(object->embedding_origin, kBarOrigin);
  }
  EXPECT_EQ(2u, all_origin_objects.size());

  EXPECT_FALSE(
      store->HasDevicePermission(kFooOrigin, kFooOrigin, *device_info));
  EXPECT_FALSE(store->HasDevicePermission(kFooOrigin, kFooOrigin,
                                          *ephemeral_device_info));
  EXPECT_TRUE(store->HasDevicePermission(kBarOrigin, kBarOrigin, *device_info));
  EXPECT_TRUE(store->HasDevicePermission(kBarOrigin, kBarOrigin,
                                         *ephemeral_device_info));
}
