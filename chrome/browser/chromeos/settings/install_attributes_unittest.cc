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

}  // namespace

static const char kTestUser[] = "test@example.com";
static const char kTestUserCanonicalize[] = "UPPER.CASE@example.com";
static const char kTestDomain[] = "example.com";
static const char kTestDeviceId[] = "133750519";

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
      const std::string& user,
      policy::DeviceMode device_mode,
      const std::string& device_id) {
    base::RunLoop loop;
    InstallAttributes::LockResult result;
    install_attributes_->LockDevice(
        user,
        device_mode,
        device_id,
        base::Bind(&CopyLockResult, &loop, &result));
    loop.Run();
    return result;
  }
};

TEST_F(InstallAttributesTest, Lock) {
  EXPECT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(kTestUser,
                                       policy::DEVICE_MODE_ENTERPRISE,
                                       kTestDeviceId));

  // Locking an already locked device should succeed if the parameters match.
  EXPECT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(kTestUser,
                                       policy::DEVICE_MODE_ENTERPRISE,
                                       kTestDeviceId));

  // Another user from the same domain should also succeed.
  EXPECT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult("test1@example.com",
                                       policy::DEVICE_MODE_ENTERPRISE,
                                       kTestDeviceId));

  // But another domain should fail.
  EXPECT_EQ(InstallAttributes::LOCK_WRONG_DOMAIN,
            LockDeviceAndWaitForResult("test@bluebears.com",
                                       policy::DEVICE_MODE_ENTERPRISE,
                                       kTestDeviceId));

  // A non-matching mode should fail as well.
  EXPECT_EQ(InstallAttributes::LOCK_WRONG_MODE,
            LockDeviceAndWaitForResult(kTestUser, policy::DEVICE_MODE_CONSUMER,
                                       kTestDeviceId));
}

TEST_F(InstallAttributesTest, LockCanonicalize) {
  EXPECT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(
                kTestUserCanonicalize,
                policy::DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));
  EXPECT_EQ(gaia::CanonicalizeEmail(kTestUserCanonicalize),
            install_attributes_->GetRegistrationUser());
}

TEST_F(InstallAttributesTest, IsEnterpriseDevice) {
  install_attributes_->Init(GetTempPath());
  EXPECT_FALSE(install_attributes_->IsEnterpriseDevice());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(
                kTestUser,
                policy::DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));
  EXPECT_TRUE(install_attributes_->IsEnterpriseDevice());
}

TEST_F(InstallAttributesTest, GetDomain) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(
                kTestUser,
                policy::DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));
  EXPECT_EQ(kTestDomain, install_attributes_->GetDomain());
}

TEST_F(InstallAttributesTest, GetRegistrationUser) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(std::string(), install_attributes_->GetRegistrationUser());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(
                kTestUser,
                policy::DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));
  EXPECT_EQ(kTestUser, install_attributes_->GetRegistrationUser());
}

TEST_F(InstallAttributesTest, GetDeviceId) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(
                kTestUser,
                policy::DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));
  EXPECT_EQ(kTestDeviceId, install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, GetMode) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(kTestUser,
                                       policy::DEVICE_MODE_ENTERPRISE,
                                       kTestDeviceId));
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
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
}

TEST_F(InstallAttributesTest, ConsumerKioskDevice) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  // Lock the attributes for consumer kiosk.
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(
                std::string(),
                policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,
                std::string()));

  ASSERT_FALSE(cryptohome_util::InstallAttributesIsFirstInstall());
  EXPECT_EQ(policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,
            install_attributes_->GetMode());
  ASSERT_TRUE(install_attributes_->IsConsumerKioskDeviceWithAutoLaunch());
}

TEST_F(InstallAttributesTest, DeviceLockedFromOlderVersion) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  // Lock the attributes as if it was done from older Chrome version.
  ASSERT_TRUE(cryptohome_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseOwned, "true"));
  ASSERT_TRUE(cryptohome_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseUser, kTestUser));
  ASSERT_TRUE(cryptohome_util::InstallAttributesFinalize());
  base::RunLoop loop;
  install_attributes_->ReadImmutableAttributes(loop.QuitClosure());
  loop.Run();

  ASSERT_FALSE(cryptohome_util::InstallAttributesIsFirstInstall());
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_->GetDomain());
  EXPECT_EQ(kTestUser, install_attributes_->GetRegistrationUser());
  EXPECT_EQ("", install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, Init) {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  SetAttribute(&install_attrs_proto,
               InstallAttributes::kAttrEnterpriseOwned, "true");
  SetAttribute(&install_attrs_proto,
               InstallAttributes::kAttrEnterpriseUser, kTestUser);
  const std::string blob(install_attrs_proto.SerializeAsString());
  ASSERT_EQ(static_cast<int>(blob.size()),
            base::WriteFile(GetTempPath(), blob.c_str(), blob.size()));
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_->GetDomain());
  EXPECT_EQ(kTestUser, install_attributes_->GetRegistrationUser());
  EXPECT_EQ("", install_attributes_->GetDeviceId());
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
  EXPECT_EQ("", install_attributes_->GetDomain());
  EXPECT_EQ("", install_attributes_->GetRegistrationUser());
  EXPECT_EQ("", install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, VerifyFakeInstallAttributesCache) {
  // This test verifies that FakeCryptohomeClient::InstallAttributesFinalize
  // writes a cache that InstallAttributes::Init accepts.

  // Verify that no attributes are initially set.
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ("", install_attributes_->GetRegistrationUser());

  // Write test values.
  ASSERT_TRUE(cryptohome_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseOwned, "true"));
  ASSERT_TRUE(cryptohome_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseUser, kTestUser));
  ASSERT_TRUE(cryptohome_util::InstallAttributesFinalize());

  // Verify that InstallAttributes correctly decodes the stub cache file.
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(kTestUser, install_attributes_->GetRegistrationUser());
}

}  // namespace chromeos
