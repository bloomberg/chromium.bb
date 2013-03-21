// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_prefs_unittest.h"

#include "base/basictypes.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/prefs/mock_pref_change_callback.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/mock_notification_observer.h"
#include "sync/api/string_ordinal.h"

using base::Time;
using base::TimeDelta;
using content::BrowserThread;

namespace {

const char kPref1[] = "path1.subpath";
const char kPref2[] = "path2";
const char kPref3[] = "path3";
const char kPref4[] = "path4";

// Default values in case an extension pref value is not overridden.
const char kDefaultPref1[] = "default pref 1";
const char kDefaultPref2[] = "default pref 2";
const char kDefaultPref3[] = "default pref 3";
const char kDefaultPref4[] = "default pref 4";

}  // namespace

namespace extensions {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

ExtensionPrefsTest::ExtensionPrefsTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      prefs_(message_loop_.message_loop_proxy()) {
}

ExtensionPrefsTest::~ExtensionPrefsTest() {
}

void ExtensionPrefsTest::RegisterPreferences(PrefRegistrySyncable* registry) {}

void ExtensionPrefsTest::SetUp() {
  RegisterPreferences(prefs_.pref_registry());
  Initialize();
}

void ExtensionPrefsTest::TearDown() {
  Verify();

  // Reset ExtensionPrefs, and re-verify.
  prefs_.ResetPrefRegistry();
  RegisterPreferences(prefs_.pref_registry());
  prefs_.RecreateExtensionPrefs();
  Verify();
  prefs_.pref_service()->CommitPendingWrite();
  message_loop_.RunUntilIdle();
}

// Tests the LastPingDay/SetLastPingDay functions.
class ExtensionPrefsLastPingDay : public ExtensionPrefsTest {
 public:
  ExtensionPrefsLastPingDay()
      : extension_time_(Time::Now() - TimeDelta::FromHours(4)),
        blacklist_time_(Time::Now() - TimeDelta::FromHours(2)) {}

  virtual void Initialize() OVERRIDE {
    extension_id_ = prefs_.AddExtensionAndReturnId("last_ping_day");
    EXPECT_TRUE(prefs()->LastPingDay(extension_id_).is_null());
    prefs()->SetLastPingDay(extension_id_, extension_time_);
    prefs()->SetBlacklistLastPingDay(blacklist_time_);
  }

  virtual void Verify() OVERRIDE {
    Time result = prefs()->LastPingDay(extension_id_);
    EXPECT_FALSE(result.is_null());
    EXPECT_TRUE(result == extension_time_);
    result = prefs()->BlacklistLastPingDay();
    EXPECT_FALSE(result.is_null());
    EXPECT_TRUE(result == blacklist_time_);
  }

 private:
  Time extension_time_;
  Time blacklist_time_;
  std::string extension_id_;
};
TEST_F(ExtensionPrefsLastPingDay, LastPingDay) {}

namespace {

void AddGalleryPermission(chrome::MediaGalleryPrefId gallery, bool has_access,
                          std::vector<chrome::MediaGalleryPermission>* vector) {
  chrome::MediaGalleryPermission permission;
  permission.pref_id = gallery;
  permission.has_permission = has_access;
  vector->push_back(permission);
}

}  // namspace

// Test the MediaGalleries permissions functions.
class MediaGalleriesPermissions : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    extension1_id_ = prefs_.AddExtensionAndReturnId("test1");
    extension2_id_ = prefs_.AddExtensionAndReturnId("test2");
    extension3_id_ = prefs_.AddExtensionAndReturnId("test3");
    // Id4 isn't used to ensure that an empty permission list is ok.
    extension4_id_ = prefs_.AddExtensionAndReturnId("test4");
    Verify();

    prefs()->SetMediaGalleryPermission(extension1_id_, 1, false);
    AddGalleryPermission(1, false, &extension1_expectation_);
    Verify();

    prefs()->SetMediaGalleryPermission(extension1_id_, 2, true);
    AddGalleryPermission(2, true, &extension1_expectation_);
    Verify();

    prefs()->SetMediaGalleryPermission(extension1_id_, 2, false);
    extension1_expectation_[1].has_permission = false;
    Verify();

    prefs()->SetMediaGalleryPermission(extension2_id_, 1, true);
    prefs()->SetMediaGalleryPermission(extension2_id_, 3, true);
    prefs()->SetMediaGalleryPermission(extension2_id_, 4, true);
    AddGalleryPermission(1, true, &extension2_expectation_);
    AddGalleryPermission(3, true, &extension2_expectation_);
    AddGalleryPermission(4, true, &extension2_expectation_);
    Verify();

    prefs()->SetMediaGalleryPermission(extension3_id_, 3, true);
    AddGalleryPermission(3, true, &extension3_expectation_);
    Verify();

    prefs()->RemoveMediaGalleryPermissions(3);
    extension2_expectation_.erase(extension2_expectation_.begin() + 1);
    extension3_expectation_.erase(extension3_expectation_.begin());
    Verify();

    prefs()->UnsetMediaGalleryPermission(extension1_id_, 1);
    extension1_expectation_.erase(extension1_expectation_.begin());
    Verify();
  }

  virtual void Verify() OVERRIDE {
    struct TestData {
      std::string* id;
      std::vector<chrome::MediaGalleryPermission>* expectation;
    };

    const TestData test_data[] = {{&extension1_id_, &extension1_expectation_},
                                  {&extension2_id_, &extension2_expectation_},
                                  {&extension3_id_, &extension3_expectation_},
                                  {&extension4_id_, &extension4_expectation_}};
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); i++) {
      std::vector<chrome::MediaGalleryPermission> actual =
          prefs()->GetMediaGalleryPermissions(*test_data[i].id);
      EXPECT_EQ(test_data[i].expectation->size(), actual.size());
      for (size_t permission_entry = 0;
           permission_entry < test_data[i].expectation->size() &&
               permission_entry < actual.size();
           permission_entry++) {
        EXPECT_EQ(test_data[i].expectation->at(permission_entry).pref_id,
                  actual[permission_entry].pref_id);
        EXPECT_EQ(test_data[i].expectation->at(permission_entry).has_permission,
                  actual[permission_entry].has_permission);
      }
    }
  }

 private:
  std::string extension1_id_;
  std::string extension2_id_;
  std::string extension3_id_;
  std::string extension4_id_;

  std::vector<chrome::MediaGalleryPermission> extension1_expectation_;
  std::vector<chrome::MediaGalleryPermission> extension2_expectation_;
  std::vector<chrome::MediaGalleryPermission> extension3_expectation_;
  std::vector<chrome::MediaGalleryPermission> extension4_expectation_;
};
TEST_F(MediaGalleriesPermissions, MediaGalleries) {}

