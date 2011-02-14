// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_source.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

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

static void AddPattern(ExtensionExtent* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

static void AssertEqualExtents(ExtensionExtent* extent1,
                               ExtensionExtent* extent2) {
  std::vector<URLPattern> patterns1 = extent1->patterns();
  std::vector<URLPattern> patterns2 = extent2->patterns();
  std::set<std::string> strings1;
  EXPECT_EQ(patterns1.size(), patterns2.size());

  for (size_t i = 0; i < patterns1.size(); ++i)
    strings1.insert(patterns1.at(i).GetAsString());

  std::set<std::string> strings2;
  for (size_t i = 0; i < patterns2.size(); ++i)
    strings2.insert(patterns2.at(i).GetAsString());

  EXPECT_EQ(strings1, strings2);
}

// Base class for tests.
class ExtensionPrefsTest : public testing::Test {
 public:
  ExtensionPrefsTest() {}

  // This function will get called once, and is the right place to do operations
  // on ExtensionPrefs that write data.
  virtual void Initialize() = 0;

  // This function will be called twice - once while the original ExtensionPrefs
  // object is still alive, and once after recreation. Thus, it tests that
  // things don't break after any ExtensionPrefs startup work.
  virtual void Verify() = 0;

  // This function is called to Register preference default values.
  virtual void RegisterPreferences() {}

  virtual void SetUp() {
    RegisterPreferences();
    Initialize();
  }

  virtual void TearDown() {
    Verify();

    // Reset ExtensionPrefs, and re-verify.
    prefs_.RecreateExtensionPrefs();
    RegisterPreferences();
    Verify();
  }

 protected:
  ExtensionPrefs* prefs() { return prefs_.prefs(); }

  TestExtensionPrefs prefs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefsTest);
};

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


// Tests the GetExtensionState/SetExtensionState functions.
class ExtensionPrefsExtensionState : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    extension = prefs_.AddExtension("test");
    prefs()->SetExtensionState(extension.get(), Extension::DISABLED);
  }

  virtual void Verify() {
    EXPECT_EQ(Extension::DISABLED, prefs()->GetExtensionState(extension->id()));
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

    api_perm_set1_.insert("tabs");
    api_perm_set1_.insert("bookmarks");
    api_perm_set1_.insert("something_random");

    api_perm_set2_.insert("history");
    api_perm_set2_.insert("unknown2");

    AddPattern(&host_perm_set1_, "http://*.google.com/*");
    AddPattern(&host_perm_set1_, "http://example.com/*");
    AddPattern(&host_perm_set1_, "chrome://favicon/*");

    AddPattern(&host_perm_set2_, "https://*.google.com/*");
    // with duplicate:
    AddPattern(&host_perm_set2_, "http://*.google.com/*");

    std::set_union(api_perm_set1_.begin(), api_perm_set1_.end(),
                   api_perm_set2_.begin(), api_perm_set2_.end(),
                   std::inserter(api_permissions_, api_permissions_.begin()));

    AddPattern(&host_permissions_, "http://*.google.com/*");
    AddPattern(&host_permissions_, "http://example.com/*");
    AddPattern(&host_permissions_, "chrome://favicon/*");
    AddPattern(&host_permissions_, "https://*.google.com/*");

    std::set<std::string> empty_set;
    std::set<std::string> api_perms;
    bool full_access = false;
    ExtensionExtent host_perms;
    ExtensionExtent empty_extent;

    // Make sure both granted api and host permissions start empty.
    EXPECT_FALSE(prefs()->GetGrantedPermissions(
        extension_id_, &full_access, &api_perms, &host_perms));

    EXPECT_TRUE(api_perms.empty());
    EXPECT_TRUE(host_perms.is_empty());

    // Add part of the api permissions.
    prefs()->AddGrantedPermissions(
        extension_id_, false, api_perm_set1_, empty_extent);
    EXPECT_TRUE(prefs()->GetGrantedPermissions(
        extension_id_, &full_access, &api_perms, &host_perms));
    EXPECT_EQ(api_perm_set1_, api_perms);
    EXPECT_TRUE(host_perms.is_empty());
    EXPECT_FALSE(full_access);
    host_perms.ClearPaths();
    api_perms.clear();

    // Add part of the host permissions.
    prefs()->AddGrantedPermissions(
        extension_id_, false, empty_set, host_perm_set1_);
    EXPECT_TRUE(prefs()->GetGrantedPermissions(
        extension_id_, &full_access, &api_perms, &host_perms));
    EXPECT_FALSE(full_access);
    EXPECT_EQ(api_perm_set1_, api_perms);
    AssertEqualExtents(&host_perm_set1_, &host_perms);
    host_perms.ClearPaths();
    api_perms.clear();

    // Add the rest of both the api and host permissions.
    prefs()->AddGrantedPermissions(extension_id_,
                                   true,
                                   api_perm_set2_,
                                   host_perm_set2_);

    EXPECT_TRUE(prefs()->GetGrantedPermissions(
        extension_id_, &full_access, &api_perms, &host_perms));
    EXPECT_TRUE(full_access);
    EXPECT_EQ(api_permissions_, api_perms);
    AssertEqualExtents(&host_permissions_, &host_perms);
  }

  virtual void Verify() {
    std::set<std::string> api_perms;
    ExtensionExtent host_perms;
    bool full_access;

    EXPECT_TRUE(prefs()->GetGrantedPermissions(
        extension_id_, &full_access, &api_perms, &host_perms));
    EXPECT_EQ(api_permissions_, api_perms);
    EXPECT_TRUE(full_access);
    AssertEqualExtents(&host_permissions_, &host_perms);
  }

 private:
  std::string extension_id_;
  std::set<std::string> api_perm_set1_;
  std::set<std::string> api_perm_set2_;
  ExtensionExtent host_perm_set1_;
  ExtensionExtent host_perm_set2_;


  std::set<std::string> api_permissions_;
  ExtensionExtent host_permissions_;
};
TEST_F(ExtensionPrefsGrantedPermissions, GrantedPermissions) {}

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
    EXPECT_EQ(Extension::ENABLED,
              prefs()->GetExtensionState(extension_->id()));
    EXPECT_FALSE(prefs()->IsIncognitoEnabled(extension_->id()));
    prefs()->OnExtensionInstalled(extension_.get(),
                                  Extension::DISABLED, true);
  }

  virtual void Verify() {
    EXPECT_EQ(Extension::DISABLED,
              prefs()->GetExtensionState(extension_->id()));
    EXPECT_TRUE(prefs()->IsIncognitoEnabled(extension_->id()));
  }

 private:
  scoped_refptr<Extension> extension_;
};
TEST_F(ExtensionPrefsOnExtensionInstalled,
       ExtensionPrefsOnExtensionInstalled) {}

