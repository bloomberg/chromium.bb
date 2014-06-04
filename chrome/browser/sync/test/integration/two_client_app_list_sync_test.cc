// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_app_list_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"

using apps_helper::DisableApp;
using apps_helper::EnableApp;
using apps_helper::HasSameAppsAsVerifier;
using apps_helper::IncognitoDisableApp;
using apps_helper::IncognitoEnableApp;
using apps_helper::InstallApp;
using apps_helper::InstallAppsPendingForSync;
using apps_helper::UninstallApp;
using sync_integration_test_util::AwaitCommitActivityCompletion;

namespace {

const size_t kNumDefaultApps = 2;

bool AllProfilesHaveSameAppListAsVerifier() {
  return SyncAppListHelper::GetInstance()->
      AllProfilesHaveSameAppListAsVerifier();
}

const app_list::AppListSyncableService::SyncItem* GetSyncItem(
    Profile* profile,
    const std::string& app_id) {
  app_list::AppListSyncableService* service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  return service->GetSyncItem(app_id);
}

}  // namespace

class TwoClientAppListSyncTest : public SyncTest {
 public:
  TwoClientAppListSyncTest() : SyncTest(TWO_CLIENT_LEGACY) {}

  virtual ~TwoClientAppListSyncTest() {}

  // SyncTest
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(app_list::switches::kEnableSyncAppList);
  }

  virtual bool SetupClients() OVERRIDE {
    if (!SyncTest::SetupClients())
      return false;

    // Init SyncAppListHelper to ensure that the extension system is initialized
    // for each Profile.
    SyncAppListHelper::GetInstance();
    return true;
  }

  virtual bool SetupSync() OVERRIDE {
    if (!SyncTest::SetupSync())
      return false;
    WaitForExtensionServicesToLoad();
    return true;
  }

 private:
  void WaitForExtensionServicesToLoad() {
    for (int i = 0; i < num_clients(); ++i)
      WaitForExtensionsServiceToLoadForProfile(GetProfile(i));
    WaitForExtensionsServiceToLoadForProfile(verifier());
  }

  void WaitForExtensionsServiceToLoadForProfile(Profile* profile) {
    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    if (extension_service && extension_service->is_ready())
      return;
    content::WindowedNotificationObserver extensions_loaded_observer(
        chrome::NOTIFICATION_EXTENSIONS_READY,
        content::NotificationService::AllSources());
    extensions_loaded_observer.Wait();
  }

  DISALLOW_COPY_AND_ASSIGN(TwoClientAppListSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, StartWithNoApps) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, StartWithSameApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

// Install some apps on both clients, some on only one client, some on only the
// other, and sync.  Both clients should end up with all apps, and the app and
// page ordinals should be identical.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, StartWithDifferentApps) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonApps = 5;
  for (int j = 0; j < kNumCommonApps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
  }

  const int kNumProfile0Apps = 10;
  for (int j = 0; j < kNumProfile0Apps; ++i, ++j) {
    std::string id = InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
    SyncAppListHelper::GetInstance()->CopyOrdinalsToVerifier(GetProfile(0), id);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    std::string id = InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
    SyncAppListHelper::GetInstance()->CopyOrdinalsToVerifier(GetProfile(1), id);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));

  // Verify the app lists, but ignore absolute position values, checking only
  // relative positions (see note in app_list_syncable_service.h).
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

// Install some apps on both clients, then sync.  Then install some apps on only
// one client, some on only the other, and then sync again.  Both clients should
// end up with all apps, and the app and page ordinals should be identical.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, InstallDifferentApps) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonApps = 5;
  for (int j = 0; j < kNumCommonApps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  const int kNumProfile0Apps = 10;
  for (int j = 0; j < kNumProfile0Apps; ++i, ++j) {
    std::string id = InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
    SyncAppListHelper::GetInstance()->CopyOrdinalsToVerifier(GetProfile(0), id);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    std::string id = InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
    SyncAppListHelper::GetInstance()->CopyOrdinalsToVerifier(GetProfile(1), id);
  }

  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));

  // Verify the app lists, but ignore absolute position values, checking only
  // relative positions (see note in app_list_syncable_service.h).
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, Install) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, Uninstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  UninstallApp(GetProfile(0), 0);
  UninstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

