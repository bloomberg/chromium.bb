// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"

#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_image_burner_client.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace policy {

DevicePolicyCrosBrowserTest::DevicePolicyCrosBrowserTest()
    : mock_dbus_thread_manager_(new chromeos::MockDBusThreadManager) {
}

DevicePolicyCrosBrowserTest::~DevicePolicyCrosBrowserTest() {
}

void DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture() {
  EXPECT_CALL(*mock_dbus_thread_manager_, GetSessionManagerClient())
      .WillRepeatedly(Return(&session_manager_client_));

  SetMockDBusThreadManagerExpectations();
  chromeos::DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager_);
  CrosInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
}

void DevicePolicyCrosBrowserTest::SetMockDBusThreadManagerExpectations() {
  // TODO(satorux): MockImageBurnerClient seems unnecessary. Remove it?
  EXPECT_CALL(*mock_dbus_thread_manager_->mock_image_burner_client(),
              ResetEventHandlers())
      .Times(AnyNumber());
  EXPECT_CALL(*mock_dbus_thread_manager_->mock_image_burner_client(),
              SetEventHandlers(_, _))
      .Times(AnyNumber());
}

void DevicePolicyCrosBrowserTest::InstallOwnerKey() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath owner_key_file = temp_dir_.path().AppendASCII("owner.key");
  std::vector<uint8> owner_key_bits;
  ASSERT_TRUE(device_policy()->signing_key()->ExportPublicKey(&owner_key_bits));
  ASSERT_EQ(
      file_util::WriteFile(
          owner_key_file,
          reinterpret_cast<const char*>(vector_as_array(&owner_key_bits)),
          owner_key_bits.size()),
      static_cast<int>(owner_key_bits.size()));
  ASSERT_TRUE(PathService::Override(chromeos::FILE_OWNER_KEY, owner_key_file));
}

void DevicePolicyCrosBrowserTest::RefreshDevicePolicy() {
  // Reset the key to its original state.
  device_policy_.set_signing_key(PolicyBuilder::CreateTestSigningKey());
  device_policy_.Build();
  // The local instance of the private half of the owner key must be dropped
  // as otherwise the NSS library will tell Chrome that the key is available -
  // which is incorrect and leads to Chrome behaving as if a local owner were
  // logged in.
  device_policy_.set_signing_key(
      make_scoped_ptr<crypto::RSAPrivateKey>(NULL));
  device_policy_.set_new_signing_key(
      make_scoped_ptr<crypto::RSAPrivateKey>(NULL));
  session_manager_client_.set_device_policy(device_policy_.GetBlob());
  session_manager_client_.OnPropertyChangeComplete(true);
}

void DevicePolicyCrosBrowserTest::TearDownInProcessBrowserTestFixture() {
  CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  chromeos::DBusThreadManager::Shutdown();
}

}  // namespace policy
