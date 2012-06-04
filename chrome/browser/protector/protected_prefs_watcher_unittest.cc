// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_pref_value_map_factory.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/protector/protected_prefs_watcher.h"
#include "chrome/browser/protector/protector_service_factory.h"
#include "chrome/browser/protector/protector_service.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace protector {

namespace {

const char kBackupSignature[] = "backup._signature";
const char kNewHomePage[] = "http://example.com";

}

class ProtectedPrefsWatcherTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    prefs_watcher_ =
        ProtectorServiceFactory::GetForProfile(&profile_)->GetPrefsWatcher();
    prefs_ = profile_.GetPrefs();
  }

  bool IsSignatureValid() {
    return prefs_watcher_->IsSignatureValid();
  }

  bool HasBackup() {
    return prefs_watcher_->HasBackup();
  }

  void RevalidateBackup() {
    prefs_watcher_->ValidateBackup();
  }

  void ForceUpdateSignature(ProtectedPrefsWatcher* prefs_watcher) {
    prefs_watcher->UpdateBackupSignature();
  }

 protected:
  ProtectedPrefsWatcher* prefs_watcher_;
  TestingProfile profile_;
  PrefService* prefs_;
};

TEST_F(ProtectedPrefsWatcherTest, ValidOnCleanProfile) {
  EXPECT_TRUE(HasBackup());
  EXPECT_TRUE(prefs_watcher_->is_backup_valid());
}

TEST_F(ProtectedPrefsWatcherTest, ValidAfterPrefChange) {
  // Signature is still valid after a protected pref has been changed.
  base::StringValue new_homepage(kNewHomePage);
  EXPECT_NE(prefs_->GetString(prefs::kHomePage), kNewHomePage);
  EXPECT_FALSE(new_homepage.Equals(
      prefs_watcher_->GetBackupForPref(prefs::kHomePage)));

  prefs_->SetString(prefs::kHomePage, kNewHomePage);

  EXPECT_TRUE(HasBackup());
  EXPECT_TRUE(prefs_watcher_->is_backup_valid());
  EXPECT_EQ(prefs_->GetString(prefs::kHomePage), kNewHomePage);
  // Backup is updated accordingly.
  EXPECT_TRUE(new_homepage.Equals(
      prefs_watcher_->GetBackupForPref(prefs::kHomePage)));
}

TEST_F(ProtectedPrefsWatcherTest, InvalidSignature) {
  // Make backup invalid by changing one of its members directly.
  prefs_->SetString("backup.homepage", kNewHomePage);
  RevalidateBackup();
  EXPECT_TRUE(HasBackup());
  EXPECT_FALSE(prefs_watcher_->is_backup_valid());
  // No backup values available.
  EXPECT_FALSE(prefs_watcher_->GetBackupForPref(prefs::kHomePage));

  // Now change the corresponding protected prefernce: backup should be signed
  // again but still invalid.
  prefs_->SetString(prefs::kHomePage, kNewHomePage);
  EXPECT_TRUE(IsSignatureValid());
  EXPECT_FALSE(prefs_watcher_->is_backup_valid());
  EXPECT_FALSE(prefs_watcher_->GetBackupForPref(prefs::kHomePage));
}

TEST_F(ProtectedPrefsWatcherTest, ExtensionPrefChange) {
  // Changes to extensions data (but not to extension IDs) do not update
  // the backup and its signature.
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);

  FilePath extensions_install_dir =
      profile_.GetPath().AppendASCII(ExtensionService::kInstallDirectoryName);
  scoped_ptr<ExtensionPrefs> extension_prefs(
      new ExtensionPrefs(profile_.GetPrefs(),
                         extensions_install_dir,
                         ExtensionPrefValueMapFactory::GetForProfile(
                             &profile_)));
  std::string sample_id = extension_misc::kWebStoreAppId;
  extension_prefs->Init(false);
  // Flip a pref value of an extension (this will actually add it to the list).
  extension_prefs->SetAppNotificationDisabled(
      sample_id, !extension_prefs->IsAppNotificationDisabled(sample_id));

  // Backup is still valid.
  EXPECT_TRUE(prefs_watcher_->is_backup_valid());

  // Make signature invalid by changing it directly.
  prefs_->SetString(kBackupSignature, "INVALID");
  EXPECT_FALSE(IsSignatureValid());

  // Flip another pref value of that extension.
  extension_prefs->SetIsIncognitoEnabled(
      sample_id, !extension_prefs->IsIncognitoEnabled(sample_id));

  // No changes to the backup and signature.
  EXPECT_FALSE(IsSignatureValid());

  // Blacklisting the extension does update the backup and signature.
  std::set<std::string> blacklist;
  blacklist.insert(sample_id);
  extension_prefs->UpdateBlacklist(blacklist);

  EXPECT_TRUE(IsSignatureValid());
}

