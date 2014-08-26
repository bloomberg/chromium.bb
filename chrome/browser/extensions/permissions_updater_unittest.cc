// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/testing_profile.h"
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

scoped_refptr<const Extension> CreateExtensionWithPermissions(
    const std::set<URLPattern>& scriptable_hosts,
    const std::set<URLPattern>& explicit_hosts,
    Manifest::Location location) {
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
               .Set("name", "extension")
               .Set("description", "foo")
               .Set("manifest_version", 2)
               .Set("version", "0.1.2.3")
               .Set("content_scripts", ListBuilder().Append(script.Pass()))
               .Set("permissions", explicit_host_list.Pass()))
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
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    received_notification_ = true;
    UpdatedExtensionPermissionsInfo* info =
        content::Details<UpdatedExtensionPermissionsInfo>(details).ptr();

    extension_ = info->extension;
    permissions_ = info->permissions;
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
  scoped_refptr<const PermissionSet> permissions_;
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
  scoped_refptr<PermissionSet> default_permissions =
      new PermissionSet(default_apis, empty_manifest_permissions,
                        default_hosts, URLPatternSet());

  // Make sure it loaded properly.
  scoped_refptr<const PermissionSet> permissions =
      extension->permissions_data()->active_permissions();
  ASSERT_EQ(*default_permissions.get(),
            *extension->permissions_data()->active_permissions().get());

  // Add a few permissions.
  APIPermissionSet apis;
  apis.insert(APIPermission::kTab);
  apis.insert(APIPermission::kNotifications);
  URLPatternSet hosts;
  AddPattern(&hosts, "http://*.c.com/*");

  scoped_refptr<PermissionSet> delta =
      new PermissionSet(apis, empty_manifest_permissions,
                        hosts, URLPatternSet());

  PermissionsUpdaterListener listener;
  PermissionsUpdater updater(profile_.get());
  updater.AddPermissions(extension.get(), delta.get());

  listener.Wait();

  // Verify that the permission notification was sent correctly.
  ASSERT_TRUE(listener.received_notification());
  ASSERT_EQ(extension.get(), listener.extension());
  ASSERT_EQ(UpdatedExtensionPermissionsInfo::ADDED, listener.reason());
  ASSERT_EQ(*delta.get(), *listener.permissions());

  // Make sure the extension's active permissions reflect the change.
  scoped_refptr<PermissionSet> active_permissions =
      PermissionSet::CreateUnion(default_permissions.get(), delta.get());
  ASSERT_EQ(*active_permissions.get(),
            *extension->permissions_data()->active_permissions().get());

  // Verify that the new granted and active permissions were also stored
  // in the extension preferences. In this case, the granted permissions should
  // be equal to the active permissions.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_.get());
  scoped_refptr<PermissionSet> granted_permissions =
      active_permissions;

  scoped_refptr<PermissionSet> from_prefs =
      prefs->GetActivePermissions(extension->id());
  ASSERT_EQ(*active_permissions.get(), *from_prefs.get());

  from_prefs = prefs->GetGrantedPermissions(extension->id());
  ASSERT_EQ(*active_permissions.get(), *from_prefs.get());

  // In the second part of the test, we'll remove the permissions that we
  // just added except for 'notifications'.
  apis.erase(APIPermission::kNotifications);
  delta = new PermissionSet(apis, empty_manifest_permissions,
                            hosts, URLPatternSet());

  listener.Reset();
  updater.RemovePermissions(extension.get(), delta.get());
  listener.Wait();

  // Verify that the notification was correct.
  ASSERT_TRUE(listener.received_notification());
  ASSERT_EQ(extension.get(), listener.extension());
  ASSERT_EQ(UpdatedExtensionPermissionsInfo::REMOVED, listener.reason());
  ASSERT_EQ(*delta.get(), *listener.permissions());

  // Make sure the extension's active permissions reflect the change.
  active_permissions =
      PermissionSet::CreateDifference(active_permissions.get(), delta.get());
  ASSERT_EQ(*active_permissions.get(),
            *extension->permissions_data()->active_permissions().get());

  // Verify that the extension prefs hold the new active permissions and the
  // same granted permissions.
  from_prefs = prefs->GetActivePermissions(extension->id());
  ASSERT_EQ(*active_permissions.get(), *from_prefs.get());

  from_prefs = prefs->GetGrantedPermissions(extension->id());
  ASSERT_EQ(*granted_permissions.get(), *from_prefs.get());
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
      all_patterns, all_patterns, Manifest::INTERNAL);
  const PermissionsData* permissions_data = extension->permissions_data();
  PermissionsUpdater updater(profile_.get());
  updater.InitializePermissions(extension.get());

  // At first, the active permissions should have only the safe patterns and
  // the withheld permissions should have only the all host patterns.
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions()->scriptable_hosts().patterns(),
      safe_patterns));
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions()->explicit_hosts().patterns(),
      safe_patterns));
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->withheld_permissions()->scriptable_hosts().patterns(),
      all_host_patterns));
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->withheld_permissions()->explicit_hosts().patterns(),
      all_host_patterns));

  // Then, we grant the withheld all-hosts permissions.
  updater.GrantWithheldImpliedAllHosts(extension.get());
  // Now, active permissions should have all patterns, and withheld permissions
  // should have none.
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions()->scriptable_hosts().patterns(),
      all_patterns));
  EXPECT_TRUE(permissions_data->withheld_permissions()
                  ->scriptable_hosts()
                  .patterns()
                  .empty());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions()->explicit_hosts().patterns(),
      all_patterns));
  EXPECT_TRUE(permissions_data->withheld_permissions()
                  ->explicit_hosts()
                  .patterns()
                  .empty());

  // Finally, we revoke the all hosts permissions.
  updater.WithholdImpliedAllHosts(extension.get());

  // We should be back to our initial state - all_hosts should be withheld, and
  // the safe patterns should be granted.
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions()->scriptable_hosts().patterns(),
      safe_patterns));
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions()->explicit_hosts().patterns(),
      safe_patterns));
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->withheld_permissions()->scriptable_hosts().patterns(),
      all_host_patterns));
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->withheld_permissions()->explicit_hosts().patterns(),
      all_host_patterns));

  // Creating a component extension should result in no withheld permissions.
  extension = CreateExtensionWithPermissions(
      all_patterns, all_patterns, Manifest::COMPONENT);
  permissions_data = extension->permissions_data();
  updater.InitializePermissions(extension.get());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions()->scriptable_hosts().patterns(),
      all_patterns));
  EXPECT_TRUE(permissions_data->withheld_permissions()
                  ->scriptable_hosts()
                  .patterns()
                  .empty());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions()->explicit_hosts().patterns(),
      all_patterns));
  EXPECT_TRUE(permissions_data->withheld_permissions()
                  ->explicit_hosts()
                  .patterns()
                  .empty());

  // Without the switch, we shouldn't withhold anything.
  switch_override.reset();
  extension = CreateExtensionWithPermissions(
      all_patterns, all_patterns, Manifest::INTERNAL);
  permissions_data = extension->permissions_data();
  updater.InitializePermissions(extension.get());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions()->scriptable_hosts().patterns(),
      all_patterns));
  EXPECT_TRUE(permissions_data->withheld_permissions()
                  ->scriptable_hosts()
                  .patterns()
                  .empty());
  EXPECT_TRUE(SetsAreEqual(
      permissions_data->active_permissions()->explicit_hosts().patterns(),
      all_patterns));
  EXPECT_TRUE(permissions_data->withheld_permissions()
                  ->explicit_hosts()
                  .patterns()
                  .empty());
}

}  // namespace extensions
