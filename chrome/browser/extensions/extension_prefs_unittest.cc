// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_prefs_unittest.h"

#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/string_ordinal.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/test/notification_observer_mock.h"

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

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

ExtensionPrefsTest::ExtensionPrefsTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      file_thread_(BrowserThread::FILE, &message_loop_) {
}

ExtensionPrefsTest::~ExtensionPrefsTest() {}

void ExtensionPrefsTest::RegisterPreferences() {}

void ExtensionPrefsTest::SetUp() {
  RegisterPreferences();
  Initialize();
}

void ExtensionPrefsTest::TearDown() {
  Verify();

  // Reset ExtensionPrefs, and re-verify.
  prefs_.RecreateExtensionPrefs();
  RegisterPreferences();
  Verify();
}

// Tests the LastPingDay/SetLastPingDay functions.
class ExtensionPrefsLastPingDay : public ExtensionPrefsTest {
 public:
  ExtensionPrefsLastPingDay()
      : extension_time_(Time::Now() - TimeDelta::FromHours(4)),
        blacklist_time_(Time::Now() - TimeDelta::FromHours(2)) {}

  virtual void Initialize() {
    extension_id_ = prefs_.AddExtensionAndReturnId("last_ping_day");
    EXPECT_TRUE(prefs()->LastPingDay(extension_id_).is_null());
    prefs()->SetLastPingDay(extension_id_, extension_time_);
    prefs()->SetBlacklistLastPingDay(blacklist_time_);
  }

  virtual void Verify() {
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


// Tests the GetToolbarOrder/SetToolbarOrder functions.
class ExtensionPrefsToolbarOrder : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    list_.push_back(prefs_.AddExtensionAndReturnId("1"));
    list_.push_back(prefs_.AddExtensionAndReturnId("2"));
    list_.push_back(prefs_.AddExtensionAndReturnId("3"));
    std::vector<std::string> before_list = prefs()->GetToolbarOrder();
    EXPECT_TRUE(before_list.empty());
    prefs()->SetToolbarOrder(list_);
  }

  virtual void Verify() {
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
  virtual void Initialize() {
    extension = prefs_.AddExtension("test");
    prefs()->SetExtensionState(extension->id(), Extension::DISABLED);
  }

  virtual void Verify() {
    EXPECT_TRUE(prefs()->IsExtensionDisabled(extension->id()));
  }

 private:
  scoped_refptr<Extension> extension;
};
TEST_F(ExtensionPrefsExtensionState, ExtensionState) {}


class ExtensionPrefsEscalatePermissions : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    extension = prefs_.AddExtension("test");
    prefs()->SetDidExtensionEscalatePermissions(extension.get(), true);
  }

  virtual void Verify() {
    EXPECT_TRUE(prefs()->DidExtensionEscalatePermissions(extension->id()));
  }

 private:
  scoped_refptr<Extension> extension;
};
TEST_F(ExtensionPrefsEscalatePermissions, EscalatePermissions) {}

