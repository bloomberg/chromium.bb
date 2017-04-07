// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/install_attributes.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/policy/proto/install_attributes.pb.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

void CopyLockResult(base::RunLoop* loop,
                    InstallAttributes::LockResult* out,
                    InstallAttributes::LockResult result) {
  *out = result;
  loop->Quit();
}

void OnSetBlockDevmode(chromeos::DBusMethodCallStatus* out_status,
                       chromeos::DBusMethodCallStatus call_status,
                       bool result,
                       const cryptohome::BaseReply& reply) {
  *out_status = call_status;
}

}  // namespace

static const char kTestDomain[] = "example.com";
static const char kTestRealm[] = "realm.example.com";
static const char kTestDeviceId[] = "133750519";
static const char kTestUserDeprecated[] = "test@example.com";

class InstallAttributesTest : public testing::Test {
 protected:
  InstallAttributesTest() {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(PathService::OverrideAndCreateIfNeeded(
        FILE_INSTALL_ATTRIBUTES, GetTempPath(), true, false));
    DBusThreadManager::Initialize();
    install_attributes_ = base::MakeUnique<InstallAttributes>(
        DBusThreadManager::Get()->GetCryptohomeClient());
  }

  void TearDown() override { DBusThreadManager::Shutdown(); }

  base::FilePath GetTempPath() const {
    base::FilePath temp_path = base::MakeAbsoluteFilePath(temp_dir_.GetPath());
    return temp_path.Append("install_attrs_test");
  }

  void SetAttribute(
      cryptohome::SerializedInstallAttributes* install_attrs_proto,
      const std::string& name,
      const std::string& value) {
    cryptohome::SerializedInstallAttributes::Attribute* attribute;
    attribute = install_attrs_proto->add_attributes();
    attribute->set_name(name);
    attribute->set_value(value);
  }

  base::MessageLoopForUI message_loop_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<InstallAttributes> install_attributes_;

  InstallAttributes::LockResult LockDeviceAndWaitForResult(
      policy::DeviceMode device_mode,
      const std::string& domain,
      const std::string& realm,
      const std::string& device_id) {
    base::RunLoop loop;
    InstallAttributes::LockResult result;
    install_attributes_->LockDevice(
        device_mode,
        domain,
        realm,
        device_id,
        base::Bind(&CopyLockResult, &loop, &result));
    loop.Run();
    return result;
  }
};

TEST_F(InstallAttributesTest, Lock) {
  EXPECT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE,
                                       kTestDomain,
                                       std::string(),  // realm
                                       kTestDeviceId));

  // Locking an already locked device should succeed if the parameters match.
  EXPECT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE,
                                       kTestDomain,
                                       std::string(),  // realm
                                       kTestDeviceId));

  // But another domain should fail.
  EXPECT_EQ(InstallAttributes::LOCK_WRONG_DOMAIN,
            LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE,
                                       "anotherexample.com",
                                       std::string(),  // realm
                                       kTestDeviceId));

  // A non-matching mode should fail as well.
  EXPECT_EQ(InstallAttributes::LOCK_WRONG_MODE,
            LockDeviceAndWaitForResult(
                policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,
                std::string(),    // domain
                std::string(),    // realm
                std::string()));  // device id
}

TEST_F(InstallAttributesTest, IsEnterpriseManagedCloud) {
  install_attributes_->Init(GetTempPath());
  EXPECT_FALSE(install_attributes_->IsEnterpriseManaged());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(
                policy::DEVICE_MODE_ENTERPRISE,
                kTestDomain,
                std::string(),  // realm
                kTestDeviceId));
  EXPECT_TRUE(install_attributes_->IsEnterpriseManaged());
  EXPECT_TRUE(install_attributes_->IsCloudManaged());
  EXPECT_FALSE(install_attributes_->IsActiveDirectoryManaged());
}

TEST_F(InstallAttributesTest, IsEnterpriseManagedRealm) {
  install_attributes_->Init(GetTempPath());
  EXPECT_FALSE(install_attributes_->IsEnterpriseManaged());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(
                policy::DEVICE_MODE_ENTERPRISE_AD,
                std::string(),  // domain
                kTestRealm,
                kTestDeviceId));
  EXPECT_TRUE(install_attributes_->IsEnterpriseManaged());
  EXPECT_FALSE(install_attributes_->IsCloudManaged());
  EXPECT_TRUE(install_attributes_->IsActiveDirectoryManaged());
}