// Tests the GetToolbarOrder/SetToolbarOrder functions.
class ExtensionPrefsToolbarOrder : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    list_.push_back(prefs_.AddExtensionAndReturnId("1"));
    list_.push_back(prefs_.AddExtensionAndReturnId("2"));
    list_.push_back(prefs_.AddExtensionAndReturnId("3"));
    std::vector<std::string> before_list = prefs()->GetToolbarOrder();
    EXPECT_TRUE(before_list.empty());
    prefs()->SetToolbarOrder(list_);
  }

  virtual void Verify() OVERRIDE {
    std::vector<std::string> result = prefs()->GetToolbarOrder();
    ASSERT_EQ(list_.size(), result.size());
    for (size_t i = 0; i < list_.size(); i++) {
      EXPECT_EQ(list_[i], result[i]);
    }
  }

 private:
  std::vector<std::string> list_;
};
TEST_F(ExtensionPrefsToolbarOrder, ToolbarOrder) {}


// Tests the IsExtensionDisabled/SetExtensionState functions.
class ExtensionPrefsExtensionState : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    extension = prefs_.AddExtension("test");
    prefs()->SetExtensionState(extension->id(), Extension::DISABLED);
  }

  virtual void Verify() OVERRIDE {
    EXPECT_TRUE(prefs()->IsExtensionDisabled(extension->id()));
  }

 private:
  scoped_refptr<Extension> extension;
};
TEST_F(ExtensionPrefsExtensionState, ExtensionState) {}

class ExtensionPrefsEscalatePermissions : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    extension = prefs_.AddExtension("test");
    prefs()->SetDidExtensionEscalatePermissions(extension.get(), true);
  }

  virtual void Verify() OVERRIDE {
    EXPECT_TRUE(prefs()->DidExtensionEscalatePermissions(extension->id()));
  }

 private:
  scoped_refptr<Extension> extension;
};
TEST_F(ExtensionPrefsEscalatePermissions, EscalatePermissions) {}

