// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests make sure SettingsOverridePermission values are set correctly.

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/settings_override_permission.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class SettingsOverridePermissionTest : public ChromeManifestTest {
 protected:
  enum Flags {
    kHomepage = 1,
    kStartupPages = 1 << 1,
    kSearchProvider = 1 << 2,
  };

  scoped_refptr<Extension> GetPermissionSet(uint32 flags) {
    base::DictionaryValue ext_manifest;
    ext_manifest.SetString(manifest_keys::kName, "test");
    ext_manifest.SetString(manifest_keys::kVersion, "0.1");
    ext_manifest.SetInteger(manifest_keys::kManifestVersion, 2);

    scoped_ptr<base::DictionaryValue> settings_override(
        new base::DictionaryValue);
    if (flags & kHomepage)
      settings_override->SetString("homepage", "http://www.google.com");
    if (flags & kStartupPages) {
      scoped_ptr<base::ListValue> startup_pages(new base::ListValue);
      startup_pages->AppendString("http://startup.com/startup.html");
      settings_override->Set("startup_pages", startup_pages.release());
    }
    if (flags & kSearchProvider) {
      scoped_ptr<base::DictionaryValue> search_provider(
          new base::DictionaryValue);
      search_provider->SetString("search_url", "http://google.com/search.html");
      search_provider->SetString("name", "test");
      search_provider->SetString("keyword", "lock");
      search_provider->SetString("encoding", "UTF-8");
      search_provider->SetBoolean("is_default", true);
      search_provider->SetString("favicon_url",
                                 "http://wikipedia.org/wiki/Favicon");
      settings_override->Set("search_provider", search_provider.release());
    }
    ext_manifest.Set(
        manifest_keys::kSettingsOverride, settings_override.release());

    ManifestData manifest(&ext_manifest, "test");
    return LoadAndExpectSuccess(manifest);
  }
};

TEST_F(SettingsOverridePermissionTest, HomePage) {
  scoped_refptr<Extension> extension(GetPermissionSet(kHomepage));
  scoped_refptr<const PermissionSet> permission_set(
      extension->permissions_data()->active_permissions());

#if defined(OS_WIN)
  EXPECT_TRUE(permission_set->HasAPIPermission(APIPermission::kHomepage));
  std::vector<base::string16> warnings =
      extension->permissions_data()->GetPermissionMessageStrings();
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ("Change your home page to: google.com/",
            base::UTF16ToUTF8(warnings[0]));

  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kStartupPages));
  EXPECT_FALSE(permission_set->HasAPIPermission(
      APIPermission::kSearchProvider));
#else
  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kHomepage));
  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kStartupPages));
  EXPECT_FALSE(
      permission_set->HasAPIPermission(APIPermission::kSearchProvider));
#endif
}

TEST_F(SettingsOverridePermissionTest, StartupPages) {
  scoped_refptr<Extension> extension(GetPermissionSet(kStartupPages));
  scoped_refptr<const PermissionSet> permission_set(
      extension->permissions_data()->active_permissions());

#if defined(OS_WIN)
  EXPECT_TRUE(permission_set->HasAPIPermission(APIPermission::kStartupPages));
  std::vector<base::string16> warnings =
      extension->permissions_data()->GetPermissionMessageStrings();
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ("Change your start page to: startup.com/startup.html",
            base::UTF16ToUTF8(warnings[0]));

  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kHomepage));
  EXPECT_FALSE(permission_set->HasAPIPermission(
      APIPermission::kSearchProvider));
#else
  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kHomepage));
  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kStartupPages));
  EXPECT_FALSE(
      permission_set->HasAPIPermission(APIPermission::kSearchProvider));
#endif
}

TEST_F(SettingsOverridePermissionTest, SearchSettings) {
  scoped_refptr<Extension> extension(GetPermissionSet(kSearchProvider));
  scoped_refptr<const PermissionSet> permission_set(
      extension->permissions_data()->active_permissions());

#if defined(OS_WIN)
  EXPECT_TRUE(permission_set->HasAPIPermission(APIPermission::kSearchProvider));
  std::vector<base::string16> warnings =
      extension->permissions_data()->GetPermissionMessageStrings();
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ("Change your search settings to: google.com",
            base::UTF16ToUTF8(warnings[0]));

  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kHomepage));
  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kStartupPages));
#else
  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kHomepage));
  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kStartupPages));
  EXPECT_FALSE(
      permission_set->HasAPIPermission(APIPermission::kSearchProvider));
#endif
}

TEST_F(SettingsOverridePermissionTest, All) {
  scoped_refptr<Extension> extension(GetPermissionSet(
      kSearchProvider | kStartupPages | kHomepage));
  scoped_refptr<const PermissionSet> permission_set(
      extension->permissions_data()->active_permissions());

#if defined(OS_WIN)
  EXPECT_TRUE(permission_set->HasAPIPermission(APIPermission::kSearchProvider));
  EXPECT_TRUE(permission_set->HasAPIPermission(APIPermission::kHomepage));
  EXPECT_TRUE(permission_set->HasAPIPermission(APIPermission::kStartupPages));
#else
  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kHomepage));
  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kStartupPages));
  EXPECT_FALSE(
      permission_set->HasAPIPermission(APIPermission::kSearchProvider));
#endif
}

TEST_F(SettingsOverridePermissionTest, Some) {
  scoped_refptr<Extension> extension(GetPermissionSet(
      kSearchProvider | kHomepage));
  scoped_refptr<const PermissionSet> permission_set(
      extension->permissions_data()->active_permissions());

#if defined(OS_WIN)
  EXPECT_TRUE(permission_set->HasAPIPermission(APIPermission::kSearchProvider));
  EXPECT_TRUE(permission_set->HasAPIPermission(APIPermission::kHomepage));
  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kStartupPages));
#else
  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kHomepage));
  EXPECT_FALSE(permission_set->HasAPIPermission(APIPermission::kStartupPages));
  EXPECT_FALSE(
      permission_set->HasAPIPermission(APIPermission::kSearchProvider));
#endif
}

}  // namespace

}  // namespace extensions