// Tests the AddGrantedPermissions / GetGrantedPermissions functions.
class ExtensionPrefsGrantedPermissions : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    extension_id_ = prefs_.AddExtensionAndReturnId("test");

    api_perm_set1_.insert(ExtensionAPIPermission::kTab);
    api_perm_set1_.insert(ExtensionAPIPermission::kBookmark);

    api_perm_set2_.insert(ExtensionAPIPermission::kHistory);

    scopes_set1_.insert("scope1");

    scopes_set2_.insert("scope1");
    scopes_set2_.insert("scope2");
    scopes_set2_.insert("scope3");

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

    ExtensionAPIPermissionSet expected_apis = api_perm_set1_;

    AddPattern(&ehost_permissions_, "http://*.google.com/*");
    AddPattern(&ehost_permissions_, "http://example.com/*");
    AddPattern(&ehost_permissions_, "chrome://favicon/*");
    AddPattern(&ehost_permissions_, "https://*.google.com/*");

    AddPattern(&shost_permissions_, "http://reddit.com/r/test/*");
    AddPattern(&shost_permissions_, "http://somesite.com/*");
    AddPattern(&shost_permissions_, "http://example.com/*");

    scope_permissions_.insert("scope1");
    scope_permissions_.insert("scope2");
    scope_permissions_.insert("scope3");

    ExtensionAPIPermissionSet empty_set;
    URLPatternSet empty_extent;
    ExtensionOAuth2Scopes empty_scopes;
    scoped_refptr<ExtensionPermissionSet> permissions;
    scoped_refptr<ExtensionPermissionSet> granted_permissions;

    // Make sure both granted api and host permissions start empty.
    granted_permissions =
        prefs()->GetGrantedPermissions(extension_id_);
    EXPECT_TRUE(granted_permissions->IsEmpty());

    permissions = new ExtensionPermissionSet(
        api_perm_set1_, empty_extent, empty_extent);

    // Add part of the api permissions.
    prefs()->AddGrantedPermissions(extension_id_, permissions.get());
    granted_permissions = prefs()->GetGrantedPermissions(extension_id_);
    EXPECT_TRUE(granted_permissions.get());
    EXPECT_FALSE(granted_permissions->IsEmpty());
    EXPECT_EQ(expected_apis, granted_permissions->apis());
    EXPECT_TRUE(granted_permissions->effective_hosts().is_empty());
    EXPECT_FALSE(granted_permissions->HasEffectiveFullAccess());
    EXPECT_EQ(empty_scopes, granted_permissions->scopes());
    granted_permissions = NULL;

    // Add part of the explicit host permissions.
    permissions = new ExtensionPermissionSet(
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
    EXPECT_EQ(empty_scopes, granted_permissions->scopes());

    // Add part of the scriptable host permissions.
    permissions = new ExtensionPermissionSet(
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
    EXPECT_EQ(empty_scopes, granted_permissions->scopes());

    URLPatternSet::CreateUnion(ehost_perm_set1_, shost_perm_set1_,
                               &effective_permissions_);
    EXPECT_EQ(effective_permissions_, granted_permissions->effective_hosts());

    // Add part of the oauth2 scopes.
    permissions = new ExtensionPermissionSet(
        empty_set, empty_extent, empty_extent, scopes_set1_);
    prefs()->AddGrantedPermissions(extension_id_, permissions.get());
    granted_permissions = prefs()->GetGrantedPermissions(extension_id_);
    EXPECT_FALSE(granted_permissions->IsEmpty());
    EXPECT_FALSE(granted_permissions->HasEffectiveFullAccess());
    EXPECT_EQ(expected_apis, granted_permissions->apis());
    EXPECT_EQ(ehost_perm_set1_,
              granted_permissions->explicit_hosts());
    EXPECT_EQ(shost_perm_set1_,
              granted_permissions->scriptable_hosts());
    EXPECT_EQ(scopes_set1_, granted_permissions->scopes());

    // Add the rest of the permissions.
    permissions = new ExtensionPermissionSet(
        api_perm_set2_, ehost_perm_set2_, shost_perm_set2_, scopes_set2_);

    std::set_union(expected_apis.begin(), expected_apis.end(),
                   api_perm_set2_.begin(), api_perm_set2_.end(),
                   std::inserter(api_permissions_, api_permissions_.begin()));

    prefs()->AddGrantedPermissions(extension_id_, permissions.get());
    granted_permissions = prefs()->GetGrantedPermissions(extension_id_);
    EXPECT_TRUE(granted_permissions.get());
    EXPECT_FALSE(granted_permissions->IsEmpty());
    EXPECT_EQ(api_permissions_, granted_permissions->apis());
    EXPECT_EQ(ehost_permissions_,
              granted_permissions->explicit_hosts());
    EXPECT_EQ(shost_permissions_,
              granted_permissions->scriptable_hosts());
    EXPECT_EQ(scope_permissions_, granted_permissions->scopes());
    effective_permissions_.ClearPatterns();
    URLPatternSet::CreateUnion(ehost_permissions_, shost_permissions_,
                               &effective_permissions_);
    EXPECT_EQ(effective_permissions_, granted_permissions->effective_hosts());
  }

  virtual void Verify() {
    scoped_refptr<ExtensionPermissionSet> permissions(
        prefs()->GetGrantedPermissions(extension_id_));
    EXPECT_TRUE(permissions.get());
    EXPECT_FALSE(permissions->HasEffectiveFullAccess());
    EXPECT_EQ(api_permissions_, permissions->apis());
    EXPECT_EQ(ehost_permissions_,
              permissions->explicit_hosts());
    EXPECT_EQ(shost_permissions_,
              permissions->scriptable_hosts());
    EXPECT_EQ(scope_permissions_,
              permissions->scopes());
  }

 private:
  std::string extension_id_;
  ExtensionAPIPermissionSet api_perm_set1_;
  ExtensionAPIPermissionSet api_perm_set2_;
  URLPatternSet ehost_perm_set1_;
  URLPatternSet ehost_perm_set2_;
  URLPatternSet shost_perm_set1_;
  URLPatternSet shost_perm_set2_;
  ExtensionOAuth2Scopes scopes_set1_;
  ExtensionOAuth2Scopes scopes_set2_;

  ExtensionAPIPermissionSet api_permissions_;
  URLPatternSet ehost_permissions_;
  URLPatternSet shost_permissions_;
  ExtensionOAuth2Scopes scope_permissions_;
  URLPatternSet effective_permissions_;
};
TEST_F(ExtensionPrefsGrantedPermissions, GrantedPermissions) {}

