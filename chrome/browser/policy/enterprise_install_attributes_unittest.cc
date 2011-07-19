// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/enterprise_install_attributes.h"

#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

static const char kTestUser[] = "test@example.com";

class EnterpriseInstallAttributesTest : public testing::Test {
 protected:
  EnterpriseInstallAttributesTest()
      : cryptohome_(chromeos::CryptohomeLibrary::GetImpl(true)),
        install_attributes_(cryptohome_.get()) {}

  scoped_ptr<chromeos::CryptohomeLibrary> cryptohome_;
  EnterpriseInstallAttributes install_attributes_;
};

TEST_F(EnterpriseInstallAttributesTest, Lock) {
  EXPECT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(kTestUser));

  EXPECT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(kTestUser));
  EXPECT_EQ(EnterpriseInstallAttributes::LOCK_WRONG_USER,
            install_attributes_.LockDevice("test1@example.com"));
}

TEST_F(EnterpriseInstallAttributesTest, IsEnterpriseDevice) {
  EXPECT_FALSE(install_attributes_.IsEnterpriseDevice());
  ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(kTestUser));
  EXPECT_TRUE(install_attributes_.IsEnterpriseDevice());
}

TEST_F(EnterpriseInstallAttributesTest, GetDomain) {
  EXPECT_EQ(std::string(), install_attributes_.GetDomain());
  ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(kTestUser));
  EXPECT_EQ("example.com", install_attributes_.GetDomain());
}

TEST_F(EnterpriseInstallAttributesTest, GetRegistrationUser) {
  EXPECT_EQ(std::string(), install_attributes_.GetRegistrationUser());
  ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(kTestUser));
  EXPECT_EQ(kTestUser, install_attributes_.GetRegistrationUser());
}

}  // namespace policy
