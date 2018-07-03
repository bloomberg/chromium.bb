// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"

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

class RuntimeHostPermissionsEnabledScope {
 public:
  RuntimeHostPermissionsEnabledScope() {
    feature_list_.InitAndEnableFeature(features::kRuntimeHostPermissions);
  }
  ~RuntimeHostPermissionsEnabledScope() {}

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(RuntimeHostPermissionsEnabledScope);
};

using ScriptingPermissionsModifierUnitTest = ExtensionServiceTestBase;

}  // namespace

TEST_F(ScriptingPermissionsModifierUnitTest, GrantAndWithholdHostPermissions) {
  InitializeEmptyExtensionService();

  // Permissions can only be withheld with the appropriate feature turned on.
  RuntimeHostPermissionsEnabledScope enabled_scope;

  std::vector<std::string> test_cases[] = {
      {"http://www.google.com/*"},
      {"http://*/*"},
      {"<all_urls>"},
      {"http://*.com/*"},
      {"http://google.com/*", "<all_urls>"},
  };

  for (const auto& test_case : test_cases) {
    std::string test_case_name = base::JoinString(test_case, ",");
    SCOPED_TRACE(test_case_name);
    std::set<URLPattern> patterns;
    for (const auto& pattern : test_case)
      patterns.insert(URLPattern(URLPattern::SCHEME_ALL, pattern));
    scoped_refptr<const Extension> extension = CreateExtensionWithPermissions(
        patterns, patterns, Manifest::INTERNAL, test_case_name);

    PermissionsUpdater(profile()).InitializePermissions(extension.get());

    const PermissionsData* permissions_data = extension->permissions_data();

    ScriptingPermissionsModifier modifier(profile(), extension);
    ASSERT_TRUE(modifier.CanAffectExtension());

    // By default, all permissions are granted.
    EXPECT_TRUE(SetsAreEqual(
        permissions_data->active_permissions().scriptable_hosts().patterns(),
        patterns));
    EXPECT_TRUE(SetsAreEqual(
        permissions_data->active_permissions().explicit_hosts().patterns(),
        patterns));
    EXPECT_TRUE(permissions_data->withheld_permissions()
                    .scriptable_hosts()
                    .patterns()
                    .empty());
    EXPECT_TRUE(permissions_data->withheld_permissions()
                    .explicit_hosts()
                    .patterns()
                    .empty());

    // Then, withhold host permissions.
    modifier.SetWithholdHostPermissions(true);

    EXPECT_TRUE(permissions_data->active_permissions()
                    .scriptable_hosts()
                    .patterns()
                    .empty());
    // Note: can't use explicit_hosts().empty() here, since chrome://favicon/
    // will remain will still be present in explicit_hosts() (it's not really a
    // host permission and isn't withheld). SetsAreEqual() ignores
    // chrome://favicon in its checks.
    EXPECT_TRUE(SetsAreEqual(
        permissions_data->active_permissions().explicit_hosts().patterns(),
        std::set<URLPattern>()));

    EXPECT_TRUE(SetsAreEqual(
        permissions_data->withheld_permissions().scriptable_hosts().patterns(),
        patterns));
    EXPECT_TRUE(SetsAreEqual(
        permissions_data->withheld_permissions().explicit_hosts().patterns(),
        patterns));

    // Finally, re-grant the withheld permissions.
    modifier.SetWithholdHostPermissions(false);

    // We should be back to our initial state - all requested permissions are
    // granted.
    EXPECT_TRUE(SetsAreEqual(
        permissions_data->active_permissions().scriptable_hosts().patterns(),
        patterns));
    EXPECT_TRUE(SetsAreEqual(
        permissions_data->active_permissions().explicit_hosts().patterns(),
        patterns));
    EXPECT_TRUE(permissions_data->withheld_permissions()
                    .scriptable_hosts()
                    .patterns()
                    .empty());
    EXPECT_TRUE(permissions_data->withheld_permissions()
                    .explicit_hosts()
                    .patterns()
                    .empty());
  }
}