// Tests the AddGrantedPermissions / GetGrantedPermissions functions.
class ExtensionPrefsGrantedPermissions : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

    extension_id_ = prefs_.AddExtensionAndReturnId("test");

    api_perm_set1_.insert(APIPermission::kTab);
    api_perm_set1_.insert(APIPermission::kBookmark);
    scoped_ptr<APIPermission> permission(
        permission_info->CreateAPIPermission());
    {
      scoped_ptr<ListValue> value(new ListValue());
      value->Append(Value::CreateStringValue("tcp-connect:*.example.com:80"));
      value->Append(Value::CreateStringValue("udp-bind::8080"));
      value->Append(Value::CreateStringValue("udp-send-to::8888"));
      if (!permission->FromValue(value.get()))
        NOTREACHED();
    }
    api_perm_set1_.insert(permission.release());

    api_perm_set2_.insert(APIPermission::kHistory);

    AddPattern(&ehost_perm_set1_, "http://*.google.com/*");
    AddPattern(&ehost_perm_set1_, "http://example.com/*");
    AddPattern(&ehost_perm_set1_, "chrome://favicon/*");

    AddPattern(&ehost_perm_set2_, "https://*.google.com/*");
    // with duplicate:
    AddPattern(&ehost_perm_set2_, "http://*.google.com/*");

    AddPattern(&shost_perm_set1_, "http://reddit.com/r/test/*");
    AddPattern(&shost_perm_set2_, "http://reddit.com/r/test/*");
    AddPattern(&shost_perm_set2_, "http://somesite.com/*");
    AddPattern(&shost_perm_set2_, "http://example.com/*");

    APIPermissionSet expected_apis = api_perm_set1_;

    AddPattern(&ehost_permissions_, "http://*.google.com/*");
    AddPattern(&ehost_permissions_, "http://example.com/*");
    AddPattern(&ehost_permissions_, "chrome://favicon/*");
    AddPattern(&ehost_permissions_, "https://*.google.com/*");

    AddPattern(&shost_permissions_, "http://reddit.com/r/test/*");
    AddPattern(&shost_permissions_, "http://somesite.com/*");
    AddPattern(&shost_permissions_, "http://example.com/*");

    APIPermissionSet empty_set;
    URLPatternSet empty_extent;
    scoped_refptr<PermissionSet> permissions;
    scoped_refptr<PermissionSet> granted_permissions;

    // Make sure both granted api and host permissions start empty.
    granted_permissions =
        prefs()->GetGrantedPermissions(extension_id_);
    EXPECT_TRUE(granted_permissions->IsEmpty());

    permissions = new PermissionSet(
        api_perm_set1_, empty_extent, empty_extent);

    // Add part of the api permissions.
    prefs()->AddGrantedPermissions(extension_id_, permissions.get());
    granted_permissions = prefs()->GetGrantedPermissions(extension_id_);
    EXPECT_TRUE(granted_permissions.get());
    EXPECT_FALSE(granted_permissions->IsEmpty());
    EXPECT_EQ(expected_apis, granted_permissions->apis());
    EXPECT_TRUE(granted_permissions->effective_hosts().is_empty());
    EXPECT_FALSE(granted_permissions->HasEffectiveFullAccess());
    granted_permissions = NULL;

    // Add part of the explicit host permissions.
    permissions = new PermissionSet(
        empty_set, ehost_perm_set1_, empty_extent);
    prefs()->AddGrantedPermissions(extension_id_, permissions.get());
    granted_permissions = prefs()->GetGrantedPermissions(extension_id_);
    EXPECT_FALSE(granted_permissions->IsEmpty());
    EXPECT_FALSE(granted_permissions->HasEffectiveFullAccess());
    EXPECT_EQ(expected_apis, granted_permissions->apis());
    EXPECT_EQ(ehost_perm_set1_,
              granted_permissions->explicit_hosts());
    EXPECT_EQ(ehost_perm_set1_,
              granted_permissions->effective_hosts());

    // Add part of the scriptable host permissions.
    permissions = new PermissionSet(
        empty_set, empty_extent, shost_perm_set1_);
    prefs()->AddGrantedPermissions(extension_id_, permissions.get());
    granted_permissions = prefs()->GetGrantedPermissions(extension_id_);
    EXPECT_FALSE(granted_permissions->IsEmpty());
    EXPECT_FALSE(granted_permissions->HasEffectiveFullAccess());
    EXPECT_EQ(expected_apis, granted_permissions->apis());
    EXPECT_EQ(ehost_perm_set1_,
              granted_permissions->explicit_hosts());
    EXPECT_EQ(shost_perm_set1_,
              granted_permissions->scriptable_hosts());

    URLPatternSet::CreateUnion(ehost_perm_set1_, shost_perm_set1_,
                               &effective_permissions_);
    EXPECT_EQ(effective_permissions_, granted_permissions->effective_hosts());

    // Add the rest of the permissions.
    permissions = new PermissionSet(
        api_perm_set2_, ehost_perm_set2_, shost_perm_set2_);

    APIPermissionSet::Union(expected_apis, api_perm_set2_, &api_permissions_);

    prefs()->AddGrantedPermissions(extension_id_, permissions.get());
    granted_permissions = prefs()->GetGrantedPermissions(extension_id_);
    EXPECT_TRUE(granted_permissions.get());
    EXPECT_FALSE(granted_permissions->IsEmpty());
    EXPECT_EQ(api_permissions_, granted_permissions->apis());
    EXPECT_EQ(ehost_permissions_,
              granted_permissions->explicit_hosts());
    EXPECT_EQ(shost_permissions_,
              granted_permissions->scriptable_hosts());
    effective_permissions_.ClearPatterns();
    URLPatternSet::CreateUnion(ehost_permissions_, shost_permissions_,
                               &effective_permissions_);
    EXPECT_EQ(effective_permissions_, granted_permissions->effective_hosts());
  }

  virtual void Verify() OVERRIDE {
    scoped_refptr<PermissionSet> permissions(
        prefs()->GetGrantedPermissions(extension_id_));
    EXPECT_TRUE(permissions.get());
    EXPECT_FALSE(permissions->HasEffectiveFullAccess());
    EXPECT_EQ(api_permissions_, permissions->apis());
    EXPECT_EQ(ehost_permissions_,
              permissions->explicit_hosts());
    EXPECT_EQ(shost_permissions_,
              permissions->scriptable_hosts());
  }

 private:
  std::string extension_id_;
  APIPermissionSet api_perm_set1_;
  APIPermissionSet api_perm_set2_;
  URLPatternSet ehost_perm_set1_;
  URLPatternSet ehost_perm_set2_;
  URLPatternSet shost_perm_set1_;
  URLPatternSet shost_perm_set2_;

  APIPermissionSet api_permissions_;
  URLPatternSet ehost_permissions_;
  URLPatternSet shost_permissions_;
  URLPatternSet effective_permissions_;
};
TEST_F(ExtensionPrefsGrantedPermissions, GrantedPermissions) {}

// Tests the SetActivePermissions / GetActivePermissions functions.
class ExtensionPrefsActivePermissions : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    extension_id_ = prefs_.AddExtensionAndReturnId("test");

    APIPermissionSet api_perms;
    api_perms.insert(APIPermission::kTab);
    api_perms.insert(APIPermission::kBookmark);
    api_perms.insert(APIPermission::kHistory);

    URLPatternSet ehosts;
    AddPattern(&ehosts, "http://*.google.com/*");
    AddPattern(&ehosts, "http://example.com/*");
    AddPattern(&ehosts, "chrome://favicon/*");

    URLPatternSet shosts;
    AddPattern(&shosts, "https://*.google.com/*");
    AddPattern(&shosts, "http://reddit.com/r/test/*");

    active_perms_ = new PermissionSet(api_perms, ehosts, shosts);

    // Make sure the active permissions start empty.
    scoped_refptr<PermissionSet> active(
        prefs()->GetActivePermissions(extension_id_));
    EXPECT_TRUE(active->IsEmpty());

    // Set the active permissions.
    prefs()->SetActivePermissions(extension_id_, active_perms_.get());
    active = prefs()->GetActivePermissions(extension_id_);
    EXPECT_EQ(active_perms_->apis(), active->apis());
    EXPECT_EQ(active_perms_->explicit_hosts(), active->explicit_hosts());
    EXPECT_EQ(active_perms_->scriptable_hosts(), active->scriptable_hosts());
    EXPECT_EQ(*active_perms_, *active);
  }

  virtual void Verify() OVERRIDE {
    scoped_refptr<PermissionSet> permissions(
        prefs()->GetActivePermissions(extension_id_));
    EXPECT_EQ(*active_perms_, *permissions);
  }

 private:
  std::string extension_id_;
  scoped_refptr<PermissionSet> active_perms_;
};
TEST_F(ExtensionPrefsActivePermissions, SetAndGetActivePermissions) {}

// Tests the GetVersionString function.
class ExtensionPrefsVersionString : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    extension = prefs_.AddExtension("test");
    EXPECT_EQ("0.1", prefs()->GetVersionString(extension->id()));
    prefs()->OnExtensionUninstalled(extension->id(),
                                    Manifest::INTERNAL, false);
  }

  virtual void Verify() OVERRIDE {
    EXPECT_EQ("", prefs()->GetVersionString(extension->id()));
  }

 private:
  scoped_refptr<Extension> extension;
};
TEST_F(ExtensionPrefsVersionString, VersionString) {}

