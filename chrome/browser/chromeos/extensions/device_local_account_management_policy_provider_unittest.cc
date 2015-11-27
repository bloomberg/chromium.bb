// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
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
    base::DictionaryValue* values,
    int flags) {
  values->SetString(extensions::manifest_keys::kName, "test");
  values->SetString(extensions::manifest_keys::kVersion, "0.1");
  std::string error;
  return extensions::Extension::Create(base::FilePath(),
                                       location,
                                       *values,
                                       flags,
                                       id,
                                       &error);
}

scoped_refptr<const extensions::Extension> CreateRegularExtension(
    const std::string& id) {
  base::DictionaryValue values;
  return CreateExtensionFromValues(id,
                                   extensions::Manifest::INTERNAL,
                                   &values,
                                   extensions::Extension::NO_FLAGS);
}

scoped_refptr<const extensions::Extension> CreateExternalComponentExtension() {
  base::DictionaryValue values;
  return CreateExtensionFromValues(std::string(),
                                   extensions::Manifest::EXTERNAL_COMPONENT,
                                   &values,
                                   extensions::Extension::NO_FLAGS);
}

scoped_refptr<const extensions::Extension> CreateHostedApp() {
  base::DictionaryValue values;
  values.Set(extensions::manifest_keys::kApp, new base::DictionaryValue);
  values.Set(extensions::manifest_keys::kWebURLs, new base::ListValue);
  return CreateExtensionFromValues(std::string(),
                                   extensions::Manifest::INTERNAL,
                                   &values,
                                   extensions::Extension::NO_FLAGS);
}

scoped_refptr<const extensions::Extension> CreatePlatformAppWithExtraValues(
    const base::DictionaryValue* extra_values,
    extensions::Manifest::Location location,
    int flags) {
  base::DictionaryValue values;
  values.SetString("app.background.page", "background.html");
  values.MergeDictionary(extra_values);
  return CreateExtensionFromValues(std::string(), location, &values, flags);
}

scoped_refptr<const extensions::Extension> CreatePlatformApp() {
  base::DictionaryValue values;
  return CreatePlatformAppWithExtraValues(&values,
                                          extensions::Manifest::INTERNAL,
                                          extensions::Extension::NO_FLAGS);
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
  // been whitelisted for use in public sessions, the extension cannot be
  // installed.
  extension = CreateRegularExtension(std::string());
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_NE(base::string16(), error);
  error.clear();

#if 0
  // Verify that a minimal platform app can be installed from location
  // EXTERNAL_POLICY.
  {
    base::DictionaryValue values;
    extension = CreatePlatformAppWithExtraValues(
        &values,
        extensions::Manifest::EXTERNAL_POLICY,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(base::string16(), error);
    error.clear();
  }

  // Verify that a minimal platform app can be installed from location
  // EXTERNAL_POLICY_DOWNLOAD.
  {
    base::DictionaryValue values;
    extension = CreatePlatformAppWithExtraValues(
        &values,
        extensions::Manifest::EXTERNAL_POLICY_DOWNLOAD,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(base::string16(), error);
    error.clear();
  }

  // Verify that a minimal platform app cannot be installed from location
  // UNPACKED.
  {
    base::DictionaryValue values;
    extension = CreatePlatformAppWithExtraValues(
        &values,
        extensions::Manifest::UNPACKED,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(base::string16(), error);
    error.clear();
  }

  // Verify that a platform app with all safe manifest entries can be installed.
  {
    base::DictionaryValue values;
    values.SetString(extensions::manifest_keys::kDescription, "something");
    values.SetString(extensions::manifest_keys::kShortName, "something else");
    base::ListValue* permissions = new base::ListValue();
    permissions->AppendString("alarms");
    permissions->AppendString("background");
    values.Set(extensions::manifest_keys::kPermissions, permissions);
    base::ListValue* optional_permissions = new base::ListValue();
    optional_permissions->AppendString("alarms");
    optional_permissions->AppendString("background");
    values.Set(extensions::manifest_keys::kOptionalPermissions,
               optional_permissions);
    extension = CreatePlatformAppWithExtraValues(
        &values,
        extensions::Manifest::EXTERNAL_POLICY,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(base::string16(), error);
    error.clear();
  }

  // Verify that a platform app with an unknown manifest entry cannot be
  // installed.
  {
    base::DictionaryValue values;
    values.SetString("not_whitelisted", "something");
    extension = CreatePlatformAppWithExtraValues(
        &values,
        extensions::Manifest::EXTERNAL_POLICY,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(base::string16(), error);
    error.clear();
  }

  // Verify that a platform app with an unknown manifest entry under "app"
  // cannot be installed.
  {
    base::DictionaryValue values;
    values.SetString("app.not_whitelisted2", "something2");
    extension = CreatePlatformAppWithExtraValues(
        &values,
        extensions::Manifest::EXTERNAL_POLICY,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(base::string16(), error);
    error.clear();
  }

  // Verify that a platform app with an unsafe permission entry cannot be
  // installed.
  {
    base::ListValue* const permissions = new base::ListValue();
    permissions->AppendString("audioCapture");
    base::DictionaryValue values;
    values.Set(extensions::manifest_keys::kPermissions, permissions);

    extension = CreatePlatformAppWithExtraValues(
        &values,
        extensions::Manifest::EXTERNAL_POLICY,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(base::string16(), error);
    error.clear();
  }

  // Verify that a platform app with an unsafe optional permission entry cannot
  // be installed.
  {
    base::ListValue* const permissions = new base::ListValue();
    permissions->AppendString("audioCapture");
    base::DictionaryValue values;
    values.Set(extensions::manifest_keys::kOptionalPermissions, permissions);

    extension = CreatePlatformAppWithExtraValues(
        &values,
        extensions::Manifest::EXTERNAL_POLICY,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(base::string16(), error);
    error.clear();
  }

  // Verify that a platform app with an url_handlers manifest entry and which is
  // not installed through the web store cannot be installed.
  {
    base::ListValue* const matches = new base::ListValue();
    matches->AppendString("https://example.com/*");
    base::DictionaryValue values;
    values.Set("url_handlers.example_com.matches", matches);
    values.SetString("url_handlers.example_com.title", "example title");

    extension = CreatePlatformAppWithExtraValues(
        &values,
        extensions::Manifest::EXTERNAL_POLICY,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(base::string16(), error);
    error.clear();
  }

  // Verify that a platform app with a url_handlers manifest entry and which is
  // installed through the web store can be installed.
  {
    base::ListValue* const matches = new base::ListValue();
    matches->AppendString("https://example.com/*");
    base::DictionaryValue values;
    values.Set("url_handlers.example_com.matches", matches);
    values.SetString("url_handlers.example_com.title", "example title");

    extension = CreatePlatformAppWithExtraValues(
        &values,
        extensions::Manifest::EXTERNAL_POLICY,
        extensions::Extension::FROM_WEBSTORE);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(base::string16(), error);
    error.clear();
  }
#endif
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
  EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_EQ(base::string16(), error);
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
  EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_EQ(base::string16(), error);
  error.clear();
}

}  // namespace chromeos