// Tests the SetActivePermissions / GetActivePermissions functions.
class ExtensionPrefsActivePermissions : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    extension_id_ = prefs_.AddExtensionAndReturnId("test");

    ExtensionAPIPermissionSet api_perms;
    api_perms.insert(ExtensionAPIPermission::kTab);
    api_perms.insert(ExtensionAPIPermission::kBookmark);
    api_perms.insert(ExtensionAPIPermission::kHistory);

    URLPatternSet ehosts;
    AddPattern(&ehosts, "http://*.google.com/*");
    AddPattern(&ehosts, "http://example.com/*");
    AddPattern(&ehosts, "chrome://favicon/*");

    URLPatternSet shosts;
    AddPattern(&shosts, "https://*.google.com/*");
    AddPattern(&shosts, "http://reddit.com/r/test/*");

    ExtensionOAuth2Scopes scopes;
    scopes.insert("my-new-scope");

    active_perms_ = new ExtensionPermissionSet(
        api_perms, ehosts, shosts, scopes);

    // Make sure the active permissions start empty.
    scoped_refptr<ExtensionPermissionSet> active(
        prefs()->GetActivePermissions(extension_id_));
    EXPECT_TRUE(active->IsEmpty());

    // Set the active permissions.
    prefs()->SetActivePermissions(extension_id_, active_perms_.get());
    active = prefs()->GetActivePermissions(extension_id_);
    EXPECT_EQ(active_perms_->apis(), active->apis());
    EXPECT_EQ(active_perms_->explicit_hosts(), active->explicit_hosts());
    EXPECT_EQ(active_perms_->scriptable_hosts(), active->scriptable_hosts());
    EXPECT_EQ(active_perms_->scopes(), active->scopes());
    EXPECT_EQ(*active_perms_, *active);
  }

  virtual void Verify() {
    scoped_refptr<ExtensionPermissionSet> permissions(
        prefs()->GetActivePermissions(extension_id_));
    EXPECT_EQ(*active_perms_, *permissions);
  }

 private:
  std::string extension_id_;
  scoped_refptr<ExtensionPermissionSet> active_perms_;
};
TEST_F(ExtensionPrefsActivePermissions, SetAndGetActivePermissions) {}

// Tests the GetVersionString function.
class ExtensionPrefsVersionString : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    extension = prefs_.AddExtension("test");
    EXPECT_EQ("0.1", prefs()->GetVersionString(extension->id()));
    prefs()->OnExtensionUninstalled(extension->id(),
                                    Extension::INTERNAL, false);
  }

  virtual void Verify() {
    EXPECT_EQ("", prefs()->GetVersionString(extension->id()));
  }

 private:
  scoped_refptr<Extension> extension;
};
TEST_F(ExtensionPrefsVersionString, VersionString) {}

