// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/enterprise_install_attributes.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/policy/proto/install_attributes.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

static const char kTestUser[] = "test@example.com";
static const char kTestDomain[] = "example.com";
static const char kTestDeviceId[] = "133750519";

static const char kAttrEnterpriseOwned[] = "enterprise.owned";
static const char kAttrEnterpriseUser[] = "enterprise.user";

class EnterpriseInstallAttributesTest : public testing::Test {
 protected:
  EnterpriseInstallAttributesTest()
      : cryptohome_(chromeos::CryptohomeLibrary::GetImpl(true)),
        install_attributes_(cryptohome_.get()) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  FilePath GetTempPath() const {
    return temp_dir_.path().Append("install_attrs_test");
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

  base::ScopedTempDir temp_dir_;
  scoped_ptr<chromeos::CryptohomeLibrary> cryptohome_;
  EnterpriseInstallAttributes install_attributes_;
};

TEST_F(EnterpriseInstallAttributesTest, Lock) {
  EXPECT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(
                kTestUser,
                DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));

  EXPECT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(
                kTestUser,
                DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));
  // Another user from the4 same domain should also succeed.
  EXPECT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(
                "test1@example.com",
                DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));
  // But another domain should fail.
  EXPECT_EQ(EnterpriseInstallAttributes::LOCK_WRONG_USER,
            install_attributes_.LockDevice(
                "test@bluebears.com",
                DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));
}

TEST_F(EnterpriseInstallAttributesTest, IsEnterpriseDevice) {
  install_attributes_.ReadCacheFile(GetTempPath());
  EXPECT_FALSE(install_attributes_.IsEnterpriseDevice());
  ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(
                kTestUser,
                DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));
  EXPECT_TRUE(install_attributes_.IsEnterpriseDevice());
}

TEST_F(EnterpriseInstallAttributesTest, GetDomain) {
  install_attributes_.ReadCacheFile(GetTempPath());
  EXPECT_EQ(std::string(), install_attributes_.GetDomain());
  ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(
                kTestUser,
                DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));
  EXPECT_EQ(kTestDomain, install_attributes_.GetDomain());
}

TEST_F(EnterpriseInstallAttributesTest, GetRegistrationUser) {
  install_attributes_.ReadCacheFile(GetTempPath());
  EXPECT_EQ(std::string(), install_attributes_.GetRegistrationUser());
  ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(
                kTestUser,
                DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));
  EXPECT_EQ(kTestUser, install_attributes_.GetRegistrationUser());
}

TEST_F(EnterpriseInstallAttributesTest, GetDeviceId) {
  install_attributes_.ReadCacheFile(GetTempPath());
  EXPECT_EQ(std::string(), install_attributes_.GetDeviceId());
  ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(
                kTestUser,
                DEVICE_MODE_ENTERPRISE,
                kTestDeviceId));
  EXPECT_EQ(kTestDeviceId, install_attributes_.GetDeviceId());
}

TEST_F(EnterpriseInstallAttributesTest, GetMode) {
  install_attributes_.ReadCacheFile(GetTempPath());
  EXPECT_EQ(DEVICE_MODE_PENDING, install_attributes_.GetMode());
  ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(
                kTestUser,
                DEVICE_MODE_KIOSK,
                kTestDeviceId));
  EXPECT_EQ(DEVICE_MODE_KIOSK,
            install_attributes_.GetMode());
}

TEST_F(EnterpriseInstallAttributesTest, ConsumerDevice) {
  install_attributes_.ReadCacheFile(GetTempPath());
  EXPECT_EQ(DEVICE_MODE_PENDING, install_attributes_.GetMode());
  // Lock the attributes empty.
  ASSERT_TRUE(cryptohome_->InstallAttributesFinalize());
  install_attributes_.ReadImmutableAttributes();
  ASSERT_FALSE(cryptohome_->InstallAttributesIsFirstInstall());
  EXPECT_EQ(DEVICE_MODE_CONSUMER, install_attributes_.GetMode());
}

TEST_F(EnterpriseInstallAttributesTest, DeviceLockedFromOlderVersion) {
  install_attributes_.ReadCacheFile(GetTempPath());
  EXPECT_EQ(DEVICE_MODE_PENDING, install_attributes_.GetMode());
  // Lock the attributes as if it was done from older Chrome version.
  ASSERT_TRUE(cryptohome_->InstallAttributesSet(kAttrEnterpriseOwned, "true"));
  ASSERT_TRUE(cryptohome_->InstallAttributesSet(kAttrEnterpriseUser,
                                                kTestUser));
  ASSERT_TRUE(cryptohome_->InstallAttributesFinalize());
  install_attributes_.ReadImmutableAttributes();
  ASSERT_FALSE(cryptohome_->InstallAttributesIsFirstInstall());
  EXPECT_EQ(DEVICE_MODE_ENTERPRISE, install_attributes_.GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_.GetDomain());
  EXPECT_EQ(kTestUser, install_attributes_.GetRegistrationUser());
  EXPECT_EQ("", install_attributes_.GetDeviceId());
}

TEST_F(EnterpriseInstallAttributesTest, ReadCacheFile) {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  SetAttribute(&install_attrs_proto, kAttrEnterpriseOwned, "true");
  SetAttribute(&install_attrs_proto, kAttrEnterpriseUser, kTestUser);
  const std::string blob(install_attrs_proto.SerializeAsString());
  ASSERT_EQ(static_cast<int>(blob.size()),
            file_util::WriteFile(GetTempPath(), blob.c_str(), blob.size()));
  install_attributes_.ReadCacheFile(GetTempPath());
  EXPECT_EQ(DEVICE_MODE_ENTERPRISE, install_attributes_.GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_.GetDomain());
  EXPECT_EQ(kTestUser, install_attributes_.GetRegistrationUser());
  EXPECT_EQ("", install_attributes_.GetDeviceId());
}

}  // namespace policy