TEST_F(ScriptingPermissionsModifierUnitTest, SwitchBehavior) {
  InitializeEmptyExtensionService();

  // Permissions can only be withheld with the appropriate feature turned on.
  auto enabled_scope = std::make_unique<RuntimeHostPermissionsEnabledScope>();

  URLPattern all_hosts(URLPattern::SCHEME_ALL, "<all_urls>");
  std::set<URLPattern> all_host_patterns;
  all_host_patterns.insert(all_hosts);

  scoped_refptr<const Extension> extension = CreateExtensionWithPermissions(
      all_host_patterns, all_host_patterns, Manifest::INTERNAL, "a");
  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension.get());
  const PermissionsData* permissions_data = extension->permissions_data();

  // By default, the extension should have all its permissions.
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().scriptable_hosts().patterns(),
      all_host_patterns));
  EXPECT_TRUE(
      permissions_data->withheld_permissions().scriptable_hosts().is_empty());
  ScriptingPermissionsModifier modifier(profile(), extension);
  EXPECT_FALSE(modifier.HasWithheldHostPermissions());

  // Revoke access.
  modifier.SetWithholdHostPermissions(true);
  EXPECT_TRUE(modifier.HasWithheldHostPermissions());
  EXPECT_TRUE(
      permissions_data->active_permissions().scriptable_hosts().is_empty());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->withheld_permissions().scriptable_hosts().patterns(),
      all_host_patterns));

  // Remove the switch. The extension should have permission again.
  enabled_scope.reset();
  updater.InitializePermissions(extension.get());
  EXPECT_FALSE(modifier.CanAffectExtension());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions().scriptable_hosts().patterns(),
      all_host_patterns));
  EXPECT_TRUE(
      permissions_data->withheld_permissions().scriptable_hosts().is_empty());

  // Reapply the switch; the extension should go back to having permissions
  // withheld.
  enabled_scope = std::make_unique<RuntimeHostPermissionsEnabledScope>();
  updater.InitializePermissions(extension.get());
  EXPECT_TRUE(modifier.HasWithheldHostPermissions());
  EXPECT_TRUE(
      permissions_data->active_permissions().scriptable_hosts().is_empty());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->withheld_permissions().scriptable_hosts().patterns(),
      all_host_patterns));
}

TEST_F(ScriptingPermissionsModifierUnitTest, GrantHostPermission) {
  InitializeEmptyExtensionService();

  // Permissions can only be withheld with the appropriate feature turned on.
  RuntimeHostPermissionsEnabledScope enabled_scope;

  URLPattern all_hosts(URLPattern::SCHEME_ALL, "<all_urls>");
  std::set<URLPattern> all_host_patterns;
  all_host_patterns.insert(all_hosts);

  scoped_refptr<const Extension> extension = CreateExtensionWithPermissions(
      all_host_patterns, all_host_patterns, Manifest::INTERNAL, "extension");
  PermissionsUpdater(profile()).InitializePermissions(extension.get());

  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  const GURL kUrl("https://www.google.com/");
  const GURL kUrl2("https://www.chromium.org/");
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kUrl));
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kUrl2));

  const PermissionsData* permissions = extension->permissions_data();
  auto get_page_access = [&permissions](const GURL& url) {
    return permissions->GetPageAccess(url, 0, nullptr);
  };

  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_page_access(kUrl));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_page_access(kUrl2));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  {
    std::unique_ptr<const PermissionSet> permissions =
        prefs->GetRuntimeGrantedPermissions(extension->id());
    EXPECT_FALSE(permissions->effective_hosts().MatchesURL(kUrl));
    EXPECT_FALSE(permissions->effective_hosts().MatchesURL(kUrl2));
  }

  modifier.GrantHostPermission(kUrl);
  EXPECT_TRUE(modifier.HasGrantedHostPermission(kUrl));
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kUrl2));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed, get_page_access(kUrl));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_page_access(kUrl2));
  {
    std::unique_ptr<const PermissionSet> permissions =
        prefs->GetRuntimeGrantedPermissions(extension->id());
    EXPECT_TRUE(permissions->effective_hosts().MatchesURL(kUrl));
    EXPECT_FALSE(permissions->effective_hosts().MatchesURL(kUrl2));
  }

  modifier.RemoveGrantedHostPermission(kUrl);
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kUrl));
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kUrl2));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_page_access(kUrl));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld, get_page_access(kUrl2));
  {
    std::unique_ptr<const PermissionSet> permissions =
        prefs->GetRuntimeGrantedPermissions(extension->id());
    EXPECT_FALSE(permissions->effective_hosts().MatchesURL(kUrl));
    EXPECT_FALSE(permissions->effective_hosts().MatchesURL(kUrl2));
  }
}

