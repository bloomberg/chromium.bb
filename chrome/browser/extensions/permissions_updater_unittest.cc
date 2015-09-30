// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using extension_test_util::LoadManifest;

namespace extensions {

namespace {

scoped_refptr<const Extension> CreateExtensionWithOptionalPermissions(
    scoped_ptr<base::Value> optional_permissions,
    scoped_ptr<base::Value> permissions,
    const std::string& name) {
  return ExtensionBuilder()
      .SetLocation(Manifest::INTERNAL)
      .SetManifest(
          DictionaryBuilder()
              .Set("name", name)
              .Set("description", "foo")
              .Set("manifest_version", 2)
              .Set("version", "0.1.2.3")
              .Set("permissions", permissions.Pass())
              .Set("optional_permissions", optional_permissions.Pass()))
      .SetID(crx_file::id_util::GenerateId(name))
      .Build();
}

scoped_refptr<const Extension> CreateExtensionWithPermissions(
    const std::set<URLPattern>& scriptable_hosts,
    const std::set<URLPattern>& explicit_hosts,
    Manifest::Location location,
    const std::string& name) {
  ListBuilder scriptable_host_list;
  for (std::set<URLPattern>::const_iterator pattern = scriptable_hosts.begin();
       pattern != scriptable_hosts.end();
       ++pattern) {
    scriptable_host_list.Append(pattern->GetAsString());
  }

  ListBuilder explicit_host_list;
  for (std::set<URLPattern>::const_iterator pattern = explicit_hosts.begin();
       pattern != explicit_hosts.end();
       ++pattern) {
    explicit_host_list.Append(pattern->GetAsString());
  }

  DictionaryBuilder script;
  script.Set("matches", scriptable_host_list.Pass())
      .Set("js", ListBuilder().Append("foo.js"));

  return ExtensionBuilder()
      .SetLocation(location)
      .SetManifest(
           DictionaryBuilder()
               .Set("name", name)
               .Set("description", "foo")
               .Set("manifest_version", 2)
               .Set("version", "0.1.2.3")
               .Set("content_scripts", ListBuilder().Append(script.Pass()))
               .Set("permissions", explicit_host_list.Pass()))
      .SetID(crx_file::id_util::GenerateId(name))
      .Build();
}

testing::AssertionResult SetsAreEqual(const std::set<URLPattern>& set1,
                                      const std::set<URLPattern>& set2) {
  // Take the (set1 - set2) U (set2 - set1). This is then the set of all
  // elements which are in either set1 or set2, but not both.
  // If the sets are equal, this is none.
  std::set<URLPattern> difference = base::STLSetUnion<std::set<URLPattern> >(
      base::STLSetDifference<std::set<URLPattern> >(set1, set2),
      base::STLSetDifference<std::set<URLPattern> >(set2, set1));

  std::string error;
  for (std::set<URLPattern>::const_iterator iter = difference.begin();
       iter != difference.end();
       ++iter) {
    if (iter->GetAsString() == "chrome://favicon/*")
      continue;  // Grr... This is auto-added for extensions with <all_urls>
    error = base::StringPrintf("%s\n%s contains %s and the other does not.",
                               error.c_str(),
                               (set1.count(*iter) ? "Set1" : "Set2"),
                               iter->GetAsString().c_str());
  }

  if (!error.empty())
    return testing::AssertionFailure() << error;
  return testing::AssertionSuccess();
}

// A helper class that listens for NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED.
class PermissionsUpdaterListener : public content::NotificationObserver {
 public:
  PermissionsUpdaterListener()
      : received_notification_(false), waiting_(false) {
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
                   content::NotificationService::AllSources());
  }

  void Reset() {
    received_notification_ = false;
    waiting_ = false;
    extension_ = NULL;
    permissions_ = NULL;
  }

  void Wait() {
    if (received_notification_)
      return;

    waiting_ = true;
    base::RunLoop run_loop;
    run_loop.Run();
  }

  bool received_notification() const { return received_notification_; }
  const Extension* extension() const { return extension_.get(); }
  const PermissionSet* permissions() const { return permissions_.get(); }
  UpdatedExtensionPermissionsInfo::Reason reason() const { return reason_; }

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    received_notification_ = true;
    UpdatedExtensionPermissionsInfo* info =
        content::Details<UpdatedExtensionPermissionsInfo>(details).ptr();