// Install an app on one client, then sync. Then uninstall the app on the first
// client and sync again. Now install a new app on the first client and sync.
// Both client should only have the second app, with identical app and page
// ordinals.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, UninstallThenInstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  UninstallApp(GetProfile(0), 0);
  UninstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  InstallApp(GetProfile(0), 1);
  InstallApp(verifier(), 1);
  ASSERT_TRUE(AwaitQuiescence());
  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, Merge) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(1), 0);
  ASSERT_TRUE(AwaitQuiescence());

  UninstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(0), 1);
  InstallApp(verifier(), 1);

  InstallApp(GetProfile(0), 2);
  InstallApp(GetProfile(1), 2);
  InstallApp(verifier(), 2);

  InstallApp(GetProfile(1), 3);
  InstallApp(verifier(), 3);

  ASSERT_TRUE(AwaitQuiescence());
  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, UpdateEnableDisableApp) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(1), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  DisableApp(GetProfile(0), 0);
  DisableApp(verifier(), 0);
  ASSERT_TRUE(HasSameAppsAsVerifier(0));
  ASSERT_FALSE(HasSameAppsAsVerifier(1));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  EnableApp(GetProfile(1), 0);
  EnableApp(verifier(), 0);
  ASSERT_TRUE(HasSameAppsAsVerifier(1));
  ASSERT_FALSE(HasSameAppsAsVerifier(0));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, UpdateIncognitoEnableDisable) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(1), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  IncognitoEnableApp(GetProfile(0), 0);
  IncognitoEnableApp(verifier(), 0);
  ASSERT_TRUE(HasSameAppsAsVerifier(0));
  ASSERT_FALSE(HasSameAppsAsVerifier(1));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  IncognitoDisableApp(GetProfile(1), 0);
  IncognitoDisableApp(verifier(), 0);
  ASSERT_TRUE(HasSameAppsAsVerifier(1));
  ASSERT_FALSE(HasSameAppsAsVerifier(0));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, DisableApps) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncer::APP_LIST));
  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(HasSameAppsAsVerifier(0));
  ASSERT_FALSE(HasSameAppsAsVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncer::APP_LIST));
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

// Disable sync for the second client and then install an app on the first
// client, then enable sync on the second client. Both clients should have the
// same app with identical app and page ordinals.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(HasSameAppsAsVerifier(0));
  ASSERT_FALSE(HasSameAppsAsVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

// Install some apps on both clients, then sync. Move an app on one client
// and sync. Both clients should have the updated position for the app.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, Move) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  size_t first = kNumDefaultApps;
  SyncAppListHelper::GetInstance()->MoveApp(
      GetProfile(0), first + 1, first + 2);
  SyncAppListHelper::GetInstance()->MoveApp(
      verifier(), first + 1, first + 2);

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

// Install a Default App on both clients, then sync. Remove the app on one
// client and sync. Ensure that the app is removed on the other client and
// that a REMOVE_DEFAULT_APP entry exists.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, RemoveDefault) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(SetupSync());

  // Install a non-default app.
  InstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(1), 0);
  InstallApp(verifier(), 0);

  // Install a default app in Profile 0 only.
  const int default_app_index = 1;
  std::string default_app_id = InstallApp(GetProfile(0), default_app_index);
  InstallApp(verifier(), default_app_index);
  SyncAppListHelper::GetInstance()->CopyOrdinalsToVerifier(
      GetProfile(0), default_app_id);

  ASSERT_TRUE(AwaitQuiescence());
  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  // Flag Default app in Profile 1.
  extensions::ExtensionPrefs::Get(GetProfile(1))
      ->UpdateExtensionPref(default_app_id,
                            "was_installed_by_default",
                            new base::FundamentalValue(true));

  // Remove the default app in Profile 0 and verifier, ensure it was removed
  // in Profile 1.
  UninstallApp(GetProfile(0), default_app_index);
  UninstallApp(verifier(), default_app_index);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  // Ensure that a REMOVE_DEFAULT_APP SyncItem entry exists in Profile 1.
  const app_list::AppListSyncableService::SyncItem* sync_item =
      GetSyncItem(GetProfile(1), default_app_id);
  ASSERT_TRUE(sync_item);
  ASSERT_EQ(sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP,
            sync_item->item_type);

  // Re-Install the same app in Profile 0.
  std::string app_id2 = InstallApp(GetProfile(0), default_app_index);
  EXPECT_EQ(default_app_id, app_id2);
  InstallApp(verifier(), default_app_index);
  sync_item = GetSyncItem(GetProfile(0), app_id2);
  EXPECT_EQ(sync_pb::AppListSpecifics::TYPE_APP, sync_item->item_type);

  ASSERT_TRUE(AwaitQuiescence());
  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  // Ensure that the REMOVE_DEFAULT_APP SyncItem entry in Profile 1 is replaced
  // with an APP entry after an install.
  sync_item = GetSyncItem(GetProfile(1), app_id2);
  ASSERT_TRUE(sync_item);
  EXPECT_EQ(sync_pb::AppListSpecifics::TYPE_APP, sync_item->item_type);
}

