// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/value_builder.h"

namespace extensions {

namespace {

scoped_refptr<const Extension> CreateExtensionWithPermissions(
    const std::set<URLPattern>& scriptable_hosts,
    const std::set<URLPattern>& explicit_hosts,
    Manifest::Location location,
    const std::string& name) {
  ListBuilder scriptable_host_list;
  for (std::set<URLPattern>::const_iterator pattern = scriptable_hosts.begin();
       pattern != scriptable_hosts.end(); ++pattern) {
    scriptable_host_list.Append(pattern->GetAsString());
  }

  ListBuilder explicit_host_list;
  for (std::set<URLPattern>::const_iterator pattern = explicit_hosts.begin();
       pattern != explicit_hosts.end(); ++pattern) {
    explicit_host_list.Append(pattern->GetAsString());
  }

  DictionaryBuilder script;
  script.Set("matches", scriptable_host_list.Build())
      .Set("js", ListBuilder().Append("foo.js").Build());

  return ExtensionBuilder()
      .SetLocation(location)
      .SetManifest(DictionaryBuilder()
                       .Set("name", name)
                       .Set("description", "foo")
                       .Set("manifest_version", 2)
                       .Set("version", "0.1.2.3")
                       .Set("content_scripts",
                            ListBuilder().Append(script.Build()).Build())
                       .Set("permissions", explicit_host_list.Build())
                       .Build())
      .SetID(crx_file::id_util::GenerateId(name))
      .Build();
}

testing::AssertionResult SetsAreEqual(const std::set<URLPattern>& set1,
                                      const std::set<URLPattern>& set2) {
  // Take the (set1 - set2) U (set2 - set1). This is then the set of all
  // elements which are in either set1 or set2, but not both.
  // If the sets are equal, this is none.
  std::set<URLPattern> difference = base::STLSetUnion<std::set<URLPattern>>(
      base::STLSetDifference<std::set<URLPattern>>(set1, set2),
      base::STLSetDifference<std::set<URLPattern>>(set2, set1));

  std::string error;
  for (std::set<URLPattern>::const_iterator iter = difference.begin();
       iter != difference.end(); ++iter) {
    if (iter->GetAsString() == "chrome://favicon/*")
      continue;  // Grr... This is auto-added for extensions with <all_urls>
    error = base::StringPrintf(
        "%s\n%s contains %s and the other does not.", error.c_str(),
        (set1.count(*iter) ? "Set1" : "Set2"), iter->GetAsString().c_str());
  }

  if (!error.empty())
    return testing::AssertionFailure() << error;
  return testing::AssertionSuccess();
}

using ScriptingPermissionsModifierUnitTest = ExtensionServiceTestBase;

}  // namespace

TEST_F(ScriptingPermissionsModifierUnitTest, WithholdAllHosts) {
  InitializeEmptyExtensionService();

  // Permissions are only withheld with the appropriate switch turned on.
  std::unique_ptr<FeatureSwitch::ScopedOverride> switch_override(
      new FeatureSwitch::ScopedOverride(FeatureSwitch::scripts_require_action(),
                                        FeatureSwitch::OVERRIDE_ENABLED));

  URLPattern google(URLPattern::SCHEME_ALL, "http://www.google.com/*");
  URLPattern sub_google(URLPattern::SCHEME_ALL, "http://*.google.com/*");
  URLPattern all_http(URLPattern::SCHEME_ALL, "http://*/*");
  URLPattern all_hosts(URLPattern::SCHEME_ALL, "<all_urls>");
  URLPattern all_com(URLPattern::SCHEME_ALL, "http://*.com/*");

  std::set<URLPattern> all_host_patterns;
  std::set<URLPattern> safe_patterns;

  all_host_patterns.insert(all_http);
  all_host_patterns.insert(all_hosts);
  all_host_patterns.insert(all_com);

  safe_patterns.insert(google);
  safe_patterns.insert(sub_google);

  std::set<URLPattern> all_patterns =
      base::STLSetUnion<std::set<URLPattern>>(all_host_patterns, safe_patterns);

  scoped_refptr<const Extension> extension = CreateExtensionWithPermissions(
      all_patterns, all_patterns, Manifest::INTERNAL, "a");
  const PermissionsData* permissions_data = extension->permissions_data();
  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension.get());