class ExtensionPrefsAppLaunchIndex : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    // No extensions yet.
    EXPECT_EQ(0, prefs()->GetNextAppLaunchIndex());

    extension_ = prefs_.AddExtension("on_extension_installed");
    EXPECT_EQ(Extension::ENABLED,
        prefs()->GetExtensionState(extension_->id()));
    prefs()->OnExtensionInstalled(extension_.get(),
        Extension::ENABLED, false);
  }

  virtual void Verify() {
    int launch_index = prefs()->GetAppLaunchIndex(extension_->id());
    // Extension should have been assigned a launch index > 0.
    EXPECT_GT(launch_index, 0);
    EXPECT_EQ(launch_index + 1, prefs()->GetNextAppLaunchIndex());
    // Set a new launch index of one higher and verify.
    prefs()->SetAppLaunchIndex(extension_->id(),
        prefs()->GetNextAppLaunchIndex());
    int new_launch_index = prefs()->GetAppLaunchIndex(extension_->id());
    EXPECT_EQ(launch_index + 1, new_launch_index);

    // This extension doesn't exist, so it should return -1.
    EXPECT_EQ(-1, prefs()->GetAppLaunchIndex("foo"));
  }

 private:
  scoped_refptr<Extension> extension_;
};
TEST_F(ExtensionPrefsAppLaunchIndex, ExtensionPrefsAppLaunchIndex) {}

namespace keys = extension_manifest_keys;

class ExtensionPrefsPreferencesBase : public ExtensionPrefsTest {
 public:
  ExtensionPrefsPreferencesBase()
      : ExtensionPrefsTest(),
        ext1_(NULL),
        ext2_(NULL),
        ext3_(NULL),
        installed() {
    DictionaryValue simple_dict;
    std::string error;

    simple_dict.SetString(keys::kVersion, "1.0.0.0");
    simple_dict.SetString(keys::kName, "unused");

    ext1_scoped_ = Extension::Create(
        prefs_.temp_dir().AppendASCII("ext1_"), Extension::EXTERNAL_PREF,
        simple_dict, false, &error);
    ext2_scoped_ = Extension::Create(
        prefs_.temp_dir().AppendASCII("ext2_"), Extension::EXTERNAL_PREF,
        simple_dict, false, &error);
    ext3_scoped_ = Extension::Create(
        prefs_.temp_dir().AppendASCII("ext3_"), Extension::EXTERNAL_PREF,
        simple_dict, false, &error);

    ext1_ = ext1_scoped_.get();
    ext2_ = ext2_scoped_.get();
    ext3_ = ext3_scoped_.get();

    for (size_t i = 0; i < arraysize(installed); ++i)
      installed[i] = false;
  }