// Tests various areas of blacklist functionality.
class ExtensionPrefsBlacklist : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    not_installed_id_ = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    // Install 5 extensions.
    for (int i = 0; i < 5; i++) {
      std::string name = "test" + base::IntToString(i);
      extensions_.push_back(prefs_.AddExtension(name));
    }
    EXPECT_EQ(NULL, prefs()->GetInstalledExtensionInfo(not_installed_id_));

    ExtensionList::const_iterator iter;
    for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
      EXPECT_FALSE(prefs()->IsExtensionBlacklisted((*iter)->id()));
    }
    // Blacklist one installed and one not-installed extension id.
    std::set<std::string> blacklisted_ids;
    blacklisted_ids.insert(extensions_[0]->id());
    blacklisted_ids.insert(not_installed_id_);
    prefs()->UpdateBlacklist(blacklisted_ids);
  }

  virtual void Verify() {
    // Make sure the two id's we expect to be blacklisted are.
    EXPECT_TRUE(prefs()->IsExtensionBlacklisted(extensions_[0]->id()));
    EXPECT_TRUE(prefs()->IsExtensionBlacklisted(not_installed_id_));

    // Make sure the other id's are not blacklisted.
    ExtensionList::const_iterator iter;
    for (iter = extensions_.begin() + 1; iter != extensions_.end(); ++iter) {
      EXPECT_FALSE(prefs()->IsExtensionBlacklisted((*iter)->id()));
    }

    // Make sure GetInstalledExtensionsInfo returns only the non-blacklisted
    // extensions data.
    scoped_ptr<ExtensionPrefs::ExtensionsInfo> info(
        prefs()->GetInstalledExtensionsInfo());
    EXPECT_EQ(4u, info->size());
    ExtensionPrefs::ExtensionsInfo::iterator info_iter;
    for (info_iter = info->begin(); info_iter != info->end(); ++info_iter) {
      ExtensionInfo* extension_info = info_iter->get();
      EXPECT_NE(extensions_[0]->id(), extension_info->extension_id);
    }
  }

 private:
  ExtensionList extensions_;

  // An id we'll make up that doesn't match any installed extension id.
  std::string not_installed_id_;
};
TEST_F(ExtensionPrefsBlacklist, Blacklist) {}

class ExtensionPrefsAcknowledgment : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    not_installed_id_ = "pghjnghklobnfoidcldiidjjjhkeeaoi";

    // Install some extensions.
    for (int i = 0; i < 5; i++) {
      std::string name = "test" + base::IntToString(i);
      extensions_.push_back(prefs_.AddExtension(name));
    }
    EXPECT_EQ(NULL, prefs()->GetInstalledExtensionInfo(not_installed_id_));

    ExtensionList::const_iterator iter;
    for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
      std::string id = (*iter)->id();
      EXPECT_FALSE(prefs()->IsExternalExtensionAcknowledged(id));
      EXPECT_FALSE(prefs()->IsBlacklistedExtensionAcknowledged(id));
      EXPECT_FALSE(prefs()->IsOrphanedExtensionAcknowledged(id));
      if (external_id_.empty()) {
        external_id_ = id;
        continue;
      }
      if (blacklisted_id_.empty()) {
        blacklisted_id_ = id;
        continue;
      }
      if (orphaned_id_.empty()) {
        orphaned_id_ = id;
        continue;
      }
    }
    // For each type of acknowledgment, acknowledge one installed and one
    // not-installed extension id.
    prefs()->AcknowledgeExternalExtension(external_id_);
    prefs()->AcknowledgeBlacklistedExtension(blacklisted_id_);
    prefs()->AcknowledgeOrphanedExtension(orphaned_id_);
    prefs()->AcknowledgeExternalExtension(not_installed_id_);
    prefs()->AcknowledgeBlacklistedExtension(not_installed_id_);
    prefs()->AcknowledgeOrphanedExtension(not_installed_id_);
  }

  virtual void Verify() {
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
      if (id == orphaned_id_) {
        EXPECT_TRUE(prefs()->IsOrphanedExtensionAcknowledged(id));
      } else {
        EXPECT_FALSE(prefs()->IsOrphanedExtensionAcknowledged(id));
      }
    }
    EXPECT_TRUE(prefs()->IsExternalExtensionAcknowledged(not_installed_id_));
    EXPECT_TRUE(prefs()->IsBlacklistedExtensionAcknowledged(not_installed_id_));
    EXPECT_TRUE(prefs()->IsOrphanedExtensionAcknowledged(not_installed_id_));
  }

 private:
  ExtensionList extensions_;

  std::string not_installed_id_;
  std::string external_id_;
  std::string blacklisted_id_;
  std::string orphaned_id_;
};
TEST_F(ExtensionPrefsAcknowledgment, Acknowledgment) {}