class ExtensionPrefsAcknowledgment : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    not_installed_id_ = "pghjnghklobnfoidcldiidjjjhkeeaoi";

    // Install some extensions.
    for (int i = 0; i < 5; i++) {
      std::string name = "test" + base::IntToString(i);
      extensions_.push_back(prefs_.AddExtension(name));
    }
    EXPECT_EQ(NULL,
              prefs()->GetInstalledExtensionInfo(not_installed_id_).get());

    ExtensionList::const_iterator iter;
    for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
      std::string id = (*iter)->id();
      EXPECT_FALSE(prefs()->IsExternalExtensionAcknowledged(id));
      EXPECT_FALSE(prefs()->IsBlacklistedExtensionAcknowledged(id));
      if (external_id_.empty()) {
        external_id_ = id;
        continue;
      }
      if (blacklisted_id_.empty()) {
        blacklisted_id_ = id;
        continue;
      }
    }
    // For each type of acknowledgment, acknowledge one installed and one
    // not-installed extension id.
    prefs()->AcknowledgeExternalExtension(external_id_);
    prefs()->AcknowledgeBlacklistedExtension(blacklisted_id_);
    prefs()->AcknowledgeExternalExtension(not_installed_id_);
    prefs()->AcknowledgeBlacklistedExtension(not_installed_id_);
  }

  virtual void Verify() OVERRIDE {
    ExtensionList::const_iterator iter;
    for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
      std::string id = (*iter)->id();
      if (id == external_id_) {
        EXPECT_TRUE(prefs()->IsExternalExtensionAcknowledged(id));
      } else {
        EXPECT_FALSE(prefs()->IsExternalExtensionAcknowledged(id));
      }
      if (id == blacklisted_id_) {
        EXPECT_TRUE(prefs()->IsBlacklistedExtensionAcknowledged(id));
      } else {
        EXPECT_FALSE(prefs()->IsBlacklistedExtensionAcknowledged(id));
      }
    }
    EXPECT_TRUE(prefs()->IsExternalExtensionAcknowledged(not_installed_id_));
    EXPECT_TRUE(prefs()->IsBlacklistedExtensionAcknowledged(not_installed_id_));
  }

 private:
  ExtensionList extensions_;

  std::string not_installed_id_;
  std::string external_id_;
  std::string blacklisted_id_;
};
TEST_F(ExtensionPrefsAcknowledgment, Acknowledgment) {}

// Tests force hiding browser actions.
class ExtensionPrefsHidingBrowserActions : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    // Install 5 extensions.
    for (int i = 0; i < 5; i++) {
      std::string name = "test" + base::IntToString(i);
      extensions_.push_back(prefs_.AddExtension(name));
    }

    ExtensionList::const_iterator iter;
    for (iter = extensions_.begin(); iter != extensions_.end(); ++iter)
      EXPECT_TRUE(prefs()->GetBrowserActionVisibility(*iter));

    prefs()->SetBrowserActionVisibility(extensions_[0], false);
    prefs()->SetBrowserActionVisibility(extensions_[1], true);
  }

  virtual void Verify() OVERRIDE {
    // Make sure the one we hid is hidden.
    EXPECT_FALSE(prefs()->GetBrowserActionVisibility(extensions_[0]));

    // Make sure the other id's are not hidden.
    ExtensionList::const_iterator iter = extensions_.begin() + 1;
    for (; iter != extensions_.end(); ++iter) {
      SCOPED_TRACE(base::StringPrintf("Loop %d ",
                       static_cast<int>(iter - extensions_.begin())));
      EXPECT_TRUE(prefs()->GetBrowserActionVisibility(*iter));
    }
  }

 private:
  ExtensionList extensions_;
};
TEST_F(ExtensionPrefsHidingBrowserActions, ForceHide) {}

// Tests the idle install information functions.
class ExtensionPrefsDelayedInstallInfo : public ExtensionPrefsTest {
 public:
  // Sets idle install information for one test extension.
  void SetIdleInfo(std::string id, int num) {
    DictionaryValue manifest;
    manifest.SetString(extension_manifest_keys::kName, "test");
    manifest.SetString(extension_manifest_keys::kVersion,
                       "1." + base::IntToString(num));
    base::FilePath path =
        prefs_.extensions_dir().AppendASCII(base::IntToString(num));
    std::string errors;
    scoped_refptr<Extension> extension = Extension::Create(
        path, Manifest::INTERNAL, manifest, Extension::NO_FLAGS, id, &errors);
    ASSERT_TRUE(extension) << errors;
    ASSERT_EQ(id, extension->id());
    prefs()->SetDelayedInstallInfo(extension.get(), Extension::ENABLED,
                                   syncer::StringOrdinal());
  }

  // Verifies that we get back expected idle install information previously
  // set by SetIdleInfo.
  void VerifyIdleInfo(std::string id, int num) {
    scoped_ptr<ExtensionInfo> info(prefs()->GetDelayedInstallInfo(id));
    ASSERT_TRUE(info);
    std::string version;
    ASSERT_TRUE(info->extension_manifest->GetString("version", &version));
    ASSERT_EQ("1." + base::IntToString(num), version);
    ASSERT_EQ(base::IntToString(num),
              info->extension_path.BaseName().MaybeAsASCII());
  }

  bool HasInfoForId(extensions::ExtensionPrefs::ExtensionsInfo* info,
                    const std::string& id) {
    for (size_t i = 0; i < info->size(); ++i) {
      if (info->at(i)->extension_id == id)
        return true;
    }
    return false;
  }

