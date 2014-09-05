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
#include "extensions/common/manifest_constants.h"
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
    InitPrefService();
  }

  void InitPrefService() {
    extension_management_.reset();
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

 protected:
  scoped_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_ptr<ExtensionManagement> extension_management_;
};

class ExtensionAdminPolicyTest : public ExtensionManagementTest {
 public:
  ExtensionAdminPolicyTest() {}
  virtual ~ExtensionAdminPolicyTest() {}

  void CreateExtension(Manifest::Location location) {
    base::DictionaryValue values;
    CreateExtensionFromValues(location, &values);
  }

  void CreateHostedApp(Manifest::Location location) {
    base::DictionaryValue values;
    values.Set(extensions::manifest_keys::kWebURLs, new base::ListValue());
    values.SetString(extensions::manifest_keys::kLaunchWebURL,
                     "http://www.example.com");
    CreateExtensionFromValues(location, &values);
  }

  void CreateExtensionFromValues(Manifest::Location location,
                                 base::DictionaryValue* values) {
    values->SetString(extensions::manifest_keys::kName, "test");
    values->SetString(extensions::manifest_keys::kVersion, "0.1");
    std::string error;
    extension_ = Extension::Create(base::FilePath(), location, *values,
                                   Extension::NO_FLAGS, &error);
    ASSERT_TRUE(extension_.get());
  }

  // Wrappers for legacy admin policy functions, for testing purpose only.
  bool BlacklistedByDefault(const base::ListValue* blacklist);
  bool UserMayLoad(const base::ListValue* blacklist,
                   const base::ListValue* whitelist,
                   const base::DictionaryValue* forcelist,
                   const base::ListValue* allowed_types,
                   const Extension* extension,
                   base::string16* error);
  bool UserMayModifySettings(const Extension* extension, base::string16* error);
  bool MustRemainEnabled(const Extension* extension, base::string16* error);

 protected:
  scoped_refptr<Extension> extension_;
};

bool ExtensionAdminPolicyTest::BlacklistedByDefault(
    const base::ListValue* blacklist) {
  InitPrefService();
  if (blacklist)
    SetPref(true, pref_names::kInstallDenyList, blacklist->DeepCopy());
  return extension_management_->BlacklistedByDefault();
}

bool ExtensionAdminPolicyTest::UserMayLoad(
    const base::ListValue* blacklist,
    const base::ListValue* whitelist,
    const base::DictionaryValue* forcelist,
    const base::ListValue* allowed_types,
    const Extension* extension,
    base::string16* error) {
  InitPrefService();
  if (blacklist)
    SetPref(true, pref_names::kInstallDenyList, blacklist->DeepCopy());
  if (whitelist)
    SetPref(true, pref_names::kInstallAllowList, whitelist->DeepCopy());
  if (forcelist)
    SetPref(true, pref_names::kInstallForceList, forcelist->DeepCopy());
  if (allowed_types)
    SetPref(true, pref_names::kAllowedTypes, allowed_types->DeepCopy());
  return extension_management_->GetProvider()->UserMayLoad(extension, error);
}

bool ExtensionAdminPolicyTest::UserMayModifySettings(const Extension* extension,
                                                     base::string16* error) {
  InitPrefService();
  return extension_management_->GetProvider()->UserMayModifySettings(extension,
                                                                     error);
}

bool ExtensionAdminPolicyTest::MustRemainEnabled(const Extension* extension,
                                                 base::string16* error) {
  InitPrefService();
  return extension_management_->GetProvider()->MustRemainEnabled(extension,
                                                                 error);
}

// Verify that preference controlled by legacy ExtensionInstallSources policy is
// handled well.
TEST_F(ExtensionManagementTest, LegacyInstallSources) {
  base::ListValue allowed_sites_pref;
  allowed_sites_pref.AppendString("https://www.example.com/foo");
  allowed_sites_pref.AppendString("https://corp.mycompany.com/*");
  SetPref(
      true, pref_names::kAllowedInstallSites, allowed_sites_pref.DeepCopy());
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
  EXPECT_EQ(extension_management_->ReadById(kTargetExtension).installation_mode,
            ExtensionManagement::INSTALLATION_ALLOWED);
  EXPECT_EQ(extension_management_->ReadById(kOtherExtension).installation_mode,
            ExtensionManagement::INSTALLATION_BLOCKED);

  // Verify that install whitelist preference set by user is ignored.
  RemovePref(true, pref_names::kInstallAllowList);
  SetPref(false, pref_names::kInstallAllowList, allowed_list_pref.DeepCopy());
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
  EXPECT_EQ(extension_management_->ReadById(kTargetExtension).installation_mode,
            ExtensionManagement::INSTALLATION_FORCED);
  EXPECT_EQ(extension_management_->ReadById(kTargetExtension).update_url,
            kExampleUpdateUrl);
  EXPECT_EQ(extension_management_->ReadById(kOtherExtension).installation_mode,
            ExtensionManagement::INSTALLATION_ALLOWED);

  // Verify that install forcelist preference set by user is ignored.
  RemovePref(true, pref_names::kInstallForceList);
  SetPref(false, pref_names::kInstallForceList, forced_list_pref.DeepCopy());
  EXPECT_EQ(extension_management_->ReadById(kTargetExtension).installation_mode,
            ExtensionManagement::INSTALLATION_ALLOWED);
}

// Tests the flag value indicating that extensions are blacklisted by default.
TEST_F(ExtensionAdminPolicyTest, BlacklistedByDefault) {
  EXPECT_FALSE(BlacklistedByDefault(NULL));

  base::ListValue blacklist;
  blacklist.Append(new base::StringValue(kOtherExtension));
  EXPECT_FALSE(BlacklistedByDefault(&blacklist));
  blacklist.Append(new base::StringValue("*"));
  EXPECT_TRUE(BlacklistedByDefault(&blacklist));

  blacklist.Clear();
  blacklist.Append(new base::StringValue("*"));
  EXPECT_TRUE(BlacklistedByDefault(&blacklist));
}