#if !defined(OS_MACOSX)

class TwoClientAppListSyncFolderTest : public TwoClientAppListSyncTest {
 public:
  TwoClientAppListSyncFolderTest() {}
  virtual ~TwoClientAppListSyncFolderTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    TwoClientAppListSyncTest::SetUpCommandLine(command_line);
  }

  virtual bool SetupClients() OVERRIDE {
    bool res = TwoClientAppListSyncTest::SetupClients();
    app_list::AppListSyncableService* verifier_service =
        app_list::AppListSyncableServiceFactory::GetForProfile(verifier());
    verifier_service->model()->SetFoldersEnabled(true);
    return res;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientAppListSyncFolderTest);
};

// Install some apps on both clients, then sync. Move an app on one client
// to a folder and sync. The app lists, including folders, should match.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncFolderTest, MoveToFolder) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  size_t index = 2u;
  std::string folder_id = "Folder 0";
  SyncAppListHelper::GetInstance()->MoveAppToFolder(
      GetProfile(0), index, folder_id);
  SyncAppListHelper::GetInstance()->MoveAppToFolder(
      verifier(), index, folder_id);

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncFolderTest, FolderAddRemove) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  const int kNumApps = 10;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  // Move a few apps to a folder.
  const size_t kNumAppsToMove = 3;
  std::string folder_id = "Folder 0";
  // The folder will be created at the end of the list; always move the
  // first non default item in the list.
  size_t item_index = kNumDefaultApps;
  for (size_t i = 0; i < kNumAppsToMove; ++i) {
    SyncAppListHelper::GetInstance()->MoveAppToFolder(
        GetProfile(0), item_index, folder_id);
    SyncAppListHelper::GetInstance()->MoveAppToFolder(
        verifier(), item_index, folder_id);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  // Remove one app from the folder.
  SyncAppListHelper::GetInstance()->MoveAppFromFolder(
      GetProfile(0), 0, folder_id);
  SyncAppListHelper::GetInstance()->MoveAppFromFolder(
      verifier(), 0, folder_id);

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  // Remove remaining apps from the folder (deletes folder).
  for (size_t i = 1; i < kNumAppsToMove; ++i) {
    SyncAppListHelper::GetInstance()->MoveAppFromFolder(
        GetProfile(0), 0, folder_id);
    SyncAppListHelper::GetInstance()->MoveAppFromFolder(
        verifier(), 0, folder_id);
  }

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

  // Move apps back to a (new) folder.
  for (size_t i = 0; i < kNumAppsToMove; ++i) {
    SyncAppListHelper::GetInstance()->MoveAppToFolder(
        GetProfile(0), item_index, folder_id);
    SyncAppListHelper::GetInstance()->MoveAppToFolder(
        verifier(), item_index, folder_id);
  }

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

#endif  // !defined(OS_MACOSX)
