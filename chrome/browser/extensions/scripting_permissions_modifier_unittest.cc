// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_test_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

using permissions_test_util::GetPatternsAsStrings;

std::vector<std::string> GetEffectivePatternsAsStrings(
    const Extension& extension) {
  return GetPatternsAsStrings(
      extension.permissions_data()->active_permissions().effective_hosts());
}

std::vector<std::string> GetScriptablePatternsAsStrings(
    const Extension& extension) {
  return GetPatternsAsStrings(
      extension.permissions_data()->active_permissions().scriptable_hosts());
}

std::vector<std::string> GetExplicitPatternsAsStrings(
    const Extension& extension) {
  return GetPatternsAsStrings(
      extension.permissions_data()->active_permissions().explicit_hosts());
}

void InitializeExtensionPermissions(Profile* profile,
                                    const Extension& extension) {
  PermissionsUpdater updater(profile);
  updater.InitializePermissions(&extension);
  updater.GrantActivePermissions(&extension);
}

using ScriptingPermissionsModifierUnitTest = ExtensionServiceTestBase;

}  // namespace

TEST_F(ScriptingPermissionsModifierUnitTest, GrantAndWithholdHostPermissions) {
  InitializeEmptyExtensionService();

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
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(test_case_name)
            .AddPermissions(test_case)
            .AddContentScript("foo.js", test_case)
            .SetLocation(Manifest::INTERNAL)
            .Build();

    PermissionsUpdater(profile()).InitializePermissions(extension.get());

    const PermissionsData* permissions_data = extension->permissions_data();

    ScriptingPermissionsModifier modifier(profile(), extension);
    ASSERT_TRUE(modifier.CanAffectExtension());

    // By default, all permissions are granted.
    EXPECT_THAT(GetScriptablePatternsAsStrings(*extension),
                testing::UnorderedElementsAreArray(test_case));
    EXPECT_THAT(GetExplicitPatternsAsStrings(*extension),
                testing::UnorderedElementsAreArray(test_case));
    EXPECT_TRUE(
        permissions_data->withheld_permissions().scriptable_hosts().is_empty());
    EXPECT_TRUE(
        permissions_data->withheld_permissions().explicit_hosts().is_empty());

    // Then, withhold host permissions.
    modifier.SetWithholdHostPermissions(true);

    // Note: We don't use URLPatternSet::is_empty() here, since
    // chrome://favicon/ can still be present in the set (it's not really a
    // host permission and isn't withheld). GetPatternsAsStrings() ignores
    // chrome://favicon.
    EXPECT_THAT(GetScriptablePatternsAsStrings(*extension), testing::IsEmpty());
    EXPECT_THAT(GetExplicitPatternsAsStrings(*extension), testing::IsEmpty());

    EXPECT_THAT(
        GetPatternsAsStrings(
            permissions_data->withheld_permissions().scriptable_hosts()),
        testing::UnorderedElementsAreArray(test_case));
    EXPECT_THAT(GetPatternsAsStrings(
                    permissions_data->withheld_permissions().explicit_hosts()),
                testing::UnorderedElementsAreArray(test_case));

    // Finally, re-grant the withheld permissions.
    modifier.SetWithholdHostPermissions(false);

    // We should be back to our initial state - all requested permissions are
    // granted.
    EXPECT_THAT(GetScriptablePatternsAsStrings(*extension),
                testing::UnorderedElementsAreArray(test_case));
    EXPECT_THAT(GetExplicitPatternsAsStrings(*extension),
                testing::UnorderedElementsAreArray(test_case));
    EXPECT_TRUE(
        permissions_data->withheld_permissions().scriptable_hosts().is_empty());
    EXPECT_TRUE(
        permissions_data->withheld_permissions().explicit_hosts().is_empty());
  }
}