  // At first, the active permissions should have only the safe patterns and
  // the withheld permissions should have only the all host patterns.
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().scriptable_hosts().patterns(),
      safe_patterns));
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().explicit_hosts().patterns(),
      safe_patterns));
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->withheld_permissions().scriptable_hosts().patterns(),
      all_host_patterns));
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->withheld_permissions().explicit_hosts().patterns(),
      all_host_patterns));

  ScriptingPermissionsModifier modifier(profile(), extension);
  // Then, we grant the withheld all-hosts permissions.
  modifier.SetAllowedOnAllUrls(true);
  // Now, active permissions should have all patterns, and withheld permissions
  // should have none.
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().scriptable_hosts().patterns(),
      all_patterns));
  EXPECT_TRUE(permissions_data->withheld_permissions()
                  .scriptable_hosts()
                  .patterns()
                  .empty());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().explicit_hosts().patterns(),
      all_patterns));
  EXPECT_TRUE(permissions_data->withheld_permissions()
                  .explicit_hosts()
                  .patterns()
                  .empty());

  // Finally, we revoke the all hosts permissions.
  modifier.SetAllowedOnAllUrls(false);

  // We should be back to our initial state - all_hosts should be withheld, and
  // the safe patterns should be granted.
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().scriptable_hosts().patterns(),
      safe_patterns));
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().explicit_hosts().patterns(),
      safe_patterns));
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->withheld_permissions().scriptable_hosts().patterns(),
      all_host_patterns));
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->withheld_permissions().explicit_hosts().patterns(),
      all_host_patterns));

  // Creating a component extension should result in no withheld permissions.
  extension = CreateExtensionWithPermissions(all_patterns, all_patterns,
                                             Manifest::COMPONENT, "b");
  permissions_data = extension->permissions_data();
  updater.InitializePermissions(extension.get());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().scriptable_hosts().patterns(),
      all_patterns));
  EXPECT_TRUE(permissions_data->withheld_permissions()
                  .scriptable_hosts()
                  .patterns()
                  .empty());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().explicit_hosts().patterns(),
      all_patterns));
  EXPECT_TRUE(permissions_data->withheld_permissions()
                  .explicit_hosts()
                  .patterns()
                  .empty());

  // Without the switch, we shouldn't withhold anything.
  switch_override.reset();
  extension = CreateExtensionWithPermissions(all_patterns, all_patterns,
                                             Manifest::INTERNAL, "c");
  permissions_data = extension->permissions_data();
  updater.InitializePermissions(extension.get());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().scriptable_hosts().patterns(),
      all_patterns));
  EXPECT_TRUE(permissions_data->withheld_permissions()
                  .scriptable_hosts()
                  .patterns()
                  .empty());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().explicit_hosts().patterns(),
      all_patterns));
  EXPECT_TRUE(permissions_data->withheld_permissions()
                  .explicit_hosts()
                  .patterns()
                  .empty());
}

// Tests that withholding all hosts behaves properly with extensions installed
// when the switch is turned on and off.
TEST_F(ScriptingPermissionsModifierUnitTest,
       WithholdAllHostsWithTransientSwitch) {
  InitializeEmptyExtensionService();

  URLPattern all_hosts(URLPattern::SCHEME_ALL, "<all_urls>");
  std::set<URLPattern> all_host_patterns;
  all_host_patterns.insert(all_hosts);

  scoped_refptr<const Extension> extension_a = CreateExtensionWithPermissions(
      all_host_patterns, all_host_patterns, Manifest::INTERNAL, "a");
  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension_a.get());
  const PermissionsData* permissions_data = extension_a->permissions_data();

  // Since the extension was created without the switch on, it should default
  // to having all urls access.
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().scriptable_hosts().patterns(),
      all_host_patterns));
  EXPECT_TRUE(
      permissions_data->withheld_permissions().scriptable_hosts().is_empty());
  ScriptingPermissionsModifier modifier_a(profile(), extension_a);
  EXPECT_TRUE(modifier_a.IsAllowedOnAllUrls());

  // Enable the switch, and re-init permission for the extension.
  std::unique_ptr<FeatureSwitch::ScopedOverride> switch_override(
      new FeatureSwitch::ScopedOverride(FeatureSwitch::scripts_require_action(),
                                        FeatureSwitch::OVERRIDE_ENABLED));
  updater.InitializePermissions(extension_a.get());

  // Since the extension was installed when the switch was off, it should still
  // have the all urls pref.
  permissions_data = extension_a->permissions_data();
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().scriptable_hosts().patterns(),
      all_host_patterns));
  EXPECT_TRUE(
      permissions_data->withheld_permissions().scriptable_hosts().is_empty());
  EXPECT_TRUE(modifier_a.IsAllowedOnAllUrls());

  // Load a new extension, which also has all urls. Since the switch is now on,
  // the permissions should be withheld.
  scoped_refptr<const Extension> extension_b = CreateExtensionWithPermissions(
      all_host_patterns, all_host_patterns, Manifest::INTERNAL, "b");
  updater.InitializePermissions(extension_b.get());
  permissions_data = extension_b->permissions_data();
  EXPECT_TRUE(
      permissions_data->active_permissions().scriptable_hosts().is_empty());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->withheld_permissions().scriptable_hosts().patterns(),
      all_host_patterns));
  ScriptingPermissionsModifier modifier_b(profile(), extension_b);
  EXPECT_FALSE(modifier_b.IsAllowedOnAllUrls());

  // Disable the switch, and reload the extension.
  switch_override.reset();
  updater.InitializePermissions(extension_b.get());

  // Since the extension was installed with the switch on, it should still be
  // restricted with the switch off.
  permissions_data = extension_b->permissions_data();
  EXPECT_TRUE(
      permissions_data->active_permissions().scriptable_hosts().is_empty());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->withheld_permissions().scriptable_hosts().patterns(),
      all_host_patterns));
  EXPECT_FALSE(modifier_b.IsAllowedOnAllUrls());
}

