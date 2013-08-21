// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"

#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/policy/cros_enterprise_test_utils.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/policy/proto/chromeos/chrome_device_policy.pb.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager_without_gmock.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace policy {

void DevicePolicyCrosBrowserTest::MarkAsEnterpriseOwned() {
  ASSERT_TRUE(temp_dir_.IsValid());
  test_utils::MarkAsEnterpriseOwned(DevicePolicyBuilder::kFakeUsername,
                                    temp_dir_.path());
}

DevicePolicyCrosBrowserTest::DevicePolicyCrosBrowserTest()
    : mock_dbus_thread_manager_(
        new chromeos::MockDBusThreadManagerWithoutGMock) {
}

DevicePolicyCrosBrowserTest::~DevicePolicyCrosBrowserTest() {
}

void DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture() {
  chromeos::DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager_);
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
}

void DevicePolicyCrosBrowserTest::InstallOwnerKey() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath owner_key_file = temp_dir_.path().AppendASCII("owner.key");
  std::vector<uint8> owner_key_bits;
  ASSERT_TRUE(
      device_policy()->GetSigningKey()->ExportPublicKey(&owner_key_bits));
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
  device_policy_.SetDefaultSigningKey();
  device_policy_.Build();
  session_manager_client()->set_device_policy(device_policy_.GetBlob());
  session_manager_client()->OnPropertyChangeComplete(true);
}

void DevicePolicyCrosBrowserTest::TearDownInProcessBrowserTestFixture() {
  InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  chromeos::DBusThreadManager::Shutdown();
}

}  // namespace policy