// Tests force hiding browser actions.
class ExtensionPrefsHidingBrowserActions : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
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

  virtual void Verify() {
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
class ExtensionPrefsIdleInstallInfo : public ExtensionPrefsTest {
 public:
  // Sets idle install information for one test extension.
  void SetIdleInfo(std::string id, int num) {
    prefs()->SetIdleInstallInfo(id,
                                basedir_.AppendASCII(base::IntToString(num)),
                                "1." + base::IntToString(num),
                                now_ + TimeDelta::FromSeconds(num));
  }

  // Verifies that we get back expected idle install information previously
  // set by SetIdleInfo.
  void VerifyIdleInfo(std::string id, int num) {
    FilePath crx_path;
    std::string version;
    base::Time fetch_time;
    ASSERT_TRUE(prefs()->GetIdleInstallInfo(id, &crx_path, &version,
                                            &fetch_time));
    ASSERT_EQ(crx_path.value(),
              basedir_.AppendASCII(base::IntToString(num)).value());
    ASSERT_EQ("1." + base::IntToString(num), version);
    ASSERT_TRUE(fetch_time == now_ + TimeDelta::FromSeconds(num));
  }

  virtual void Initialize() {
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
    std::set<std::string> ids = prefs()->GetIdleInstallInfoIds();
    EXPECT_EQ(2u, ids.size());
    EXPECT_TRUE(ContainsKey(ids, id1_));
    EXPECT_TRUE(ContainsKey(ids, id2_));
    prefs()->RemoveIdleInstallInfo(id1_);
    prefs()->RemoveIdleInstallInfo(id2_);
    ids = prefs()->GetIdleInstallInfoIds();
    EXPECT_TRUE(ids.empty());

    // Try getting/removing info for an id that used to have info set.
    EXPECT_FALSE(prefs()->GetIdleInstallInfo(id1_, NULL, NULL, NULL));
    EXPECT_FALSE(prefs()->RemoveIdleInstallInfo(id1_));

    // Try getting/removing info for an id that has not yet had any info set.
    EXPECT_FALSE(prefs()->GetIdleInstallInfo(id3_, NULL, NULL, NULL));
    EXPECT_FALSE(prefs()->RemoveIdleInstallInfo(id3_));

    // Set info for 4 extensions, then remove for one of them.
    SetIdleInfo(id1_, 1);
    SetIdleInfo(id2_, 2);
    SetIdleInfo(id3_, 3);
    SetIdleInfo(id4_, 4);
    VerifyIdleInfo(id1_, 1);
    VerifyIdleInfo(id2_, 2);
    VerifyIdleInfo(id3_, 3);
    VerifyIdleInfo(id4_, 4);
    prefs()->RemoveIdleInstallInfo(id3_);
  }

  virtual void Verify() {
    // Make sure the info for the 3 extensions we expect is present.
    std::set<std::string> ids = prefs()->GetIdleInstallInfoIds();
    EXPECT_EQ(3u, ids.size());
    EXPECT_TRUE(ContainsKey(ids, id1_));
    EXPECT_TRUE(ContainsKey(ids, id2_));
    EXPECT_TRUE(ContainsKey(ids, id4_));
    VerifyIdleInfo(id1_, 1);
    VerifyIdleInfo(id2_, 2);
    VerifyIdleInfo(id4_, 4);

    // Make sure there isn't info the for the one extension id we removed.
    EXPECT_FALSE(prefs()->GetIdleInstallInfo(id3_, NULL, NULL, NULL));
  }

 protected:
  Time now_;
  FilePath basedir_;
  std::string id1_;
  std::string id2_;
  std::string id3_;
  std::string id4_;
};
TEST_F(ExtensionPrefsIdleInstallInfo, IdleInstallInfo) {}

class ExtensionPrefsOnExtensionInstalled : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    extension_ = prefs_.AddExtension("on_extension_installed");
    EXPECT_FALSE(prefs()->IsExtensionDisabled(extension_->id()));
    prefs()->OnExtensionInstalled(
        extension_.get(), Extension::DISABLED, false,
        StringOrdinal());
  }

  virtual void Verify() {
    EXPECT_TRUE(prefs()->IsExtensionDisabled(extension_->id()));
  }

 private:
  scoped_refptr<Extension> extension_;
};
TEST_F(ExtensionPrefsOnExtensionInstalled,
       ExtensionPrefsOnExtensionInstalled) {}