// Tests UserMayLoad for required extensions.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadRequired) {
  CreateExtension(Manifest::COMPONENT);
  EXPECT_TRUE(UserMayLoad(NULL, NULL, NULL, NULL, extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(UserMayLoad(NULL, NULL, NULL, NULL, extension_.get(), &error));
  EXPECT_TRUE(error.empty());

  // Required extensions may load even if they're on the blacklist.
  base::ListValue blacklist;
  blacklist.Append(new base::StringValue(extension_->id()));
  EXPECT_TRUE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), NULL));

  blacklist.Append(new base::StringValue("*"));
  EXPECT_TRUE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), NULL));
}

// Tests UserMayLoad when no blacklist exists, or it's empty.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadNoBlacklist) {
  CreateExtension(Manifest::INTERNAL);
  EXPECT_TRUE(UserMayLoad(NULL, NULL, NULL, NULL, extension_.get(), NULL));
  base::ListValue blacklist;
  EXPECT_TRUE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}

// Tests UserMayLoad for an extension on the whitelist.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadWhitelisted) {
  CreateExtension(Manifest::INTERNAL);

  base::ListValue whitelist;
  whitelist.Append(new base::StringValue(extension_->id()));
  EXPECT_TRUE(
      UserMayLoad(NULL, &whitelist, NULL, NULL, extension_.get(), NULL));

  base::ListValue blacklist;
  blacklist.Append(new base::StringValue(extension_->id()));
  EXPECT_TRUE(
      UserMayLoad(NULL, &whitelist, NULL, NULL, extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(
      UserMayLoad(NULL, &whitelist, NULL, NULL, extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}

// Tests UserMayLoad for an extension on the blacklist.
TEST_F(ExtensionAdminPolicyTest, UserMayLoadBlacklisted) {
  CreateExtension(Manifest::INTERNAL);

  // Blacklisted by default.
  base::ListValue blacklist;
  blacklist.Append(new base::StringValue("*"));
  EXPECT_FALSE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), NULL));
  base::string16 error;
  EXPECT_FALSE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), &error));
  EXPECT_FALSE(error.empty());

  // Extension on the blacklist, with and without wildcard.
  blacklist.Append(new base::StringValue(extension_->id()));
  EXPECT_FALSE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), NULL));
  blacklist.Clear();
  blacklist.Append(new base::StringValue(extension_->id()));
  EXPECT_FALSE(
      UserMayLoad(&blacklist, NULL, NULL, NULL, extension_.get(), NULL));

  // With a whitelist. There's no such thing as a whitelist wildcard.
  base::ListValue whitelist;
  whitelist.Append(new base::StringValue("behllobkkfkfnphdnhnkndlbkcpglgmj"));
  EXPECT_FALSE(
      UserMayLoad(&blacklist, &whitelist, NULL, NULL, extension_.get(), NULL));
  whitelist.Append(new base::StringValue("*"));
  EXPECT_FALSE(
      UserMayLoad(&blacklist, &whitelist, NULL, NULL, extension_.get(), NULL));
}

TEST_F(ExtensionAdminPolicyTest, UserMayLoadAllowedTypes) {
  CreateExtension(Manifest::INTERNAL);
  EXPECT_TRUE(UserMayLoad(NULL, NULL, NULL, NULL, extension_.get(), NULL));

  base::ListValue allowed_types;
  EXPECT_FALSE(
      UserMayLoad(NULL, NULL, NULL, &allowed_types, extension_.get(), NULL));

  allowed_types.AppendInteger(Manifest::TYPE_EXTENSION);
  EXPECT_TRUE(
      UserMayLoad(NULL, NULL, NULL, &allowed_types, extension_.get(), NULL));

  CreateHostedApp(Manifest::INTERNAL);
  EXPECT_FALSE(
      UserMayLoad(NULL, NULL, NULL, &allowed_types, extension_.get(), NULL));

  CreateHostedApp(Manifest::EXTERNAL_POLICY_DOWNLOAD);
  EXPECT_FALSE(
      UserMayLoad(NULL, NULL, NULL, &allowed_types, extension_.get(), NULL));
}

TEST_F(ExtensionAdminPolicyTest, UserMayModifySettings) {
  CreateExtension(Manifest::INTERNAL);
  EXPECT_TRUE(UserMayModifySettings(extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(UserMayModifySettings(extension_.get(), &error));
  EXPECT_TRUE(error.empty());

  CreateExtension(Manifest::EXTERNAL_POLICY_DOWNLOAD);
  error.clear();
  EXPECT_FALSE(UserMayModifySettings(extension_.get(), NULL));
  EXPECT_FALSE(UserMayModifySettings(extension_.get(), &error));
  EXPECT_FALSE(error.empty());
}

TEST_F(ExtensionAdminPolicyTest, MustRemainEnabled) {
  CreateExtension(Manifest::EXTERNAL_POLICY_DOWNLOAD);
  EXPECT_TRUE(MustRemainEnabled(extension_.get(), NULL));
  base::string16 error;
  EXPECT_TRUE(MustRemainEnabled(extension_.get(), &error));
  EXPECT_FALSE(error.empty());

  CreateExtension(Manifest::INTERNAL);
  error.clear();
  EXPECT_FALSE(MustRemainEnabled(extension_.get(), NULL));
  EXPECT_FALSE(MustRemainEnabled(extension_.get(), &error));
  EXPECT_TRUE(error.empty());
}

}  // namespace extensions