  virtual void Initialize() OVERRIDE {
    PathService::Get(chrome::DIR_TEST_DATA, &basedir_);
    now_ = Time::Now();
    id1_ = prefs_.AddExtensionAndReturnId("1");
    id2_ = prefs_.AddExtensionAndReturnId("2");
    id3_ = prefs_.AddExtensionAndReturnId("3");
    id4_ = prefs_.AddExtensionAndReturnId("4");

    // Set info for two extensions, then remove it.
    SetIdleInfo(id1_, 1);
    SetIdleInfo(id2_, 2);
    VerifyIdleInfo(id1_, 1);
    VerifyIdleInfo(id2_, 2);
    scoped_ptr<extensions::ExtensionPrefs::ExtensionsInfo> info(
        prefs()->GetAllDelayedInstallInfo());
    EXPECT_EQ(2u, info->size());
    EXPECT_TRUE(HasInfoForId(info.get(), id1_));
    EXPECT_TRUE(HasInfoForId(info.get(), id2_));
    prefs()->RemoveDelayedInstallInfo(id1_);
    prefs()->RemoveDelayedInstallInfo(id2_);
    info = prefs()->GetAllDelayedInstallInfo();
    EXPECT_TRUE(info->empty());

    // Try getting/removing info for an id that used to have info set.
    EXPECT_FALSE(prefs()->GetDelayedInstallInfo(id1_));
    EXPECT_FALSE(prefs()->RemoveDelayedInstallInfo(id1_));

    // Try getting/removing info for an id that has not yet had any info set.
    EXPECT_FALSE(prefs()->GetDelayedInstallInfo(id3_));
    EXPECT_FALSE(prefs()->RemoveDelayedInstallInfo(id3_));

    // Set info for 4 extensions, then remove for one of them.
    SetIdleInfo(id1_, 1);
    SetIdleInfo(id2_, 2);
    SetIdleInfo(id3_, 3);
    SetIdleInfo(id4_, 4);
    VerifyIdleInfo(id1_, 1);
    VerifyIdleInfo(id2_, 2);
    VerifyIdleInfo(id3_, 3);
    VerifyIdleInfo(id4_, 4);
    prefs()->RemoveDelayedInstallInfo(id3_);
  }

  virtual void Verify() OVERRIDE {
    // Make sure the info for the 3 extensions we expect is present.
    scoped_ptr<extensions::ExtensionPrefs::ExtensionsInfo> info(
        prefs()->GetAllDelayedInstallInfo());
    EXPECT_EQ(3u, info->size());
    EXPECT_TRUE(HasInfoForId(info.get(), id1_));
    EXPECT_TRUE(HasInfoForId(info.get(), id2_));
    EXPECT_TRUE(HasInfoForId(info.get(), id4_));
    VerifyIdleInfo(id1_, 1);
    VerifyIdleInfo(id2_, 2);
    VerifyIdleInfo(id4_, 4);

    // Make sure there isn't info the for the one extension id we removed.
    EXPECT_FALSE(prefs()->GetDelayedInstallInfo(id3_));
  }

 protected:
  Time now_;
  base::FilePath basedir_;
  std::string id1_;
  std::string id2_;
  std::string id3_;
  std::string id4_;
};
TEST_F(ExtensionPrefsDelayedInstallInfo, DelayedInstallInfo) {}

class ExtensionPrefsOnExtensionInstalled : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    extension_ = prefs_.AddExtension("on_extension_installed");
    EXPECT_FALSE(prefs()->IsExtensionDisabled(extension_->id()));
    prefs()->OnExtensionInstalled(
        extension_.get(), Extension::DISABLED,
        syncer::StringOrdinal());
  }

  virtual void Verify() OVERRIDE {
    EXPECT_TRUE(prefs()->IsExtensionDisabled(extension_->id()));
  }

 private:
  scoped_refptr<Extension> extension_;
};
TEST_F(ExtensionPrefsOnExtensionInstalled,
       ExtensionPrefsOnExtensionInstalled) {}

class ExtensionPrefsAppDraggedByUser : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    extension_ = prefs_.AddExtension("on_extension_installed");
    EXPECT_FALSE(prefs()->WasAppDraggedByUser(extension_->id()));
    prefs()->OnExtensionInstalled(extension_.get(), Extension::ENABLED,
                                  syncer::StringOrdinal());
  }

  virtual void Verify() OVERRIDE {
    // Set the flag and see if it persisted.
    prefs()->SetAppDraggedByUser(extension_->id());
    EXPECT_TRUE(prefs()->WasAppDraggedByUser(extension_->id()));

    // Make sure it doesn't change on consecutive calls.
    prefs()->SetAppDraggedByUser(extension_->id());
    EXPECT_TRUE(prefs()->WasAppDraggedByUser(extension_->id()));
  }

 private:
  scoped_refptr<Extension> extension_;
};
TEST_F(ExtensionPrefsAppDraggedByUser, ExtensionPrefsAppDraggedByUser) {}

class ExtensionPrefsFlags : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    {
      base::DictionaryValue dictionary;
      dictionary.SetString(extension_manifest_keys::kName, "from_webstore");
      dictionary.SetString(extension_manifest_keys::kVersion, "0.1");
      webstore_extension_ = prefs_.AddExtensionWithManifestAndFlags(
          dictionary, Manifest::INTERNAL, Extension::FROM_WEBSTORE);
    }

    {
      base::DictionaryValue dictionary;
      dictionary.SetString(extension_manifest_keys::kName, "from_bookmark");
      dictionary.SetString(extension_manifest_keys::kVersion, "0.1");
      bookmark_extension_ = prefs_.AddExtensionWithManifestAndFlags(
          dictionary, Manifest::INTERNAL, Extension::FROM_BOOKMARK);
    }

    {
      base::DictionaryValue dictionary;
      dictionary.SetString(extension_manifest_keys::kName,
                           "was_installed_by_default");
      dictionary.SetString(extension_manifest_keys::kVersion, "0.1");
      default_extension_ = prefs_.AddExtensionWithManifestAndFlags(
          dictionary,
          Manifest::INTERNAL,
          Extension::WAS_INSTALLED_BY_DEFAULT);
    }
  }

  virtual void Verify() OVERRIDE {
    EXPECT_TRUE(prefs()->IsFromWebStore(webstore_extension_->id()));
    EXPECT_FALSE(prefs()->IsFromBookmark(webstore_extension_->id()));

    EXPECT_TRUE(prefs()->IsFromBookmark(bookmark_extension_->id()));
    EXPECT_FALSE(prefs()->IsFromWebStore(bookmark_extension_->id()));

    EXPECT_TRUE(prefs()->WasInstalledByDefault(default_extension_->id()));
  }

 private:
  scoped_refptr<Extension> webstore_extension_;
  scoped_refptr<Extension> bookmark_extension_;
  scoped_refptr<Extension> default_extension_;
};
TEST_F(ExtensionPrefsFlags, ExtensionPrefsFlags) {}