    extension_ = info->extension;
    permissions_ = info->permissions.Clone();
    reason_ = info->reason;

    if (waiting_) {
      waiting_ = false;
      base::MessageLoopForUI::current()->Quit();
    }
  }

  bool received_notification_;
  bool waiting_;
  content::NotificationRegistrar registrar_;
  scoped_refptr<const Extension> extension_;
  scoped_ptr<const PermissionSet> permissions_;
  UpdatedExtensionPermissionsInfo::Reason reason_;
};

class PermissionsUpdaterTest : public ExtensionServiceTestBase {
};

scoped_refptr<Extension> LoadOurManifest() {
  base::FilePath path;
  path = path.AppendASCII("api_test")
      .AppendASCII("permissions")
      .AppendASCII("optional");
  return LoadManifest(path.AsUTF8Unsafe(),
                      "manifest.json",
                      Manifest::INTERNAL,
                      Extension::NO_FLAGS);
}

void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

}  // namespace

// Test that the PermissionUpdater can correctly add and remove active
// permissions. This tests all of PermissionsUpdater's public methods because
// GrantActivePermissions and SetPermissions are used by AddPermissions.
TEST_F(PermissionsUpdaterTest, AddAndRemovePermissions) {
  InitializeEmptyExtensionService();

  // Load the test extension.
  scoped_refptr<Extension> extension = LoadOurManifest();
  ASSERT_TRUE(extension.get());

  APIPermissionSet default_apis;
  default_apis.insert(APIPermission::kManagement);
  ManifestPermissionSet empty_manifest_permissions;

  URLPatternSet default_hosts;
  AddPattern(&default_hosts, "http://a.com/*");
  PermissionSet default_permissions(default_apis, empty_manifest_permissions,
                                    default_hosts, URLPatternSet());

  // Make sure it loaded properly.
  ASSERT_EQ(default_permissions,
            extension->permissions_data()->active_permissions());

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_.get());
  scoped_ptr<const PermissionSet> active_permissions;
  scoped_ptr<const PermissionSet> granted_permissions;

  // Add a few permissions.
  APIPermissionSet apis;
  apis.insert(APIPermission::kNotifications);
  URLPatternSet hosts;
  AddPattern(&hosts, "http://*.c.com/*");

  {
    PermissionSet delta(apis, empty_manifest_permissions, hosts,
                        URLPatternSet());

  PermissionsUpdaterListener listener;
  PermissionsUpdater(profile_.get()).AddPermissions(extension.get(), delta);

  listener.Wait();

  // Verify that the permission notification was sent correctly.
  ASSERT_TRUE(listener.received_notification());
  ASSERT_EQ(extension.get(), listener.extension());
  ASSERT_EQ(UpdatedExtensionPermissionsInfo::ADDED, listener.reason());
  ASSERT_EQ(delta, *listener.permissions());

  // Make sure the extension's active permissions reflect the change.
  active_permissions = PermissionSet::CreateUnion(default_permissions, delta);
  ASSERT_EQ(*active_permissions.get(),
            extension->permissions_data()->active_permissions());

  // Verify that the new granted and active permissions were also stored
  // in the extension preferences. In this case, the granted permissions should
  // be equal to the active permissions.
  ASSERT_EQ(*active_permissions.get(),
            *prefs->GetActivePermissions(extension->id()));
  granted_permissions = active_permissions->Clone();
  ASSERT_EQ(*granted_permissions,
            *prefs->GetGrantedPermissions(extension->id()));
  }

  {
  // In the second part of the test, we'll remove the permissions that we
  // just added except for 'notifications'.
  apis.erase(APIPermission::kNotifications);
  PermissionSet delta(apis, empty_manifest_permissions, hosts, URLPatternSet());

  PermissionsUpdaterListener listener;
  PermissionsUpdater(profile_.get())
      .RemovePermissions(extension.get(), delta,
                         PermissionsUpdater::REMOVE_SOFT);
  listener.Wait();

  // Verify that the notification was correct.
  ASSERT_TRUE(listener.received_notification());
  ASSERT_EQ(extension.get(), listener.extension());
  ASSERT_EQ(UpdatedExtensionPermissionsInfo::REMOVED, listener.reason());
  ASSERT_EQ(delta, *listener.permissions());

  // Make sure the extension's active permissions reflect the change.
  active_permissions =
      PermissionSet::CreateDifference(*active_permissions, delta);
  ASSERT_EQ(*active_permissions,
            extension->permissions_data()->active_permissions());

  // Verify that the extension prefs hold the new active permissions and the
  // same granted permissions.
  ASSERT_EQ(*active_permissions, *prefs->GetActivePermissions(extension->id()));

  ASSERT_EQ(*granted_permissions,
            *prefs->GetGrantedPermissions(extension->id()));
  }
}