TEST_F(ScriptingPermissionsModifierUnitTest, CanAffectExtensionByLocation) {
  auto enabled_scope = std::make_unique<RuntimeHostPermissionsEnabledScope>();

  InitializeEmptyExtensionService();

  struct {
    Manifest::Location location;
    bool can_be_affected;
  } test_cases[] = {
      {Manifest::INTERNAL, true},   {Manifest::EXTERNAL_PREF, true},
      {Manifest::UNPACKED, true},   {Manifest::EXTERNAL_POLICY_DOWNLOAD, false},
      {Manifest::COMPONENT, false},
  };

  for (const auto& test_case : test_cases) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test")
            .SetLocation(test_case.location)
            .AddPermission("<all_urls>")
            .Build();
    EXPECT_EQ(test_case.can_be_affected,
              ScriptingPermissionsModifier(profile(), extension.get())
                  .CanAffectExtension())
        << test_case.location;
  }

  enabled_scope.reset();

  // With the feature disabled, no extension should be able to be affected.
  for (const auto& test_case : test_cases) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test")
            .SetLocation(test_case.location)
            .AddPermission("<all_urls>")
            .Build();
    EXPECT_FALSE(ScriptingPermissionsModifier(profile(), extension.get())
                     .CanAffectExtension())
        << test_case.location;
  }
}

TEST_F(ScriptingPermissionsModifierUnitTest,
       ExtensionsInitializedWithSavedRuntimeGrantedHostPermissionsAcrossLoad) {
  // Permissions can only be withheld with the appropriate feature turned on.
  RuntimeHostPermissionsEnabledScope enabled_scope;

  InitializeEmptyExtensionService();

  const GURL kExampleCom("https://example.com/");
  const GURL kChromiumOrg("https://chromium.org/");
  const URLPatternSet kExampleComPatternSet({URLPattern(
      Extension::kValidHostPermissionSchemes, "https://example.com/")});

  TestExtensionDir test_extension_dir;
  test_extension_dir.WriteManifest(
      R"({
           "name": "foo",
           "manifest_version": 2,
           "version": "1",
           "permissions": ["<all_urls>"]
         })");
  ChromeTestExtensionLoader loader(profile());
  loader.set_grant_permissions(true);
  scoped_refptr<const Extension> extension =
      loader.LoadExtension(test_extension_dir.UnpackedPath());

  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .explicit_hosts()
                  .MatchesURL(kExampleCom));
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .explicit_hosts()
                  .MatchesURL(kChromiumOrg));

  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);
  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .explicit_hosts()
                   .MatchesURL(kExampleCom));
  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .explicit_hosts()
                   .MatchesURL(kChromiumOrg));

  ScriptingPermissionsModifier(profile(), extension)
      .GrantHostPermission(kExampleCom);
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .explicit_hosts()
                  .MatchesURL(kExampleCom));
  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .explicit_hosts()
                   .MatchesURL(kChromiumOrg));

  {
    TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()));
    service()->ReloadExtension(extension->id());
    extension = base::WrapRefCounted(observer.WaitForExtensionLoaded());
  }
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .explicit_hosts()
                  .MatchesURL(kExampleCom));
  EXPECT_FALSE(extension->permissions_data()
                   ->active_permissions()
                   .explicit_hosts()
                   .MatchesURL(kChromiumOrg));
}

}  // namespace extensions