namespace keys = extension_manifest_keys;

ExtensionPrefsPrepopulatedTest::ExtensionPrefsPrepopulatedTest()
    : ExtensionPrefsTest(),
      ext1_(NULL),
      ext2_(NULL),
      ext3_(NULL),
      ext4_(NULL),
      installed() {
  DictionaryValue simple_dict;
  std::string error;

  simple_dict.SetString(keys::kVersion, "1.0.0.0");
  simple_dict.SetString(keys::kName, "unused");

  ext1_scoped_ = Extension::Create(
      prefs_.temp_dir().AppendASCII("ext1_"), Manifest::EXTERNAL_PREF,
      simple_dict, Extension::NO_FLAGS, &error);
  ext2_scoped_ = Extension::Create(
      prefs_.temp_dir().AppendASCII("ext2_"), Manifest::EXTERNAL_PREF,
      simple_dict, Extension::NO_FLAGS, &error);
  ext3_scoped_ = Extension::Create(
      prefs_.temp_dir().AppendASCII("ext3_"), Manifest::EXTERNAL_PREF,
      simple_dict, Extension::NO_FLAGS, &error);
  ext4_scoped_ = Extension::Create(
      prefs_.temp_dir().AppendASCII("ext4_"), Manifest::EXTERNAL_PREF,
      simple_dict, Extension::NO_FLAGS, &error);

  ext1_ = ext1_scoped_.get();
  ext2_ = ext2_scoped_.get();
  ext3_ = ext3_scoped_.get();
  ext4_ = ext4_scoped_.get();

  for (size_t i = 0; i < arraysize(installed); ++i)
    installed[i] = false;
}

ExtensionPrefsPrepopulatedTest::~ExtensionPrefsPrepopulatedTest() {}