TEST_F(ScriptingPermissionsModifierUnitTest, SwitchBehavior) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("a")
          .AddPermission(URLPattern::kAllUrlsPattern)
          .AddContentScript("foo.js", {URLPattern::kAllUrlsPattern})
          .SetLocation(Manifest::INTERNAL)
          .Build();
  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension.get());
  const PermissionsData* permissions_data = extension->permissions_data();

  // By default, the extension should have all its permissions.
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension),
              testing::UnorderedElementsAre(URLPattern::kAllUrlsPattern));
  EXPECT_TRUE(
      permissions_data->withheld_permissions().effective_hosts().is_empty());
  ScriptingPermissionsModifier modifier(profile(), extension);
  EXPECT_FALSE(modifier.HasWithheldHostPermissions());

  // Revoke access.
  modifier.SetWithholdHostPermissions(true);
  EXPECT_TRUE(modifier.HasWithheldHostPermissions());
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());
  EXPECT_THAT(GetPatternsAsStrings(
                  permissions_data->withheld_permissions().effective_hosts()),
              testing::UnorderedElementsAre(URLPattern::kAllUrlsPattern));
}

TEST_F(ScriptingPermissionsModifierUnitTest, GrantHostPermission) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddPermission(URLPattern::kAllUrlsPattern)
          .AddContentScript("foo.js", {URLPattern::kAllUrlsPattern})
          .SetLocation(Manifest::INTERNAL)
          .Build();
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
}

TEST_F(ScriptingPermissionsModifierUnitTest,
       ExtensionsInitializedWithSavedRuntimeGrantedHostPermissionsAcrossLoad) {
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

// Test ScriptingPermissionsModifier::RemoveAllGrantedHostPermissions() revokes
// hosts granted through the ScriptingPermissionsModifier.
TEST_F(ScriptingPermissionsModifierUnitTest,
       RemoveAllGrantedHostPermissions_GrantedHosts) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddPermission("<all_urls>").Build();
  ScriptingPermissionsModifier modifier(profile(), extension.get());

  modifier.SetWithholdHostPermissions(true);

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());

  modifier.GrantHostPermission(GURL("https://example.com"));
  modifier.GrantHostPermission(GURL("https://chromium.org"));

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension),
              testing::UnorderedElementsAre("https://example.com/*",
                                            "https://chromium.org/*"));

  modifier.RemoveAllGrantedHostPermissions();
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());
}

// Test ScriptingPermissionsModifier::RemoveAllGrantedHostPermissions() revokes
// hosts granted through the ScriptingPermissionsModifier for extensions that
// don't request <all_urls>.
TEST_F(ScriptingPermissionsModifierUnitTest,
       RemoveAllGrantedHostPermissions_GrantedHostsForNonAllUrlsExtension) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test")
          .AddPermissions({"https://example.com/*", "https://chromium.org/*"})
          .Build();
  ScriptingPermissionsModifier modifier(profile(), extension.get());

  modifier.SetWithholdHostPermissions(true);

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());

  modifier.GrantHostPermission(GURL("https://example.com"));
  modifier.GrantHostPermission(GURL("https://chromium.org"));

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension),
              testing::UnorderedElementsAre("https://example.com/*",
                                            "https://chromium.org/*"));

  modifier.RemoveAllGrantedHostPermissions();
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());
}

// Test ScriptingPermissionsModifier::RemoveAllGrantedHostPermissions() revokes
// granted optional host permissions.
TEST_F(ScriptingPermissionsModifierUnitTest,
       RemoveAllGrantedHostPermissions_GrantedOptionalPermissions) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test")
          .SetManifestKey("optional_permissions",
                          ListBuilder().Append("https://example.com/*").Build())
          .Build();

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());

  {
    // Simulate adding an optional permission, which should also be revokable.
    URLPatternSet patterns;
    patterns.AddPattern(URLPattern(Extension::kValidHostPermissionSchemes,
                                   "https://example.com/*"));
    permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
        profile(), *extension,
        PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                      std::move(patterns), URLPatternSet()));
  }

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension),
              testing::UnorderedElementsAre("https://example.com/*"));

  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.RemoveAllGrantedHostPermissions();
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());
}

