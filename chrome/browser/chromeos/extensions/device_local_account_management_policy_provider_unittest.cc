// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kWhitelistedId[] = "cbkkbcmdlboombapidmoeolnmdacpkch";

scoped_refptr<const extensions::Extension> CreateExtensionFromValues(
    const std::string& id,
    extensions::Manifest::Location location,
    base::DictionaryValue* values) {
  values->SetString(extensions::manifest_keys::kName, "test");
  values->SetString(extensions::manifest_keys::kVersion, "0.1");
  std::string error;
  return extensions::Extension::Create(base::FilePath(),
                                       location,
                                       *values,
                                       extensions::Extension::NO_FLAGS,
                                       id,
                                       &error);
}

scoped_refptr<const extensions::Extension> CreateRegularExtension(
    const std::string& id) {
  base::DictionaryValue values;
  return CreateExtensionFromValues(id, extensions::Manifest::INTERNAL, &values);
}

scoped_refptr<const extensions::Extension> CreateExternalComponentExtension() {
  base::DictionaryValue values;
  return CreateExtensionFromValues(std::string(),
                                   extensions::Manifest::EXTERNAL_COMPONENT,
                                   &values);
}

scoped_refptr<const extensions::Extension> CreateHostedApp() {
  base::DictionaryValue values;
  values.Set(extensions::manifest_keys::kApp, new base::DictionaryValue);
  values.Set(extensions::manifest_keys::kWebURLs, new base::ListValue);
  return CreateExtensionFromValues(std::string(),
                                   extensions::Manifest::INTERNAL,
                                   &values);
}

scoped_refptr<const extensions::Extension> CreatePlatformApp() {
  base::DictionaryValue values;
  values.Set(extensions::manifest_keys::kApp, new base::DictionaryValue);
  values.Set(extensions::manifest_keys::kPlatformAppBackground,
             new base::DictionaryValue);
  values.Set(extensions::manifest_keys::kPlatformAppBackgroundPage,
             new base::StringValue("background.html"));
  return CreateExtensionFromValues(std::string(),
                                   extensions::Manifest::INTERNAL,
                                   &values);
}

}  // namespace

TEST(DeviceLocalAccountManagementPolicyProviderTest, PublicSession) {
  DeviceLocalAccountManagementPolicyProvider
      provider(policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION);

  // Verify that if an extension's location has been whitelisted for use in
  // public sessions, the extension can be installed.
  scoped_refptr<const extensions::Extension> extension =
      CreateExternalComponentExtension();
  ASSERT_TRUE(extension.get());
  base::string16 error;
  EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_EQ(base::string16(), error);
  error.clear();

  // Verify that if an extension's type has been whitelisted for use in
  // device-local accounts, the extension can be installed.
  extension = CreateHostedApp();
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_EQ(base::string16(), error);
  error.clear();

  // Verify that if an extension's ID has been explicitly whitelisted for use in
  // device-local accounts, the extension can be installed.
  extension = CreateRegularExtension(kWhitelistedId);
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_EQ(base::string16(), error);
  error.clear();

  // Verify that if neither the location, type nor the ID of an extension have
  // been  whitelisted for use in public sessions, the extension cannot be
  // installed.
  extension = CreateRegularExtension(std::string());
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_NE(base::string16(), error);
  error.clear();
}

TEST(DeviceLocalAccountManagementPolicyProviderTest, KioskAppSession) {
  DeviceLocalAccountManagementPolicyProvider
      provider(policy::DeviceLocalAccount::TYPE_KIOSK_APP);

  // Verify that a platform app can be installed.
  scoped_refptr<const extensions::Extension> extension = CreatePlatformApp();
  ASSERT_TRUE(extension.get());
  base::string16 error;
  EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_EQ(base::string16(), error);
  error.clear();

  // Verify that an extension whose location has been whitelisted for use in
  // other types of device-local accounts cannot be installed in a single-app
  // kiosk session.
  extension = CreateExternalComponentExtension();
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_NE(base::string16(), error);
  error.clear();

  // Verify that an extension whose type has been whitelisted for use in other
  // types of device-local accounts cannot be installed in a single-app kiosk
  // session.
  extension = CreateHostedApp();
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_NE(base::string16(), error);
  error.clear();

  // Verify that an extension whose ID has been whitelisted for use in other
  // types of device-local accounts cannot be installed in a single-app kiosk
  // session.
  extension = CreateRegularExtension(kWhitelistedId);
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_NE(base::string16(), error);
  error.clear();
}

}  // namespace chromeos
