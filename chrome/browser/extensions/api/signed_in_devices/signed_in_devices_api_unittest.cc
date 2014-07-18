// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <vector>

#include "base/guid.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/signed_in_devices/signed_in_devices_api.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/pref_names.h"
#include "extensions/common/extension.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::DeviceInfo;
using testing::Return;

namespace extensions {

TEST(SignedInDevicesAPITest, GetSignedInDevices) {
  TestingProfile profile;
  ProfileSyncServiceMock pss_mock(&profile);
  base::MessageLoop message_loop_;
  TestExtensionPrefs extension_prefs(
      message_loop_.message_loop_proxy().get());

  // Add a couple of devices and make sure we get back public ids for them.
  std::string extension_name = "test";
  scoped_refptr<Extension> extension_test =
      extension_prefs.AddExtension(extension_name);

  DeviceInfo device_info1(base::GenerateGUID(),
                          "abc Device",
                          "XYZ v1",
                          "XYZ SyncAgent v1",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                          "device_id");

  DeviceInfo device_info2(base::GenerateGUID(),
                          "def Device",
                          "XYZ v2",
                          "XYZ SyncAgent v2",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                          "device_id");

  std::vector<DeviceInfo*> devices;
  devices.push_back(&device_info1);
  devices.push_back(&device_info2);

  EXPECT_CALL(pss_mock, GetAllSignedInDevicesMock()).
              WillOnce(Return(&devices));

  ScopedVector<DeviceInfo> output1 = GetAllSignedInDevices(
      extension_test.get()->id(),
      &pss_mock,
      extension_prefs.prefs());

  std::string public_id1 = device_info1.public_id();
  std::string public_id2 = device_info2.public_id();

  EXPECT_FALSE(public_id1.empty());
  EXPECT_FALSE(public_id2.empty());
  EXPECT_NE(public_id1, public_id2);

  // Now clear output1 so its destructor will not destroy the pointers for
  // |device_info1| and |device_info2|.
  output1.weak_clear();

  // Add a third device and make sure the first 2 ids are retained and a new
  // id is generated for the third device.
  DeviceInfo device_info3(base::GenerateGUID(),
                          "def Device",
                          "jkl v2",
                          "XYZ SyncAgent v2",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                          "device_id");

  devices.push_back(&device_info3);

  EXPECT_CALL(pss_mock, GetAllSignedInDevicesMock()).
              WillOnce(Return(&devices));

  ScopedVector<DeviceInfo> output2 = GetAllSignedInDevices(
      extension_test.get()->id(),
      &pss_mock,
      extension_prefs.prefs());

  EXPECT_EQ(device_info1.public_id(), public_id1);
  EXPECT_EQ(device_info2.public_id(), public_id2);

  std::string public_id3 = device_info3.public_id();
  EXPECT_FALSE(public_id3.empty());
  EXPECT_NE(public_id3, public_id1);
  EXPECT_NE(public_id3, public_id2);

  // Now clear output2 so that its destructor does not destroy the
  // |DeviceInfo| pointers.
  output2.weak_clear();
}

class ProfileSyncServiceMockForExtensionTests:
    public ProfileSyncServiceMock {
 public:
  explicit ProfileSyncServiceMockForExtensionTests(Profile* p)
      : ProfileSyncServiceMock(p) {}
  ~ProfileSyncServiceMockForExtensionTests() {}

  MOCK_METHOD0(Shutdown, void());
};

KeyedService* CreateProfileSyncServiceMock(content::BrowserContext* profile) {
  return new ProfileSyncServiceMockForExtensionTests(
      Profile::FromBrowserContext(profile));
}

class ExtensionSignedInDevicesTest : public ExtensionApiUnittest {
 public:
  virtual void SetUp() {
    ExtensionApiUnittest::SetUp();

    ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(
        profile(), CreateProfileSyncServiceMock);
  }
};

DeviceInfo* CreateDeviceInfo(const DeviceInfo& device_info) {
  return new DeviceInfo(device_info.guid(),
                        device_info.client_name(),
                        device_info.chrome_version(),
                        device_info.sync_user_agent(),
                        device_info.device_type(),
                        device_info.signin_scoped_device_id());
}

std::string GetPublicId(const base::DictionaryValue* dictionary) {
  std::string public_id;
  if (!dictionary->GetString("id", &public_id)) {
    ADD_FAILURE() << "Not able to find public id in the dictionary";
  }

  return public_id;
}

void VerifyDictionaryWithDeviceInfo(const base::DictionaryValue* actual_value,
                                    DeviceInfo* device_info) {
  std::string public_id = GetPublicId(actual_value);
  device_info->set_public_id(public_id);

  scoped_ptr<base::DictionaryValue> expected_value(device_info->ToValue());
  EXPECT_TRUE(expected_value->Equals(actual_value));
}

base::DictionaryValue* GetDictionaryFromList(int index,
                                             base::ListValue* value) {
  base::DictionaryValue* dictionary;
  if (!value->GetDictionary(index, &dictionary)) {
    ADD_FAILURE() << "Expected a list of dictionaries";
    return NULL;
  }
  return dictionary;
}

TEST_F(ExtensionSignedInDevicesTest, GetAll) {
  ProfileSyncServiceMockForExtensionTests* pss_mock =
      static_cast<ProfileSyncServiceMockForExtensionTests*>(
          ProfileSyncServiceFactory::GetForProfile(profile()));

  DeviceInfo device_info1(base::GenerateGUID(),
                          "abc Device",
                          "XYZ v1",
                          "XYZ SyncAgent v1",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                          "device_id");

  DeviceInfo device_info2(base::GenerateGUID(),
                          "def Device",
                          "XYZ v2",
                          "XYZ SyncAgent v2",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                          "device_id");

  std::vector<DeviceInfo*> devices;
  devices.push_back(CreateDeviceInfo(device_info1));
  devices.push_back(CreateDeviceInfo(device_info2));

  EXPECT_CALL(*pss_mock, GetAllSignedInDevicesMock()).
              WillOnce(Return(&devices));

  EXPECT_CALL(*pss_mock, Shutdown());

  scoped_ptr<base::ListValue> result(RunFunctionAndReturnList(
      new SignedInDevicesGetFunction(), "[null]"));

  // Ensure dictionary matches device info.
  VerifyDictionaryWithDeviceInfo(GetDictionaryFromList(0, result.get()),
                                 &device_info1);
  VerifyDictionaryWithDeviceInfo(GetDictionaryFromList(1, result.get()),
                                 &device_info2);

  // Ensure public ids are set and unique.
  std::string public_id1 = GetPublicId(GetDictionaryFromList(0, result.get()));
  std::string public_id2 = GetPublicId(GetDictionaryFromList(1, result.get()));

  EXPECT_FALSE(public_id1.empty());
  EXPECT_FALSE(public_id2.empty());
  EXPECT_NE(public_id1, public_id2);
}

}  // namespace extensions