class ExtensionPrefsAppDraggedByUser : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    extension_ = prefs_.AddExtension("on_extension_installed");
    EXPECT_FALSE(prefs()->WasAppDraggedByUser(extension_->id()));
    prefs()->OnExtensionInstalled(extension_.get(), Extension::ENABLED,
                                  false, StringOrdinal());
  }

  virtual void Verify() {
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
  virtual void Initialize() {
    {
      base::DictionaryValue dictionary;
      dictionary.SetString(extension_manifest_keys::kName, "from_webstore");
      dictionary.SetString(extension_manifest_keys::kVersion, "0.1");
      webstore_extension_ = prefs_.AddExtensionWithManifestAndFlags(
          dictionary, Extension::INTERNAL,
          Extension::STRICT_ERROR_CHECKS | Extension::FROM_WEBSTORE);
    }

    {
      base::DictionaryValue dictionary;
      dictionary.SetString(extension_manifest_keys::kName, "from_bookmark");
      dictionary.SetString(extension_manifest_keys::kVersion, "0.1");
      bookmark_extension_ = prefs_.AddExtensionWithManifestAndFlags(
          dictionary, Extension::INTERNAL,
          Extension::STRICT_ERROR_CHECKS | Extension::FROM_BOOKMARK);
    }
  }

  virtual void Verify() {
    EXPECT_TRUE(prefs()->IsFromWebStore(webstore_extension_->id()));
    EXPECT_FALSE(prefs()->IsFromBookmark(webstore_extension_->id()));

    EXPECT_TRUE(prefs()->IsFromBookmark(bookmark_extension_->id()));
    EXPECT_FALSE(prefs()->IsFromWebStore(bookmark_extension_->id()));
  }

 private:
  scoped_refptr<Extension> webstore_extension_;
  scoped_refptr<Extension> bookmark_extension_;
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
      prefs_.temp_dir().AppendASCII("ext1_"), Extension::EXTERNAL_PREF,
      simple_dict, Extension::STRICT_ERROR_CHECKS, &error);
  ext2_scoped_ = Extension::Create(
      prefs_.temp_dir().AppendASCII("ext2_"), Extension::EXTERNAL_PREF,
      simple_dict, Extension::STRICT_ERROR_CHECKS, &error);
  ext3_scoped_ = Extension::Create(
      prefs_.temp_dir().AppendASCII("ext3_"), Extension::EXTERNAL_PREF,
      simple_dict, Extension::STRICT_ERROR_CHECKS, &error);
  ext4_scoped_ = Extension::Create(
      prefs_.temp_dir().AppendASCII("ext4_"), Extension::EXTERNAL_PREF,
      simple_dict, Extension::STRICT_ERROR_CHECKS, &error);

  ext1_ = ext1_scoped_.get();
  ext2_ = ext2_scoped_.get();
  ext3_ = ext3_scoped_.get();
  ext4_ = ext4_scoped_.get();

  for (size_t i = 0; i < arraysize(installed); ++i)
    installed[i] = false;
}

ExtensionPrefsPrepopulatedTest::~ExtensionPrefsPrepopulatedTest() {}

void ExtensionPrefsPrepopulatedTest::RegisterPreferences() {
  prefs()->pref_service()->RegisterStringPref(kPref1,
                                              kDefaultPref1,
                                              PrefService::UNSYNCABLE_PREF);
  prefs()->pref_service()->RegisterStringPref(kPref2,
                                              kDefaultPref2,
                                              PrefService::UNSYNCABLE_PREF);
  prefs()->pref_service()->RegisterStringPref(kPref3,
                                              kDefaultPref3,
                                              PrefService::UNSYNCABLE_PREF);
  prefs()->pref_service()->RegisterStringPref(kPref4,
                                              kDefaultPref4,
                                              PrefService::UNSYNCABLE_PREF);
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
                                    false, StringOrdinal());
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
  prefs()->OnExtensionUninstalled(extension_id, Extension::INTERNAL, false);
}

class ExtensionPrefsInstallOneExtension
    : public ExtensionPrefsPrepopulatedTest {
  virtual void Initialize() {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
  }
  virtual void Verify() {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
  }
};
TEST_F(ExtensionPrefsInstallOneExtension, ExtensionPrefsInstallOneExtension) {}