// Verify that version bigger than 1 is included in the signature.
TEST_F(ProtectedPrefsWatcherTest, VersionIsSigned) {
  // Reset version to 1.
  prefs_->ClearPref("backup._version");
  // This should make the backup invalid.
  EXPECT_FALSE(IsSignatureValid());

  // "Migrate" the backup back to the latest version.
  RevalidateBackup();

  EXPECT_FALSE(prefs_watcher_->is_backup_valid());
  EXPECT_EQ(ProtectedPrefsWatcher::kCurrentVersionNumber,
            prefs_->GetInteger("backup._version"));
}

// Verify that backup for "pinned_tabs" is added during version 2 migration.
TEST_F(ProtectedPrefsWatcherTest, MigrationToVersion2) {
  // Add a pinned tab.
  {
    ListPrefUpdate pinned_tabs_update(prefs_, prefs::kPinnedTabs);
    base::ListValue* pinned_tabs = pinned_tabs_update.Get();
    pinned_tabs->Clear();
    base::DictionaryValue* tab = new base::DictionaryValue;
    tab->SetString("url", "http://example.com/");
    pinned_tabs->Append(tab);
  }
  EXPECT_TRUE(prefs_watcher_->is_backup_valid());

  scoped_ptr<base::Value> pinned_tabs_copy(
      prefs_->GetList(prefs::kPinnedTabs)->DeepCopy());

  // Reset version to 1, remove "pinned_tabs" and overwrite the signature.
  // Store the old signature (without "pinned_tabs").
  prefs_->ClearPref("backup._version");
  prefs_->ClearPref("backup.pinned_tabs");
  ForceUpdateSignature(prefs_watcher_);
  EXPECT_TRUE(IsSignatureValid());

  // This will migrate backup to the latest version.
  RevalidateBackup();

  // Now the backup should be valid and "pinned_tabs" is added back.
  EXPECT_TRUE(prefs_watcher_->is_backup_valid());
  EXPECT_TRUE(pinned_tabs_copy->Equals(prefs_->GetList("backup.pinned_tabs")));
  EXPECT_TRUE(pinned_tabs_copy->Equals(prefs_->GetList(prefs::kPinnedTabs)));
  EXPECT_FALSE(prefs_watcher_->DidPrefChange(prefs::kPinnedTabs));
  EXPECT_EQ(ProtectedPrefsWatcher::kCurrentVersionNumber,
            prefs_->GetInteger("backup._version"));
}

// Verify that SessionStartupPref migration doesn't trigger Protector
// (version 3 migration).
TEST_F(ProtectedPrefsWatcherTest, MigrationToVersion3) {
  EXPECT_TRUE(prefs_watcher_->is_backup_valid());

  // Bring startup prefs to an old (pre-migration) version.
  prefs_->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  prefs_->SetString(prefs::kHomePage, kNewHomePage);
  prefs_->ClearPref(prefs::kRestoreOnStartupMigrated);

  // Reset version to 2 and overwrite the signature.
  prefs_->SetInteger("backup._version", 2);
  ForceUpdateSignature(prefs_watcher_);
  EXPECT_TRUE(IsSignatureValid());

  // Take down the old instance and create a new ProtectedPrefsWatcher from
  // current prefs.
  ProtectorServiceFactory::GetForProfile(&profile_)->
      StopWatchingPrefsForTesting();
  scoped_ptr<ProtectedPrefsWatcher> prefs_watcher2(
      new ProtectedPrefsWatcher(&profile_));
  EXPECT_TRUE(prefs_watcher2->is_backup_valid());

  // Startup settings shouldn't be reported as changed.
  EXPECT_FALSE(prefs_watcher2->DidPrefChange(prefs::kRestoreOnStartup));
  EXPECT_FALSE(prefs_watcher2->DidPrefChange(prefs::kURLsToRestoreOnStartup));
  EXPECT_EQ(ProtectedPrefsWatcher::kCurrentVersionNumber,
            prefs_->GetInteger("backup._version"));
}