TEST_F(PermissionsUpdaterTest, WithholdAllHosts) {
  InitializeEmptyExtensionService();

  // Permissions are only withheld with the appropriate switch turned on.
  scoped_ptr<FeatureSwitch::ScopedOverride> switch_override(
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

  std::set<URLPattern> all_patterns = base::STLSetUnion<std::set<URLPattern> >(
      all_host_patterns, safe_patterns);

  scoped_refptr<const Extension> extension = CreateExtensionWithPermissions(
      all_patterns, all_patterns, Manifest::INTERNAL, "a");
  const PermissionsData* permissions_data = extension->permissions_data();
  PermissionsUpdater updater(profile_.get());
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

  // Then, we grant the withheld all-hosts permissions.
  updater.GrantWithheldImpliedAllHosts(extension.get());
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
  updater.WithholdImpliedAllHosts(extension.get());

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
  extension = CreateExtensionWithPermissions(
      all_patterns, all_patterns, Manifest::COMPONENT, "b");
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
  extension = CreateExtensionWithPermissions(
      all_patterns, all_patterns, Manifest::INTERNAL, "c");
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
TEST_F(PermissionsUpdaterTest, WithholdAllHostsWithTransientSwitch) {
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
  EXPECT_TRUE(util::AllowedScriptingOnAllUrls(extension_a->id(), profile()));

  // Enable the switch, and re-init permission for the extension.
  scoped_ptr<FeatureSwitch::ScopedOverride> switch_override(
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
  EXPECT_TRUE(util::AllowedScriptingOnAllUrls(extension_a->id(), profile()));

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
  EXPECT_FALSE(util::AllowedScriptingOnAllUrls(extension_b->id(), profile()));

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
  EXPECT_FALSE(util::AllowedScriptingOnAllUrls(extension_b->id(), profile()));
}

TEST_F(PermissionsUpdaterTest, RevokingPermissions) {
  InitializeEmptyExtensionService();

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  auto api_permission_set = [](APIPermission::ID id) {
    APIPermissionSet apis;
    apis.insert(id);
    return make_scoped_ptr(new PermissionSet(apis, ManifestPermissionSet(),
                                             URLPatternSet(), URLPatternSet()));
  };

  auto url_permission_set = [](const GURL& url) {
    URLPatternSet set;
    URLPattern pattern(URLPattern::SCHEME_ALL, url.spec());
    set.AddPattern(pattern);
    return make_scoped_ptr(new PermissionSet(
        APIPermissionSet(), ManifestPermissionSet(), set, URLPatternSet()));
  };

  {
    // Test revoking optional permissions.
    ListBuilder optional_permissions;
    optional_permissions.Append("tabs").Append("cookies").Append("management");
    ListBuilder required_permissions;
    required_permissions.Append("topSites");
    scoped_refptr<const Extension> extension =
        CreateExtensionWithOptionalPermissions(optional_permissions.Build(),
                                               required_permissions.Build(),
                                               "My Extension");

    PermissionsUpdater updater(profile());
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())->IsEmpty());

    // Add the optional "cookies" permission.
    updater.AddPermissions(extension.get(),
                           *api_permission_set(APIPermission::kCookie));
    const PermissionsData* permissions = extension->permissions_data();
    // The extension should have the permission in its active permissions and
    // its granted permissions (stored in prefs). And, the permission should
    // be revokable.
    EXPECT_TRUE(permissions->HasAPIPermission(APIPermission::kCookie));
    scoped_ptr<const PermissionSet> granted_permissions =
        prefs->GetGrantedPermissions(extension->id());
    EXPECT_TRUE(granted_permissions->HasAPIPermission(APIPermission::kCookie));
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())
                    ->HasAPIPermission(APIPermission::kCookie));

    // Repeat with "tabs".
    updater.AddPermissions(extension.get(),
                           *api_permission_set(APIPermission::kTab));
    EXPECT_TRUE(permissions->HasAPIPermission(APIPermission::kTab));
    granted_permissions = prefs->GetGrantedPermissions(extension->id());
    EXPECT_TRUE(granted_permissions->HasAPIPermission(APIPermission::kTab));
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())
                    ->HasAPIPermission(APIPermission::kTab));

    // Remove the "tabs" permission. The extension should no longer have it
    // in its active or granted permissions, and it shouldn't be revokable.
    // The extension should still have the "cookies" permission.
    updater.RemovePermissions(extension.get(),
                              *api_permission_set(APIPermission::kTab),
                              PermissionsUpdater::REMOVE_HARD);
    EXPECT_FALSE(permissions->HasAPIPermission(APIPermission::kTab));
    granted_permissions = prefs->GetGrantedPermissions(extension->id());
    EXPECT_FALSE(granted_permissions->HasAPIPermission(APIPermission::kTab));
    EXPECT_FALSE(updater.GetRevokablePermissions(extension.get())
                     ->HasAPIPermission(APIPermission::kTab));
    EXPECT_TRUE(permissions->HasAPIPermission(APIPermission::kCookie));
    granted_permissions = prefs->GetGrantedPermissions(extension->id());
    EXPECT_TRUE(granted_permissions->HasAPIPermission(APIPermission::kCookie));
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())
                    ->HasAPIPermission(APIPermission::kCookie));
  }

  {
    // Test revoking non-optional host permissions with click-to-script.
    FeatureSwitch::ScopedOverride scoped_override(
        FeatureSwitch::scripts_require_action(), true);
    ListBuilder optional_permissions;
    optional_permissions.Append("tabs");
    ListBuilder required_permissions;
    required_permissions.Append("topSites")
        .Append("http://*/*")
        .Append("http://*.google.com/*");
    scoped_refptr<const Extension> extension =
        CreateExtensionWithOptionalPermissions(optional_permissions.Build(),
                                               required_permissions.Build(),
                                               "My Extension");
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(extension.get());

    // By default, all-hosts was withheld, so the extension shouldn't have
    // access to any site (like foo.com).
    const GURL kOrigin("http://foo.com");
    EXPECT_FALSE(extension->permissions_data()
                     ->active_permissions()
                     .HasExplicitAccessToOrigin(kOrigin));
    EXPECT_TRUE(extension->permissions_data()
                    ->withheld_permissions()
                    .HasExplicitAccessToOrigin(kOrigin));

    const GURL kRequiredOrigin("http://www.google.com/");
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .HasExplicitAccessToOrigin(kRequiredOrigin));
    EXPECT_FALSE(updater.GetRevokablePermissions(extension.get())
                     ->HasExplicitAccessToOrigin(kRequiredOrigin));

    // Give the extension access to foo.com. Now, the foo.com permission should
    // be revokable.
    updater.AddPermissions(extension.get(), *url_permission_set(kOrigin));
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .HasExplicitAccessToOrigin(kOrigin));
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())
                    ->HasExplicitAccessToOrigin(kOrigin));

    // Revoke the foo.com permission. The extension should no longer have
    // access to foo.com, and the revokable permissions should be empty.
    updater.RemovePermissions(extension.get(), *url_permission_set(kOrigin),
                              PermissionsUpdater::REMOVE_HARD);
    EXPECT_FALSE(extension->permissions_data()
                     ->active_permissions()
                     .HasExplicitAccessToOrigin(kOrigin));
    EXPECT_TRUE(extension->permissions_data()
                    ->withheld_permissions()
                    .HasExplicitAccessToOrigin(kOrigin));
    EXPECT_TRUE(updater.GetRevokablePermissions(extension.get())->IsEmpty());
  }
}

}  // namespace extensions