// Check that we do not forget persistent incognito values after a reload.
class ExtensionPrefsInstallIncognitoPersistent
    : public ExtensionPrefsPrepopulatedTest {
 public:
  virtual void Initialize() {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    InstallExtControlledPrefIncognito(ext1_, kPref1,
                                      Value::CreateStringValue("val2"));
    scoped_ptr<PrefService> incog_prefs(prefs_.CreateIncognitoPrefService());
    std::string actual = incog_prefs->GetString(kPref1);
    EXPECT_EQ("val2", actual);
  }
  virtual void Verify() {
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

  virtual void Initialize() {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    InstallExtControlledPrefIncognitoSessionOnly(
        ext1_, kPref1, Value::CreateStringValue("val2"));
    scoped_ptr<PrefService> incog_prefs(prefs_.CreateIncognitoPrefService());
    std::string actual = incog_prefs->GetString(kPref1);
    EXPECT_EQ("val2", actual);
  }
  virtual void Verify() {
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
  virtual void Initialize() {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    InstallExtControlledPref(ext1_, kPref2, Value::CreateStringValue("val2"));
    extensions::ContentSettingsStore* store = prefs()->content_settings_store();
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
  virtual void Verify() {
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
  virtual void Initialize() {
    using testing::_;
    using testing::Mock;
    using testing::StrEq;

    content::NotificationObserverMock observer;
    PrefChangeRegistrar registrar;
    registrar.Init(prefs()->pref_service());
    registrar.Add(kPref1, &observer);

    content::NotificationObserverMock incognito_observer;
    scoped_ptr<PrefService> incog_prefs(prefs_.CreateIncognitoPrefService());
    PrefChangeRegistrar incognito_registrar;
    incognito_registrar.Init(incog_prefs.get());
    incognito_registrar.Add(kPref1, &incognito_observer);

    // Write value and check notification.
    EXPECT_CALL(observer, Observe(_, _, _));
    EXPECT_CALL(incognito_observer, Observe(_, _, _));
    InstallExtControlledPref(ext1_, kPref1,
        Value::CreateStringValue("https://www.chromium.org"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Write same value.
    EXPECT_CALL(observer, Observe(_, _, _)).Times(0);
    EXPECT_CALL(incognito_observer, Observe(_, _, _)).Times(0);
    InstallExtControlledPref(ext1_, kPref1,
        Value::CreateStringValue("https://www.chromium.org"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Change value.
    EXPECT_CALL(observer, Observe(_, _, _));
    EXPECT_CALL(incognito_observer, Observe(_, _, _));
    InstallExtControlledPref(ext1_, kPref1,
        Value::CreateStringValue("chrome://newtab"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Change only incognito persistent value.
    EXPECT_CALL(observer, Observe(_, _, _)).Times(0);
    EXPECT_CALL(incognito_observer, Observe(_, _, _));
    InstallExtControlledPrefIncognito(ext1_, kPref1,
        Value::CreateStringValue("chrome://newtab2"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Change only incognito session-only value.
    EXPECT_CALL(observer, Observe(_, _, _)).Times(0);
    EXPECT_CALL(incognito_observer, Observe(_, _, _));
    InstallExtControlledPrefIncognito(ext1_, kPref1,
        Value::CreateStringValue("chrome://newtab3"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Uninstall.
    EXPECT_CALL(observer, Observe(_, _, _));
    EXPECT_CALL(incognito_observer, Observe(_, _, _));
    UninstallExtension(ext1_->id());
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    registrar.Remove(kPref1, &observer);
    incognito_registrar.Remove(kPref1, &incognito_observer);
  }
  virtual void Verify() {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ(kDefaultPref1, actual);
  }
};
TEST_F(ExtensionPrefsNotifyWhenNeeded,
    ExtensionPrefsNotifyWhenNeeded) {}

// Tests disabling an extension.
class ExtensionPrefsDisableExt : public ExtensionPrefsPrepopulatedTest {
  virtual void Initialize() {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
    prefs()->SetExtensionState(ext1_->id(), Extension::DISABLED);
  }
  virtual void Verify() {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ(kDefaultPref1, actual);
  }
};
TEST_F(ExtensionPrefsDisableExt,  ExtensionPrefsDisableExt) {}

// Tests disabling and reenabling an extension.
class ExtensionPrefsReenableExt : public ExtensionPrefsPrepopulatedTest {
  virtual void Initialize() {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    prefs()->SetExtensionState(ext1_->id(), Extension::DISABLED);
    prefs()->SetExtensionState(ext1_->id(), Extension::ENABLED);
  }
  virtual void Verify() {
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
  virtual void Initialize() {
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

  virtual void Verify() {
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
  virtual void Initialize() {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    // This becomes only active in the second verification phase.
    prefs_.set_extensions_disabled(true);
  }
  virtual void Verify() {
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
