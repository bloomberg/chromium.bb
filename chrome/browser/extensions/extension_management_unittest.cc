// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {
const char kTargetExtension[] = "abcdefghijklmnopabcdefghijklmnop";
const char kOtherExtension[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kExampleUpdateUrl[] = "http://example.com/update_url";
}  // namespace

class ExtensionManagementTest : public testing::Test {
 public:
  ExtensionManagementTest() {}
  virtual ~ExtensionManagementTest() {}

  // testing::Test:
  virtual void SetUp() OVERRIDE {
    pref_service_.reset(new TestingPrefServiceSimple());
    pref_service_->registry()->RegisterListPref(
        pref_names::kAllowedInstallSites);
    pref_service_->registry()->RegisterListPref(pref_names::kAllowedTypes);
    pref_service_->registry()->RegisterListPref(pref_names::kInstallDenyList);
    pref_service_->registry()->RegisterListPref(pref_names::kInstallAllowList);
    pref_service_->registry()->RegisterDictionaryPref(
        pref_names::kInstallForceList);
    extension_management_.reset(new ExtensionManagement(pref_service_.get()));
  }

  void SetPref(bool managed, const char* path, base::Value* value) {
    if (managed)
      pref_service_->SetManagedPref(path, value);
    else
      pref_service_->SetUserPref(path, value);
  }

  void RemovePref(bool managed, const char* path) {
    if (managed)
      pref_service_->RemoveManagedPref(path);
    else
      pref_service_->RemoveUserPref(path);
  }

  void Refresh() {
    extension_management_->Refresh();
  }

 protected:
  scoped_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_ptr<ExtensionManagement> extension_management_;
};

// Verify that preference controlled by legacy ExtensionInstallSources policy is
// handled well.
TEST_F(ExtensionManagementTest, LegacyInstallSources) {
  base::ListValue allowed_sites_pref;
  allowed_sites_pref.AppendString("https://www.example.com/foo");
  allowed_sites_pref.AppendString("https://corp.mycompany.com/*");
  SetPref(
      true, pref_names::kAllowedInstallSites, allowed_sites_pref.DeepCopy());
  Refresh();
  const URLPatternSet& allowed_sites =
      extension_management_->ReadGlobalSettings().install_sources;
  ASSERT_TRUE(extension_management_->ReadGlobalSettings()
                  .has_restricted_install_sources);
  EXPECT_FALSE(allowed_sites.is_empty());
  EXPECT_TRUE(allowed_sites.MatchesURL(GURL("https://www.example.com/foo")));
  EXPECT_FALSE(allowed_sites.MatchesURL(GURL("https://www.example.com/bar")));
  EXPECT_TRUE(
      allowed_sites.MatchesURL(GURL("https://corp.mycompany.com/entry")));
  EXPECT_FALSE(
      allowed_sites.MatchesURL(GURL("https://www.mycompany.com/entry")));
}

// Verify that preference controlled by legacy ExtensionAllowedTypes policy is
// handled well.
TEST_F(ExtensionManagementTest, LegacyAllowedTypes) {
  base::ListValue allowed_types_pref;
  allowed_types_pref.AppendInteger(Manifest::TYPE_THEME);
  allowed_types_pref.AppendInteger(Manifest::TYPE_USER_SCRIPT);

  SetPref(true, pref_names::kAllowedTypes, allowed_types_pref.DeepCopy());
  Refresh();
  const std::vector<Manifest::Type>& allowed_types =
      extension_management_->ReadGlobalSettings().allowed_types;
  ASSERT_TRUE(
      extension_management_->ReadGlobalSettings().has_restricted_allowed_types);
  EXPECT_TRUE(allowed_types.size() == 2);
  EXPECT_FALSE(std::find(allowed_types.begin(),
                         allowed_types.end(),
                         Manifest::TYPE_EXTENSION) != allowed_types.end());
  EXPECT_TRUE(std::find(allowed_types.begin(),
                        allowed_types.end(),
                        Manifest::TYPE_THEME) != allowed_types.end());
  EXPECT_TRUE(std::find(allowed_types.begin(),
                   allowed_types.end(),
                   Manifest::TYPE_USER_SCRIPT) != allowed_types.end());
}

// Verify that preference controlled by legacy ExtensionInstallBlacklist policy
// is handled well.
TEST_F(ExtensionManagementTest, LegacyInstallBlacklist) {
  base::ListValue denied_list_pref;
  denied_list_pref.AppendString(kTargetExtension);

  SetPref(true, pref_names::kInstallDenyList, denied_list_pref.DeepCopy());
  Refresh();
  EXPECT_EQ(extension_management_->ReadById(kTargetExtension).installation_mode,
            ExtensionManagement::INSTALLATION_BLOCKED);
  EXPECT_EQ(extension_management_->ReadById(kOtherExtension).installation_mode,
            ExtensionManagement::INSTALLATION_ALLOWED);
}

// Verify that preference controlled by legacy ExtensionInstallWhitelist policy
// is handled well.
TEST_F(ExtensionManagementTest, LegacyInstallWhitelist) {
  base::ListValue denied_list_pref;
  denied_list_pref.AppendString("*");
  base::ListValue allowed_list_pref;
  allowed_list_pref.AppendString(kTargetExtension);

  SetPref(true, pref_names::kInstallDenyList, denied_list_pref.DeepCopy());
  SetPref(true, pref_names::kInstallAllowList, allowed_list_pref.DeepCopy());
  Refresh();
  EXPECT_EQ(extension_management_->ReadById(kTargetExtension).installation_mode,
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(extension_management_->ReadById(kOtherExtension).installation_mode,
            ExtensionManagement::INSTALLATION_BLOCKED);

  // Verify that install whitelist preference set by user is ignored.
  RemovePref(true, pref_names::kInstallAllowList);
  SetPref(false, pref_names::kInstallAllowList, allowed_list_pref.DeepCopy());
  Refresh();
  EXPECT_EQ(extension_management_->ReadById(kTargetExtension).installation_mode,
            ExtensionManagement::INSTALLATION_BLOCKED);
}

// Verify that preference controlled by legacy ExtensionInstallForcelist policy
// is handled well.
TEST_F(ExtensionManagementTest, LegacyInstallForcelist) {
  base::DictionaryValue forced_list_pref;
  ExternalPolicyLoader::AddExtension(
      &forced_list_pref, kTargetExtension, kExampleUpdateUrl);

  SetPref(true, pref_names::kInstallForceList, forced_list_pref.DeepCopy());
  Refresh();
  EXPECT_EQ(extension_management_->ReadById(kTargetExtension).installation_mode,
            ExtensionManagement::INSTALLATION_FORCED);
  EXPECT_EQ(extension_management_->ReadById(kTargetExtension).update_url,
            kExampleUpdateUrl);
  EXPECT_EQ(extension_management_->ReadById(kOtherExtension).installation_mode,
            ExtensionManagement::INSTALLATION_ALLOWED);

  // Verify that install forcelist preference set by user is ignored.
  RemovePref(true, pref_names::kInstallForceList);
  SetPref(false, pref_names::kInstallForceList, forced_list_pref.DeepCopy());
  Refresh();
  EXPECT_EQ(extension_management_->ReadById(kTargetExtension).installation_mode,
            ExtensionManagement::INSTALLATION_ALLOWED);
}

}  // namespace extensions