// Tests granting runtime permissions for a full host when the extension only
// wants to run on a subset of that host.
TEST_F(ScriptingPermissionsModifierUnitTest,
       GrantingHostPermissionsBeyondRequested) {
  InitializeEmptyExtensionService();

  DictionaryBuilder content_script;
  content_script
      .Set("matches", ListBuilder().Append("https://google.com/maps").Build())
      .Set("js", ListBuilder().Append("foo.js").Build());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test")
          .SetManifestKey("content_scripts",
                          ListBuilder().Append(content_script.Build()).Build())
          .Build();

  // At installation, all permissions granted.
  ScriptingPermissionsModifier modifier(profile(), extension);
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension),
              testing::UnorderedElementsAre("https://google.com/maps"));

  // Withhold host permissions.
  modifier.SetWithholdHostPermissions(true);
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());

  // Grant the requested host permission. We grant origins (rather than just
  // paths), but we don't over-grant permissions to the actual extension object.
  // The active permissions on the extension should be restricted to the
  // permissions explicitly requested (google.com/maps), but the granted
  // permissions in preferences will be the full host (google.com).
  modifier.GrantHostPermission(GURL("https://google.com/maps"));
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension),
              testing::UnorderedElementsAre("https://google.com/maps"));
  EXPECT_THAT(GetPatternsAsStrings(
                  modifier.GetRevokablePermissions()->effective_hosts()),
              // Subtle: revokable permissions include permissions either in
              // the runtime granted permissions preference or active on the
              // extension object. In this case, that includes both google.com/*
              // and google.com/maps.
              testing::UnorderedElementsAre("https://google.com/maps",
                                            "https://google.com/*"));

  // Remove the granted permission. This should remove the permission from both
  // the active permissions on the extension object and the entry in the
  // preferences.
  modifier.RemoveAllGrantedHostPermissions();
  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());
  EXPECT_THAT(GetPatternsAsStrings(
                  modifier.GetRevokablePermissions()->effective_hosts()),
              testing::IsEmpty());
}

TEST_F(ScriptingPermissionsModifierUnitTest, GetSiteAccess_AllHostsExtension) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddPermission("<all_urls>").Build();
  InitializeExtensionPermissions(profile(), *extension);
  ScriptingPermissionsModifier modifier(profile(), extension.get());

  const GURL example_com("https://www.example.com");
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(example_com);
    EXPECT_TRUE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_TRUE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }

  // Chrome pages should be restricted, and the extension shouldn't have access
  // to them granted or withheld.
  const GURL chrome_extensions("chrome://extensions");
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(chrome_extensions);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_TRUE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }

  // Other restricted urls should also be protected, and the extension shouldn't
  // have or want access.
  const GURL webstore = ExtensionsClient::Get()->GetWebstoreBaseURL();
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(webstore);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_TRUE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }

  modifier.SetWithholdHostPermissions(true);
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(example_com);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_TRUE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_TRUE(site_access.withheld_all_sites_access);
  }

  // Restricted sites should not be considered "withheld".
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(chrome_extensions);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_TRUE(site_access.withheld_all_sites_access);
  }

  modifier.GrantHostPermission(example_com);
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(example_com);
    EXPECT_TRUE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_TRUE(site_access.withheld_all_sites_access);
  }

  const GURL google_com("https://google.com");
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(google_com);
    EXPECT_TRUE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_TRUE(site_access.withheld_all_sites_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
  }
}

TEST_F(ScriptingPermissionsModifierUnitTest,
       GetSiteAccess_AllHostsLikeExtension) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddPermission("*://*.com/*").Build();
  InitializeExtensionPermissions(profile(), *extension);
  ScriptingPermissionsModifier modifier(profile(), extension.get());

  const GURL example_com("https://www.example.com");
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(example_com);
    EXPECT_TRUE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_TRUE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }

  const GURL google_org("https://google.org");
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(google_org);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_TRUE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }

  modifier.SetWithholdHostPermissions(true);

  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(example_com);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_TRUE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_TRUE(site_access.withheld_all_sites_access);
  }
}

TEST_F(ScriptingPermissionsModifierUnitTest, GetSiteAccess_SpecificSites) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddPermission("*://*.example.com/*")
          .Build();
  InitializeExtensionPermissions(profile(), *extension);
  ScriptingPermissionsModifier modifier(profile(), extension.get());

  const GURL example_com("https://www.example.com");
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(example_com);
    EXPECT_TRUE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }

  const GURL google_com("https://google.com");
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(google_com);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }

  modifier.SetWithholdHostPermissions(true);
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(example_com);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_TRUE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }
}

