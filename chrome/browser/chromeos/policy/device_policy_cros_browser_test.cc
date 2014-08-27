// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace policy {

DevicePolicyCrosTestHelper::DevicePolicyCrosTestHelper() {}

DevicePolicyCrosTestHelper::~DevicePolicyCrosTestHelper() {}

void DevicePolicyCrosTestHelper::MarkAsEnterpriseOwned() {
  OverridePaths();

  const std::string install_attrs_blob(
      EnterpriseInstallAttributes::
          GetEnterpriseOwnedInstallAttributesBlobForTesting(
              device_policy_.policy_data().username()));

  base::FilePath install_attrs_file;
  ASSERT_TRUE(
      PathService::Get(chromeos::FILE_INSTALL_ATTRIBUTES, &install_attrs_file));
  ASSERT_EQ(static_cast<int>(install_attrs_blob.size()),
            base::WriteFile(install_attrs_file,
                            install_attrs_blob.c_str(),
                            install_attrs_blob.size()));
}

void DevicePolicyCrosTestHelper::InstallOwnerKey() {
  OverridePaths();

  base::FilePath owner_key_file;
  ASSERT_TRUE(PathService::Get(chromeos::FILE_OWNER_KEY, &owner_key_file));
  std::vector<uint8> owner_key_bits;
  ASSERT_TRUE(
      device_policy()->GetSigningKey()->ExportPublicKey(&owner_key_bits));
  ASSERT_EQ(base::WriteFile(
          owner_key_file,
          reinterpret_cast<const char*>(vector_as_array(&owner_key_bits)),
          owner_key_bits.size()),
      static_cast<int>(owner_key_bits.size()));
}

void DevicePolicyCrosTestHelper::OverridePaths() {
  // This is usually done by ChromeBrowserMainChromeOS, but some tests
  // use the overridden paths before ChromeBrowserMain starts. Make sure that
  // the paths are overridden before using them.
  base::FilePath user_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  chromeos::RegisterStubPathOverrides(user_data_dir);
}

DevicePolicyCrosBrowserTest::DevicePolicyCrosBrowserTest()
    : fake_session_manager_client_(new chromeos::FakeSessionManagerClient) {
}

DevicePolicyCrosBrowserTest::~DevicePolicyCrosBrowserTest() {
}

void DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture() {
  dbus_setter_ = chromeos::DBusThreadManager::GetSetterForTesting();
  dbus_setter_->SetSessionManagerClient(
      scoped_ptr<chromeos::SessionManagerClient>(fake_session_manager_client_));
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
}

void DevicePolicyCrosBrowserTest::TearDownInProcessBrowserTestFixture() {
  InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
}

void DevicePolicyCrosBrowserTest::MarkAsEnterpriseOwned() {
  test_helper_.MarkAsEnterpriseOwned();
}

void DevicePolicyCrosBrowserTest::InstallOwnerKey() {
  test_helper_.InstallOwnerKey();
}

void DevicePolicyCrosBrowserTest::RefreshDevicePolicy() {
  // Reset the key to its original state.
  device_policy()->SetDefaultSigningKey();
  device_policy()->Build();
  session_manager_client()->set_device_policy(device_policy()->GetBlob());
  session_manager_client()->OnPropertyChangeComplete(true);
}

}  // namespace policy