  void RegisterPreferences() {
    prefs()->pref_service()->RegisterStringPref(kPref1, kDefaultPref1);
    prefs()->pref_service()->RegisterStringPref(kPref2, kDefaultPref2);
    prefs()->pref_service()->RegisterStringPref(kPref3, kDefaultPref3);
    prefs()->pref_service()->RegisterStringPref(kPref4, kDefaultPref4);
  }

  void InstallExtControlledPref(Extension *ext,
                                const std::string& key,
                                Value* val) {
    EnsureExtensionInstalled(ext);
    prefs()->SetExtensionControlledPref(ext->id(), key, false, val);
  }

  void InstallExtControlledPrefIncognito(Extension *ext,
                                         const std::string& key,
                                         Value* val) {
    EnsureExtensionInstalled(ext);
    prefs()->SetExtensionControlledPref(ext->id(), key, true, val);
  }

  void InstallExtension(Extension *ext) {
    EnsureExtensionInstalled(ext);
  }

  void UninstallExtension(const std::string& extension_id) {
    EnsureExtensionUninstalled(extension_id);
  }

  // Weak references, for convenience.
  Extension* ext1_;
  Extension* ext2_;
  Extension* ext3_;

  // Flags indicating whether each of the extensions has been installed, yet.
  bool installed[3];

 private:
  void EnsureExtensionInstalled(Extension *ext) {
    // Install extension the first time a preference is set for it.
    Extension* extensions[] = {ext1_, ext2_, ext3_};
    for (int i = 0; i < 3; ++i) {
      if (ext == extensions[i] && !installed[i]) {
        prefs()->OnExtensionInstalled(ext, Extension::ENABLED, true);
        installed[i] = true;
        break;
      }
    }
  }

  void EnsureExtensionUninstalled(const std::string& extension_id) {
    Extension* extensions[] = {ext1_, ext2_, ext3_};
    for (int i = 0; i < 3; ++i) {
      if (extensions[i]->id() == extension_id) {
        installed[i] = false;
        break;
      }
    }
    prefs()->OnExtensionUninstalled(extension_id, Extension::INTERNAL, false);
  }

  scoped_refptr<Extension> ext1_scoped_;
  scoped_refptr<Extension> ext2_scoped_;
  scoped_refptr<Extension> ext3_scoped_;
};

class ExtensionPrefsInstallOneExtension
    : public ExtensionPrefsPreferencesBase {
  virtual void Initialize() {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
  }
  virtual void Verify() {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
  }
};
TEST_F(ExtensionPrefsInstallOneExtension, ExtensionPrefsInstallOneExtension) {}

// Check that we forget incognito values after a reload.
class ExtensionPrefsInstallIncognito
    : public ExtensionPrefsPreferencesBase {
 public:
  ExtensionPrefsInstallIncognito() : iteration_(0) {}

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
    // Incognito pref service shall see incognito values only during first run.
    // Once the pref service was reloaded, all values shall be discarded.
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
TEST_F(ExtensionPrefsInstallIncognito, ExtensionPrefsInstallOneExtension) {}

class ExtensionPrefsUninstallExtension
    : public ExtensionPrefsPreferencesBase {
  virtual void Initialize() {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    InstallExtControlledPref(ext1_, kPref2, Value::CreateStringValue("val2"));

    UninstallExtension(ext1_->id());
  }
  virtual void Verify() {
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
class ExtensionPrefsNotifyWhenNeeded
    : public ExtensionPrefsPreferencesBase {
  virtual void Initialize() {
    using testing::_;
    using testing::Mock;
    using testing::StrEq;

    NotificationObserverMock observer;
    PrefChangeRegistrar registrar;
    registrar.Init(prefs()->pref_service());
    registrar.Add(kPref1, &observer);

    NotificationObserverMock incognito_observer;
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

    // Change only incognito value.
    EXPECT_CALL(observer, Observe(_, _, _)).Times(0);
    EXPECT_CALL(incognito_observer, Observe(_, _, _));
    InstallExtControlledPrefIncognito(ext1_, kPref1,
        Value::CreateStringValue("chrome://newtab2"));
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
class ExtensionPrefsDisableExt
    : public ExtensionPrefsPreferencesBase {
  virtual void Initialize() {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
    prefs()->SetExtensionState(ext1_, Extension::DISABLED);
  }
  virtual void Verify() {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ(kDefaultPref1, actual);
  }
};
TEST_F(ExtensionPrefsDisableExt,  ExtensionPrefsDisableExt) {}

// Tests disabling and reenabling an extension.
class ExtensionPrefsReenableExt
    : public ExtensionPrefsPreferencesBase {
  virtual void Initialize() {
    InstallExtControlledPref(ext1_, kPref1, Value::CreateStringValue("val1"));
    prefs()->SetExtensionState(ext1_, Extension::DISABLED);
    prefs()->SetExtensionState(ext1_, Extension::ENABLED);
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
    : public ExtensionPrefsPreferencesBase {
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