TEST_F(ScriptingPermissionsModifierUnitTest,
       GetSiteAccess_GrantedButNotRequested) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddPermission("*://*.example.com/*")
          .Build();
  InitializeExtensionPermissions(profile(), *extension);
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  const URLPatternSet google_com_pattern({URLPattern(
      Extension::kValidHostPermissionSchemes, "https://google.com/*")});
  ExtensionPrefs::Get(profile())->AddRuntimeGrantedPermissions(
      extension->id(),
      PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                    google_com_pattern.Clone(), google_com_pattern.Clone()));

  const GURL google_com("https://google.com");
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(google_com);
    // The has_access and withheld_access bits should be set appropriately, even
    // if the extension has access to a site it didn't request.
    EXPECT_TRUE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }
}

// Tests that for the purposes of displaying an extension's site access to the
// user (or granting/revoking permissions), we ignore paths in the URL.
TEST_F(ScriptingPermissionsModifierUnitTest, GetSiteAccess_IgnorePaths) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddContentScript("foo.js", {"https://www.example.com/foo"})
          .SetLocation(Manifest::INTERNAL)
          .Build();
  InitializeExtensionPermissions(profile(), *extension);

  ScriptingPermissionsModifier modifier(profile(), extension.get());

  const GURL example_com("https://www.example.com/bar");
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(example_com);
    // Even though the path doesn't exactly match one in the content scripts,
    // the domain is requested, and thus we treat it as if the site was
    // requested.
    EXPECT_TRUE(site_access.has_site_access);
    EXPECT_FALSE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }

  modifier.SetWithholdHostPermissions(true);
  {
    const ScriptingPermissionsModifier::SiteAccess site_access =
        modifier.GetSiteAccess(example_com);
    EXPECT_FALSE(site_access.has_site_access);
    EXPECT_TRUE(site_access.withheld_site_access);
    EXPECT_FALSE(site_access.has_all_sites_access);
    EXPECT_FALSE(site_access.withheld_all_sites_access);
  }
}

// Tests that removing access for a host removes all patterns that grant access
// to that host.
TEST_F(ScriptingPermissionsModifierUnitTest,
       RemoveHostAccess_RemovesOverlappingPatterns) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddPermission("*://*/*").Build();
  InitializeExtensionPermissions(profile(), *extension);
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  const URLPattern all_com_pattern(Extension::kValidHostPermissionSchemes,
                                   "https://*.com/*");
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension,
      PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                    URLPatternSet({all_com_pattern}), URLPatternSet()));

  // Removing example.com access should result in *.com access being revoked,
  // since that is the pattern that grants access to example.com.
  const GURL example_com("https://www.example.com");
  EXPECT_TRUE(modifier.HasGrantedHostPermission(example_com));
  modifier.RemoveGrantedHostPermission(example_com);
  EXPECT_FALSE(modifier.HasGrantedHostPermission(example_com));
  EXPECT_TRUE(ExtensionPrefs::Get(profile())
                  ->GetRuntimeGrantedPermissions(extension->id())
                  ->explicit_hosts()
                  .is_empty());
}

// Test that granting <all_urls> as an optional permission, and then revoking
// it, behaves properly. Regression test for https://crbug.com/930062.
TEST_F(ScriptingPermissionsModifierUnitTest,
       RemoveAllURLsGrantedOptionalPermission) {
  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .SetManifestKey("optional_permissions",
                          ListBuilder().Append("<all_urls>").Build())
          .Build();
  InitializeExtensionPermissions(profile(), *extension);

  // Also verify the extension doesn't have file access, so that <all_urls>
  // shouldn't match file URLs either.
  EXPECT_FALSE(util::AllowFileAccess(extension->id(), profile()));

  {
    PermissionsUpdater updater(profile());
    updater.GrantOptionalPermissions(
        *extension, PermissionsParser::GetOptionalPermissions(extension.get()),
        base::DoNothing());
  }

  ScriptingPermissionsModifier(profile(), extension.get())
      .SetWithholdHostPermissions(true);

  EXPECT_THAT(GetEffectivePatternsAsStrings(*extension), testing::IsEmpty());
}

}  // namespace extensions
