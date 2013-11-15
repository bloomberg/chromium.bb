// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/permissions/permission_set.h"
#include "testing/gtest/include/gtest/gtest.h"

using extension_test_util::LoadManifest;

namespace extensions {

namespace {

// A helper class that listens for NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED.
class PermissionsUpdaterListener : public content::NotificationObserver {
 public:
  PermissionsUpdaterListener()
      : received_notification_(false), waiting_(false) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
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
// GrantActivePermissions and UpdateActivePermissions are used by
// AddPermissions.
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
      extension->GetActivePermissions();
  ASSERT_EQ(*default_permissions.get(),
            *extension->GetActivePermissions().get());

  // Add a few permissions.
  APIPermissionSet apis;
  apis.insert(APIPermission::kTab);
  apis.insert(APIPermission::kNotification);
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
  ASSERT_EQ(extension, listener.extension());
  ASSERT_EQ(UpdatedExtensionPermissionsInfo::ADDED, listener.reason());
  ASSERT_EQ(*delta.get(), *listener.permissions());

  // Make sure the extension's active permissions reflect the change.
  scoped_refptr<PermissionSet> active_permissions =
      PermissionSet::CreateUnion(default_permissions.get(), delta.get());
  ASSERT_EQ(*active_permissions.get(),
            *extension->GetActivePermissions().get());

  // Verify that the new granted and active permissions were also stored
  // in the extension preferences. In this case, the granted permissions should
  // be equal to the active permissions.
  ExtensionPrefs* prefs = service_->extension_prefs();
  scoped_refptr<PermissionSet> granted_permissions =
      active_permissions;

  scoped_refptr<PermissionSet> from_prefs =
      prefs->GetActivePermissions(extension->id());
  ASSERT_EQ(*active_permissions.get(), *from_prefs.get());

  from_prefs = prefs->GetGrantedPermissions(extension->id());
  ASSERT_EQ(*active_permissions.get(), *from_prefs.get());

  // In the second part of the test, we'll remove the permissions that we
  // just added except for 'notification'.
  apis.erase(APIPermission::kNotification);
  delta = new PermissionSet(apis, empty_manifest_permissions,
                            hosts, URLPatternSet());

  listener.Reset();
  updater.RemovePermissions(extension.get(), delta.get());
  listener.Wait();

  // Verify that the notification was correct.
  ASSERT_TRUE(listener.received_notification());
  ASSERT_EQ(extension, listener.extension());
  ASSERT_EQ(UpdatedExtensionPermissionsInfo::REMOVED, listener.reason());
  ASSERT_EQ(*delta.get(), *listener.permissions());

  // Make sure the extension's active permissions reflect the change.
  active_permissions =
      PermissionSet::CreateDifference(active_permissions.get(), delta.get());
  ASSERT_EQ(*active_permissions.get(),
            *extension->GetActivePermissions().get());

  // Verify that the extension prefs hold the new active permissions and the
  // same granted permissions.
  from_prefs = prefs->GetActivePermissions(extension->id());
  ASSERT_EQ(*active_permissions.get(), *from_prefs.get());

  from_prefs = prefs->GetGrantedPermissions(extension->id());
  ASSERT_EQ(*granted_permissions.get(), *from_prefs.get());
}

}  // namespace extensions
