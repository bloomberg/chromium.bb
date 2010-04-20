// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/extensions/extension_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

// Base class for tests.
class ExtensionPrefsTest : public testing::Test {
 public:
  ExtensionPrefsTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    FilePath preferences_file_ = temp_dir_.path().AppendASCII("Preferences");
    pref_service_.reset(new PrefService(preferences_file_));
    ExtensionPrefs::RegisterUserPrefs(pref_service_.get());
    CreateExtensionPrefs();
  }

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
    CreateExtensionPrefs();
    Verify();
  }

 protected:
  // Creates an ExtensionPrefs object.
  void CreateExtensionPrefs() {
    prefs_.reset(new ExtensionPrefs(pref_service_.get(), temp_dir_.path()));
  }

  // Creates a new Extension with the given name in our temp dir, adds it to
  // our ExtensionPrefs, and returns it.
  Extension* AddExtension(std::string name) {
    FilePath path = temp_dir_.path().AppendASCII(name);
    Extension* extension = new Extension(path);
    std::string errors;
    DictionaryValue dictionary;
    dictionary.SetString(extension_manifest_keys::kName, name);
    dictionary.SetString(extension_manifest_keys::kVersion, "0.1");
    EXPECT_TRUE(extension->InitFromValue(dictionary, false, &errors));
    extension->set_location(Extension::INTERNAL);
    EXPECT_TRUE(Extension::IdIsValid(extension->id()));
    prefs_->OnExtensionInstalled(extension);
    return extension;
  }

  // Creates an Extension and adds it to our ExtensionPrefs. Returns the
  // extension id it was assigned.
  std::string AddExtensionAndReturnId(std::string name) {
    scoped_ptr<Extension> extension(AddExtension(name));
    return extension->id();
  }

  ScopedTempDir temp_dir_;
  FilePath preferences_file_;
  scoped_ptr<PrefService> pref_service_;
  scoped_ptr<ExtensionPrefs> prefs_;

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
    extension_id_ = AddExtensionAndReturnId("last_ping_day");
    EXPECT_TRUE(prefs_->LastPingDay(extension_id_).is_null());
    prefs_->SetLastPingDay(extension_id_, extension_time_);
    prefs_->SetBlacklistLastPingDay(blacklist_time_);
  }

  virtual void Verify() {
    Time result = prefs_->LastPingDay(extension_id_);
    EXPECT_FALSE(result.is_null());
    EXPECT_TRUE(result == extension_time_);
    result = prefs_->BlacklistLastPingDay();
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
    list_.push_back(AddExtensionAndReturnId("1"));
    list_.push_back(AddExtensionAndReturnId("2"));
    list_.push_back(AddExtensionAndReturnId("3"));
    std::vector<std::string> before_list = prefs_->GetToolbarOrder();
    EXPECT_TRUE(before_list.empty());
    prefs_->SetToolbarOrder(list_);
  }

  virtual void Verify() {
    std::vector<std::string> result = prefs_->GetToolbarOrder();
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
    extension.reset(AddExtension("test"));
    prefs_->SetExtensionState(extension.get(), Extension::DISABLED);
  }

  virtual void Verify() {
    EXPECT_EQ(Extension::DISABLED, prefs_->GetExtensionState(extension->id()));
  }

 private:
  scoped_ptr<Extension> extension;
};
TEST_F(ExtensionPrefsExtensionState, ExtensionState) {}


class ExtensionPrefsEscalatePermissions : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    extension.reset(AddExtension("test"));
    prefs_->SetDidExtensionEscalatePermissions(extension.get(), true);
  }

  virtual void Verify() {
    EXPECT_EQ(true, prefs_->DidExtensionEscalatePermissions(extension->id()));
  }

 private:
  scoped_ptr<Extension> extension;
};
TEST_F(ExtensionPrefsEscalatePermissions, EscalatePermissions) {}


// Tests the GetVersionString function.
class ExtensionPrefsVersionString : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    extension.reset(AddExtension("test"));
    EXPECT_EQ("0.1", prefs_->GetVersionString(extension->id()));
    prefs_->OnExtensionUninstalled(extension->id(), Extension::INTERNAL, false);
  }

  virtual void Verify() {
    EXPECT_EQ("", prefs_->GetVersionString(extension->id()));
  }

 private:
  scoped_ptr<Extension> extension;
};
TEST_F(ExtensionPrefsVersionString, VersionString) {}

// Tests various areas of blacklist functionality.
class ExtensionPrefsBlacklist : public ExtensionPrefsTest {
 public:
  virtual void Initialize() {
    not_installed_id_ = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    // Install 5 extensions.
    for (int i = 0; i < 5; i++) {
      std::string name = "test" + IntToString(i);
      extensions_.push_back(linked_ptr<Extension>(AddExtension(name)));
    }
    EXPECT_EQ(NULL, prefs_->GetInstalledExtensionInfo(not_installed_id_));

    std::vector<linked_ptr<Extension> >::const_iterator iter;
    for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
      EXPECT_FALSE(prefs_->IsExtensionBlacklisted((*iter)->id()));
    }
    // Blacklist one installed and one not-installed extension id.
    std::set<std::string> blacklisted_ids;
    blacklisted_ids.insert(extensions_[0]->id());
    blacklisted_ids.insert(not_installed_id_);
    prefs_->UpdateBlacklist(blacklisted_ids);
  }

  virtual void Verify() {
    // Make sure the two id's we expect to be blacklisted are.
    EXPECT_TRUE(prefs_->IsExtensionBlacklisted(extensions_[0]->id()));
    EXPECT_TRUE(prefs_->IsExtensionBlacklisted(not_installed_id_));

    // Make sure the other id's are not blacklisted.
    std::vector<linked_ptr<Extension> >::const_iterator iter;
    for (iter = extensions_.begin() + 1; iter != extensions_.end(); ++iter) {
      EXPECT_FALSE(prefs_->IsExtensionBlacklisted((*iter)->id()));
    }

    // Make sure GetInstalledExtensionsInfo returns only the non-blacklisted
    // extensions data.
    scoped_ptr<ExtensionPrefs::ExtensionsInfo> info(
        prefs_->GetInstalledExtensionsInfo());
    EXPECT_EQ(4u, info->size());
    ExtensionPrefs::ExtensionsInfo::iterator info_iter;
    for (info_iter = info->begin(); info_iter != info->end(); ++info_iter) {
      ExtensionInfo* extension_info = info_iter->get();
      EXPECT_NE(extensions_[0]->id(), extension_info->extension_id);
    }
  }

 private:
  std::vector<linked_ptr<Extension> > extensions_;

  // An id we'll make up that doesn't match any installed extension id.
  std::string not_installed_id_;
};
TEST_F(ExtensionPrefsBlacklist, Blacklist) {}