TEST_F(InstallAttributesTest, GettersCloud) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(
                policy::DEVICE_MODE_ENTERPRISE,
                kTestDomain,
                std::string(),  // realm
                kTestDeviceId));
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(kTestDeviceId, install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, GettersAD) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(
                policy::DEVICE_MODE_ENTERPRISE_AD,
                std::string(),  // domain
                kTestRealm,
                kTestDeviceId));
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE_AD, install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(kTestRealm, install_attributes_->GetRealm());
  EXPECT_EQ(kTestDeviceId, install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, ConsumerDevice) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  // Lock the attributes empty.
  ASSERT_TRUE(cryptohome_util::InstallAttributesFinalize());
  base::RunLoop loop;
  install_attributes_->ReadImmutableAttributes(loop.QuitClosure());
  loop.Run();

  ASSERT_FALSE(cryptohome_util::InstallAttributesIsFirstInstall());
  EXPECT_EQ(policy::DEVICE_MODE_CONSUMER, install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, ConsumerKioskDevice) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  // Lock the attributes for consumer kiosk.
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(
                policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,
                std::string(),
                std::string(),
                std::string()));

  ASSERT_FALSE(cryptohome_util::InstallAttributesIsFirstInstall());
  EXPECT_EQ(policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,
            install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
  ASSERT_TRUE(install_attributes_->IsConsumerKioskDeviceWithAutoLaunch());
}

TEST_F(InstallAttributesTest, DeviceLockedFromOlderVersion) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  // Lock the attributes as if it was done from older Chrome version.
  ASSERT_TRUE(cryptohome_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseOwned, "true"));
  ASSERT_TRUE(cryptohome_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseUser, kTestUserDeprecated));
  ASSERT_TRUE(cryptohome_util::InstallAttributesFinalize());
  base::RunLoop loop;
  install_attributes_->ReadImmutableAttributes(loop.QuitClosure());
  loop.Run();

  ASSERT_FALSE(cryptohome_util::InstallAttributesIsFirstInstall());
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, Init) {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  SetAttribute(&install_attrs_proto,
               InstallAttributes::kAttrEnterpriseOwned, "true");
  SetAttribute(&install_attrs_proto,
               InstallAttributes::kAttrEnterpriseUser, kTestUserDeprecated);
  const std::string blob(install_attrs_proto.SerializeAsString());
  ASSERT_EQ(static_cast<int>(blob.size()),
            base::WriteFile(GetTempPath(), blob.c_str(), blob.size()));
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, InitForConsumerKiosk) {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  SetAttribute(&install_attrs_proto,
               InstallAttributes::kAttrConsumerKioskEnabled, "true");
  const std::string blob(install_attrs_proto.SerializeAsString());
  ASSERT_EQ(static_cast<int>(blob.size()),
            base::WriteFile(GetTempPath(), blob.c_str(), blob.size()));
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,
            install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, VerifyFakeInstallAttributesCache) {
  // This test verifies that FakeCryptohomeClient::InstallAttributesFinalize
  // writes a cache that InstallAttributes::Init accepts.

  // Verify that no attributes are initially set.
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());

  // Write test values.
  ASSERT_TRUE(cryptohome_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseOwned, "true"));
  ASSERT_TRUE(cryptohome_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseUser, kTestUserDeprecated));
  ASSERT_TRUE(cryptohome_util::InstallAttributesFinalize());

  // Verify that InstallAttributes correctly decodes the stub cache file.
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, CheckSetBlockDevmodeInTpm) {
  chromeos::DBusMethodCallStatus status =
      chromeos::DBusMethodCallStatus::DBUS_METHOD_CALL_FAILURE;
  install_attributes_->SetBlockDevmodeInTpm(
      true, base::Bind(&OnSetBlockDevmode, &status));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(chromeos::DBusMethodCallStatus::DBUS_METHOD_CALL_SUCCESS, status);
}

}  // namespace chromeos