// Verify that migration to version 4 removes backups with default values.
TEST_F(ProtectedPrefsWatcherTest, MigrationToVersion4) {
  EXPECT_TRUE(prefs_watcher_->is_backup_valid());

  prefs_->SetString(prefs::kHomePage, kNewHomePage);
  EXPECT_TRUE(prefs_->HasPrefPath("backup.homepage"));

  // Reset version to 3 and overwrite the signature.
  prefs_->SetInteger("backup._version", 3);
  ForceUpdateSignature(prefs_watcher_);
  EXPECT_TRUE(IsSignatureValid());

  ProtectorServiceFactory::GetForProfile(&profile_)->
      StopWatchingPrefsForTesting();

  // Restore |kHomePage| to default value.
  prefs_->ClearPref(prefs::kHomePage);

  scoped_ptr<ProtectedPrefsWatcher> prefs_watcher2(
      new ProtectedPrefsWatcher(&profile_));
  EXPECT_TRUE(prefs_watcher2->is_backup_valid());

  // Backup for |kHomePage| should now be restored to the default value, too.
  EXPECT_FALSE(prefs_->HasPrefPath("backup.homepage"));
  EXPECT_FALSE(prefs_watcher2->DidPrefChange(prefs::kHomePage));
  EXPECT_EQ(ProtectedPrefsWatcher::kCurrentVersionNumber,
            prefs_->GetInteger("backup._version"));
}

// Verify handling of default values of protected prefs.
TEST_F(ProtectedPrefsWatcherTest, DefaultValues) {
  EXPECT_TRUE(prefs_watcher_->is_backup_valid());

  EXPECT_FALSE(prefs_->HasPrefPath(prefs::kHomePage));
  EXPECT_FALSE(prefs_watcher_->DidPrefChange(prefs::kHomePage));
  prefs_->SetString(prefs::kHomePage, kNewHomePage);
  EXPECT_FALSE(prefs_watcher_->DidPrefChange(prefs::kHomePage));

  ProtectorServiceFactory::GetForProfile(&profile_)->
      StopWatchingPrefsForTesting();

  // Restore |kHomePage| to default value.
  prefs_->ClearPref(prefs::kHomePage);

  scoped_ptr<ProtectedPrefsWatcher> prefs_watcher2(
      new ProtectedPrefsWatcher(&profile_));
  EXPECT_TRUE(prefs_watcher2->is_backup_valid());
  EXPECT_TRUE(prefs_watcher2->DidPrefChange(prefs::kHomePage));

  prefs_->ClearPref("backup.homepage");
  ForceUpdateSignature(prefs_watcher2.get());

  EXPECT_TRUE(prefs_watcher2->is_backup_valid());
  EXPECT_FALSE(prefs_watcher2->DidPrefChange(prefs::kHomePage));
}

TEST_F(ProtectedPrefsWatcherTest, CheckPrefNames) {
  // If any of these preference names change, add corresponding migration code
  // to ProtectedPrefsWatcher.
  // DO NOT simply fix these literals!
  EXPECT_EQ("homepage", std::string(prefs::kHomePage));
  EXPECT_EQ("homepage_is_newtabpage",
            std::string(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ("browser.show_home_button", std::string(prefs::kShowHomeButton));
  EXPECT_EQ("session.restore_on_startup",
            std::string(prefs::kRestoreOnStartup));
  EXPECT_EQ("session.urls_to_restore_on_startup",
            std::string(prefs::kURLsToRestoreOnStartup));
  EXPECT_EQ("pinned_tabs", std::string(prefs::kPinnedTabs));
}

}  // namespace protector