TEST_F(ScriptingPermissionsModifierUnitTest, GrantHostPermission) {
  InitializeEmptyExtensionService();

  // Permissions are only withheld with the appropriate switch turned on.
  std::unique_ptr<FeatureSwitch::ScopedOverride> switch_override(
      new FeatureSwitch::ScopedOverride(FeatureSwitch::scripts_require_action(),
                                        FeatureSwitch::OVERRIDE_ENABLED));

  URLPattern all_hosts(URLPattern::SCHEME_ALL, "<all_urls>");
  std::set<URLPattern> all_host_patterns;
  all_host_patterns.insert(all_hosts);

  scoped_refptr<const Extension> extension = CreateExtensionWithPermissions(
      all_host_patterns, all_host_patterns, Manifest::INTERNAL, "extension");
  PermissionsUpdater(profile()).InitializePermissions(extension.get());

  ScriptingPermissionsModifier modifier(profile(), extension);

  const GURL kUrl("https://www.google.com/");
  const GURL kUrl2("https://www.chromium.org/");
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kUrl));
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kUrl2));

  const PermissionsData* permissions = extension->permissions_data();
  auto get_page_access = [&permissions, &extension](const GURL& url) {
    return permissions->GetPageAccess(extension.get(), url, 0, nullptr);
  };

  EXPECT_EQ(PermissionsData::ACCESS_WITHHELD, get_page_access(kUrl));
  EXPECT_EQ(PermissionsData::ACCESS_WITHHELD, get_page_access(kUrl2));

  modifier.GrantHostPermission(kUrl);
  EXPECT_TRUE(modifier.HasGrantedHostPermission(kUrl));
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kUrl2));
  EXPECT_EQ(PermissionsData::ACCESS_ALLOWED, get_page_access(kUrl));
  EXPECT_EQ(PermissionsData::ACCESS_WITHHELD, get_page_access(kUrl2));

  modifier.RemoveGrantedHostPermission(kUrl);
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kUrl));
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kUrl2));
  EXPECT_EQ(PermissionsData::ACCESS_WITHHELD, get_page_access(kUrl));
  EXPECT_EQ(PermissionsData::ACCESS_WITHHELD, get_page_access(kUrl2));
}

// Checks that policy-installed extensions don't have permissions withheld and
// that preferences are correctly recovered in the case of an improper value.
// Fix for crbug.com/629927.
TEST_F(ScriptingPermissionsModifierUnitTest,
       PolicyExtensionsCanExecuteEverywhere) {
  std::unique_ptr<FeatureSwitch::ScopedOverride> switch_override(
      new FeatureSwitch::ScopedOverride(FeatureSwitch::scripts_require_action(),
                                        FeatureSwitch::OVERRIDE_ENABLED));
  InitializeEmptyExtensionService();
  URLPattern all_hosts(URLPattern::SCHEME_ALL, "<all_urls>");
  std::set<URLPattern> all_host_patterns;
  all_host_patterns.insert(all_hosts);
  scoped_refptr<const Extension> extension =
      CreateExtensionWithPermissions(all_host_patterns, all_host_patterns,
                                     Manifest::EXTERNAL_POLICY, "extension");
  PermissionsUpdater(profile()).InitializePermissions(extension.get());

  ScriptingPermissionsModifier modifier(profile(), extension);
  EXPECT_TRUE(modifier.IsAllowedOnAllUrls());

  // Simulate preferences being incorrectly set.
  const char* kAllowedPref = "extension_can_script_all_urls";
  const char* kHasSetPref = "has_set_script_all_urls";
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  prefs->UpdateExtensionPref(extension->id(), kAllowedPref,
                             new base::Value(false));
  prefs->UpdateExtensionPref(extension->id(), kHasSetPref,
                             new base::Value(true));

  // The modifier should still return the correct value and should fix the
  // preferences.
  EXPECT_TRUE(modifier.IsAllowedOnAllUrls());
  bool stored_allowed = false;
  EXPECT_TRUE(
      prefs->ReadPrefAsBoolean(extension->id(), kAllowedPref, &stored_allowed));
  EXPECT_TRUE(stored_allowed);
  bool has_set = false;
  EXPECT_FALSE(
      prefs->ReadPrefAsBoolean(extension->id(), kHasSetPref, &has_set));
}

}  // namespace extensions
