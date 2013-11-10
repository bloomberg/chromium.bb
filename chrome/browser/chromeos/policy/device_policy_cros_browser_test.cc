// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/proto/chromeos/install_attributes.pb.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace policy {

DevicePolicyCrosTestHelper::DevicePolicyCrosTestHelper() {
  CHECK(temp_dir_.CreateUniqueTempDir());
}

DevicePolicyCrosTestHelper::~DevicePolicyCrosTestHelper() {}

void DevicePolicyCrosTestHelper::MarkAsEnterpriseOwned() {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  cryptohome::SerializedInstallAttributes::Attribute* attribute = NULL;

  attribute = install_attrs_proto.add_attributes();
  attribute->set_name(EnterpriseInstallAttributes::kAttrEnterpriseOwned);
  attribute->set_value("true");

  attribute = install_attrs_proto.add_attributes();
  attribute->set_name(EnterpriseInstallAttributes::kAttrEnterpriseUser);
  attribute->set_value(device_policy_.policy_data().username());

  base::FilePath install_attrs_file =
      temp_dir_.path().AppendASCII("install_attributes.pb");
  const std::string install_attrs_blob(
      install_attrs_proto.SerializeAsString());
  ASSERT_EQ(static_cast<int>(install_attrs_blob.size()),
            file_util::WriteFile(install_attrs_file,
                                 install_attrs_blob.c_str(),
                                 install_attrs_blob.size()));
  ASSERT_TRUE(PathService::Override(chromeos::FILE_INSTALL_ATTRIBUTES,
                                    install_attrs_file));
}

void DevicePolicyCrosTestHelper::InstallOwnerKey() {
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

DevicePolicyCrosBrowserTest::DevicePolicyCrosBrowserTest()
    : fake_dbus_thread_manager_(new chromeos::FakeDBusThreadManager),
      fake_session_manager_client_(new chromeos::FakeSessionManagerClient) {
  fake_dbus_thread_manager_->SetFakeClients();
  fake_dbus_thread_manager_->SetSessionManagerClient(
      scoped_ptr<chromeos::SessionManagerClient>(fake_session_manager_client_));
}

DevicePolicyCrosBrowserTest::~DevicePolicyCrosBrowserTest() {
}

void DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture() {
  chromeos::DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager_);
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
}

void DevicePolicyCrosBrowserTest::TearDownInProcessBrowserTestFixture() {
  InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  chromeos::DBusThreadManager::Shutdown();
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