void ExtensionPrefsPrepopulatedTest::RegisterPreferences(
    PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(
      kPref1, kDefaultPref1, PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      kPref2, kDefaultPref2, PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      kPref3, kDefaultPref3, PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      kPref4, kDefaultPref4, PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void ExtensionPrefsPrepopulatedTest::InstallExtControlledPref(
    Extension *ext,
    const std::string& key,
    Value* val) {
  EnsureExtensionInstalled(ext);
  prefs()->SetExtensionControlledPref(
      ext->id(), key, kExtensionPrefsScopeRegular, val);
}

void ExtensionPrefsPrepopulatedTest::InstallExtControlledPrefIncognito(
    Extension *ext,
    const std::string& key,
    Value* val) {
  EnsureExtensionInstalled(ext);
  prefs()->SetExtensionControlledPref(
      ext->id(), key, kExtensionPrefsScopeIncognitoPersistent, val);
}

void ExtensionPrefsPrepopulatedTest
::InstallExtControlledPrefIncognitoSessionOnly(Extension *ext,
                                               const std::string& key,
                                               Value* val) {
  EnsureExtensionInstalled(ext);
  prefs()->SetExtensionControlledPref(
      ext->id(), key, kExtensionPrefsScopeIncognitoSessionOnly, val);
}

void ExtensionPrefsPrepopulatedTest::InstallExtension(Extension *ext) {
  EnsureExtensionInstalled(ext);
}

void ExtensionPrefsPrepopulatedTest::UninstallExtension(
    const std::string& extension_id) {
  EnsureExtensionUninstalled(extension_id);
}

void ExtensionPrefsPrepopulatedTest::EnsureExtensionInstalled(Extension *ext) {
  // Install extension the first time a preference is set for it.
  Extension* extensions[] = {ext1_, ext2_, ext3_, ext4_};
  for (size_t i = 0; i < arraysize(extensions); ++i) {
    if (ext == extensions[i] && !installed[i]) {
      prefs()->OnExtensionInstalled(ext, Extension::ENABLED,
                                    syncer::StringOrdinal());
      installed[i] = true;
      break;
    }
  }
}

void ExtensionPrefsPrepopulatedTest::EnsureExtensionUninstalled(
    const std::string& extension_id) {
  Extension* extensions[] = {ext1_, ext2_, ext3_, ext4_};
  for (size_t i = 0; i < arraysize(extensions); ++i) {
    if (extensions[i]->id() == extension_id) {
      installed[i] = false;
      break;
    }
  }
  prefs()->OnExtensionUninstalled(extension_id, Manifest::INTERNAL, false);
}

class ExtensionPrefsInstallOneExtension
    : public ExtensionPrefsPrepopulatedTest {
  virtual void Initialize() OVERRIDE {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
  }
  virtual void Verify() OVERRIDE {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
  }
};
TEST_F(ExtensionPrefsInstallOneExtension, ExtensionPrefsInstallOneExtension) {}

// Check that we do not forget persistent incognito values after a reload.
class ExtensionPrefsInstallIncognitoPersistent
    : public ExtensionPrefsPrepopulatedTest {
 public:
  virtual void Initialize() OVERRIDE {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    InstallExtControlledPrefIncognito(ext1_, kPref1,
                                      Value::CreateStringValue("val2"));
    scoped_ptr<PrefService> incog_prefs(prefs_.CreateIncognitoPrefService());
    std::string actual = incog_prefs->GetString(kPref1);
    EXPECT_EQ("val2", actual);
  }
  virtual void Verify() OVERRIDE {
    // Main pref service shall see only non-incognito settings.
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
    // Incognito pref service shall see incognito values.
    scoped_ptr<PrefService> incog_prefs(prefs_.CreateIncognitoPrefService());
    actual = incog_prefs->GetString(kPref1);
    EXPECT_EQ("val2", actual);
  }
};
TEST_F(ExtensionPrefsInstallIncognitoPersistent,
       ExtensionPrefsInstallOneExtension) {}

// Check that we forget 'session only' incognito values after a reload.
class ExtensionPrefsInstallIncognitoSessionOnly
    : public ExtensionPrefsPrepopulatedTest {
 public:
  ExtensionPrefsInstallIncognitoSessionOnly() : iteration_(0) {}

  virtual void Initialize() OVERRIDE {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    InstallExtControlledPrefIncognitoSessionOnly(
        ext1_, kPref1, Value::CreateStringValue("val2"));
    scoped_ptr<PrefService> incog_prefs(prefs_.CreateIncognitoPrefService());
    std::string actual = incog_prefs->GetString(kPref1);
    EXPECT_EQ("val2", actual);
  }
  virtual void Verify() OVERRIDE {
    // Main pref service shall see only non-incognito settings.
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
    // Incognito pref service shall see session-only incognito values only
    // during first run. Once the pref service was reloaded, all values shall be
    // discarded.
    scoped_ptr<PrefService> incog_prefs(prefs_.CreateIncognitoPrefService());
    actual = incog_prefs->GetString(kPref1);
    if (iteration_ == 0) {
      EXPECT_EQ("val2", actual);
    } else {
      EXPECT_EQ("val1", actual);
    }
    ++iteration_;
  }
  int iteration_;
};
TEST_F(ExtensionPrefsInstallIncognitoSessionOnly,
       ExtensionPrefsInstallOneExtension) {}

class ExtensionPrefsUninstallExtension : public ExtensionPrefsPrepopulatedTest {
  virtual void Initialize() OVERRIDE {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    InstallExtControlledPref(ext1_, kPref2, Value::CreateStringValue("val2"));
    ContentSettingsStore* store = prefs()->content_settings_store();
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString("http://[*.]example.com");
    store->SetExtensionContentSetting(ext1_->id(),
                                      pattern, pattern,
                                      CONTENT_SETTINGS_TYPE_IMAGES,
                                      std::string(),
                                      CONTENT_SETTING_BLOCK,
                                      kExtensionPrefsScopeRegular);

    UninstallExtension(ext1_->id());
  }
  virtual void Verify() OVERRIDE {
    EXPECT_EQ(NULL, prefs()->GetExtensionPref(ext1_->id()));

    std::string actual;
    actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ(kDefaultPref1, actual);
    actual = prefs()->pref_service()->GetString(kPref2);
    EXPECT_EQ(kDefaultPref2, actual);
  }
};
TEST_F(ExtensionPrefsUninstallExtension,
       ExtensionPrefsUninstallExtension) {}

// Tests triggering of notifications to registered observers.
class ExtensionPrefsNotifyWhenNeeded : public ExtensionPrefsPrepopulatedTest {
  virtual void Initialize() OVERRIDE {
    using testing::_;
    using testing::Mock;
    using testing::StrEq;

    MockPrefChangeCallback observer(prefs()->pref_service());
    PrefChangeRegistrar registrar;
    registrar.Init(prefs()->pref_service());
    registrar.Add(kPref1, observer.GetCallback());

    MockPrefChangeCallback incognito_observer(prefs()->pref_service());
    scoped_ptr<PrefService> incog_prefs(prefs_.CreateIncognitoPrefService());
    PrefChangeRegistrar incognito_registrar;
    incognito_registrar.Init(incog_prefs.get());
    incognito_registrar.Add(kPref1, incognito_observer.GetCallback());

    // Write value and check notification.
    EXPECT_CALL(observer, OnPreferenceChanged(_));
    EXPECT_CALL(incognito_observer, OnPreferenceChanged(_));
    InstallExtControlledPref(ext1_, kPref1,
        Value::CreateStringValue("https://www.chromium.org"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Write same value.
    EXPECT_CALL(observer, OnPreferenceChanged(_)).Times(0);
    EXPECT_CALL(incognito_observer, OnPreferenceChanged(_)).Times(0);
    InstallExtControlledPref(ext1_, kPref1,
        Value::CreateStringValue("https://www.chromium.org"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Change value.
    EXPECT_CALL(observer, OnPreferenceChanged(_));
    EXPECT_CALL(incognito_observer, OnPreferenceChanged(_));
    InstallExtControlledPref(ext1_, kPref1,
        Value::CreateStringValue("chrome://newtab"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Change only incognito persistent value.
    EXPECT_CALL(observer, OnPreferenceChanged(_)).Times(0);
    EXPECT_CALL(incognito_observer, OnPreferenceChanged(_));
    InstallExtControlledPrefIncognito(ext1_, kPref1,
        Value::CreateStringValue("chrome://newtab2"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Change only incognito session-only value.
    EXPECT_CALL(observer, OnPreferenceChanged(_)).Times(0);
    EXPECT_CALL(incognito_observer, OnPreferenceChanged(_));
    InstallExtControlledPrefIncognito(ext1_, kPref1,
        Value::CreateStringValue("chrome://newtab3"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Uninstall.
    EXPECT_CALL(observer, OnPreferenceChanged(_));
    EXPECT_CALL(incognito_observer, OnPreferenceChanged(_));
    UninstallExtension(ext1_->id());
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    registrar.Remove(kPref1);
    incognito_registrar.Remove(kPref1);
  }
  virtual void Verify() OVERRIDE {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ(kDefaultPref1, actual);
  }
};
TEST_F(ExtensionPrefsNotifyWhenNeeded,
    ExtensionPrefsNotifyWhenNeeded) {}

// Tests disabling an extension.
class ExtensionPrefsDisableExt : public ExtensionPrefsPrepopulatedTest {
  virtual void Initialize() OVERRIDE {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
    prefs()->SetExtensionState(ext1_->id(), Extension::DISABLED);
  }
  virtual void Verify() OVERRIDE {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ(kDefaultPref1, actual);
  }
};
TEST_F(ExtensionPrefsDisableExt,  ExtensionPrefsDisableExt) {}

// Tests disabling and reenabling an extension.
class ExtensionPrefsReenableExt : public ExtensionPrefsPrepopulatedTest {
  virtual void Initialize() OVERRIDE {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    prefs()->SetExtensionState(ext1_->id(), Extension::DISABLED);
    prefs()->SetExtensionState(ext1_->id(), Extension::ENABLED);
  }
  virtual void Verify() OVERRIDE {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
  }
};
TEST_F(ExtensionPrefsDisableExt,  ExtensionPrefsReenableExt) {}

// Mock class to test whether objects are deleted correctly.
class MockStringValue : public StringValue {
 public:
  explicit MockStringValue(const std::string& in_value)
      : StringValue(in_value) {
  }
  virtual ~MockStringValue() {
    Die();
  }
  MOCK_METHOD0(Die, void());
};

class ExtensionPrefsSetExtensionControlledPref
    : public ExtensionPrefsPrepopulatedTest {
 public:
  virtual void Initialize() OVERRIDE {
    MockStringValue* v1 = new MockStringValue("https://www.chromium.org");
    MockStringValue* v2 = new MockStringValue("https://www.chromium.org");
    MockStringValue* v1i = new MockStringValue("https://www.chromium.org");
    MockStringValue* v2i = new MockStringValue("https://www.chromium.org");
    // Ownership is taken, value shall not be deleted.
    EXPECT_CALL(*v1, Die()).Times(0);
    EXPECT_CALL(*v1i, Die()).Times(0);
    InstallExtControlledPref(ext1_, kPref1, v1);
    InstallExtControlledPrefIncognito(ext1_, kPref1, v1i);
    testing::Mock::VerifyAndClearExpectations(v1);
    testing::Mock::VerifyAndClearExpectations(v1i);
    // Make sure there is no memory leak and both values are deleted.
    EXPECT_CALL(*v1, Die()).Times(1);
    EXPECT_CALL(*v1i, Die()).Times(1);
    EXPECT_CALL(*v2, Die()).Times(1);
    EXPECT_CALL(*v2i, Die()).Times(1);
    InstallExtControlledPref(ext1_, kPref1, v2);
    InstallExtControlledPrefIncognito(ext1_, kPref1, v2i);
    prefs_.RecreateExtensionPrefs();
    testing::Mock::VerifyAndClearExpectations(v1);
    testing::Mock::VerifyAndClearExpectations(v1i);
    testing::Mock::VerifyAndClearExpectations(v2);
    testing::Mock::VerifyAndClearExpectations(v2i);
  }

  virtual void Verify() OVERRIDE {
  }
};
TEST_F(ExtensionPrefsSetExtensionControlledPref,
    ExtensionPrefsSetExtensionControlledPref) {}

// Tests that the switches::kDisableExtensions command-line flag prevents
// extension controlled preferences from being enacted.
class ExtensionPrefsDisableExtensions : public ExtensionPrefsPrepopulatedTest {
 public:
  ExtensionPrefsDisableExtensions()
      : iteration_(0) {}
  virtual ~ExtensionPrefsDisableExtensions() {}
  virtual void Initialize() OVERRIDE {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    // This becomes only active in the second verification phase.
    prefs_.set_extensions_disabled(true);
  }
  virtual void Verify() OVERRIDE {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    if (iteration_ == 0) {
      EXPECT_EQ("val1", actual);
      ++iteration_;
    } else {
      EXPECT_EQ(kDefaultPref1, actual);
    }
  }

 private:
  int iteration_;
};
TEST_F(ExtensionPrefsDisableExtensions, ExtensionPrefsDisableExtensions) {}

// Tests that blacklist state can be queried.
class ExtensionPrefsBlacklistedExtensions : public ExtensionPrefsTest {
 public:
  virtual ~ExtensionPrefsBlacklistedExtensions() {}

  virtual void Initialize() OVERRIDE {
    extension_a_ = prefs_.AddExtension("a");
    extension_b_ = prefs_.AddExtension("b");
    extension_c_ = prefs_.AddExtension("c");
  }

  virtual void Verify() OVERRIDE {
    {
      std::set<std::string> ids;
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }
    prefs()->SetExtensionBlacklisted(extension_a_->id(), true);
    {
      std::set<std::string> ids;
      ids.insert(extension_a_->id());
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }
    prefs()->SetExtensionBlacklisted(extension_b_->id(), true);
    prefs()->SetExtensionBlacklisted(extension_c_->id(), true);
    {
      std::set<std::string> ids;
      ids.insert(extension_a_->id());
      ids.insert(extension_b_->id());
      ids.insert(extension_c_->id());
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }
    prefs()->SetExtensionBlacklisted(extension_a_->id(), false);
    {
      std::set<std::string> ids;
      ids.insert(extension_b_->id());
      ids.insert(extension_c_->id());
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }
    prefs()->SetExtensionBlacklisted(extension_b_->id(), false);
    prefs()->SetExtensionBlacklisted(extension_c_->id(), false);
    {
      std::set<std::string> ids;
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }

    // The interesting part: make sure that we're cleaning up after ourselves
    // when we're storing *just* the fact that the extension is blacklisted.
    std::string arbitrary_id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    prefs()->SetExtensionBlacklisted(arbitrary_id, true);
    prefs()->SetExtensionBlacklisted(extension_a_->id(), true);

    // (And make sure that the acknowledged bit is also cleared).
    prefs()->AcknowledgeBlacklistedExtension(arbitrary_id);

    EXPECT_TRUE(prefs()->GetExtensionPref(arbitrary_id));
    {
      std::set<std::string> ids;
      ids.insert(arbitrary_id);
      ids.insert(extension_a_->id());
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }
    prefs()->SetExtensionBlacklisted(arbitrary_id, false);
    prefs()->SetExtensionBlacklisted(extension_a_->id(), false);
    EXPECT_FALSE(prefs()->GetExtensionPref(arbitrary_id));
    {
      std::set<std::string> ids;
      EXPECT_EQ(ids, prefs()->GetBlacklistedExtensions());
    }
  }

 private:
  scoped_refptr<const Extension> extension_a_;
  scoped_refptr<const Extension> extension_b_;
  scoped_refptr<const Extension> extension_c_;
};
TEST_F(ExtensionPrefsBlacklistedExtensions,
       ExtensionPrefsBlacklistedExtensions) {}

}  // namespace extensions
