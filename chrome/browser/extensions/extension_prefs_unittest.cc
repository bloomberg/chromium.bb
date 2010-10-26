// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

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

  virtual void SetUp() {
    Initialize();
  }

  virtual void TearDown() {
    Verify();

    // Reset ExtensionPrefs, and re-verify.
    prefs_.RecreateExtensionPrefs();
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
    EXPECT_EQ(true, prefs()->DidExtensionEscalatePermissions(extension->id()));
  }

 private:
  scoped_refptr<Extension> extension;
};
TEST_F(ExtensionPrefsEscalatePermissions, EscalatePermissions) {}


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
