// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_app_list_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

using apps_helper::DisableApp;
using apps_helper::EnableApp;
using apps_helper::HasSameAppsAsVerifier;
using apps_helper::IncognitoDisableApp;
using apps_helper::IncognitoEnableApp;
using apps_helper::InstallApp;
using apps_helper::InstallAppsPendingForSync;
using apps_helper::UninstallApp;

namespace {

const size_t kNumDefaultApps = 2;

bool AllProfilesHaveSameAppListAsVerifier() {
  return SyncAppListHelper::GetInstance()->
      AllProfilesHaveSameAppListAsVerifier();
}

}  // namespace

class TwoClientAppListSyncTest : public SyncTest {
 public:
  TwoClientAppListSyncTest() : SyncTest(TWO_CLIENT) {}

  virtual ~TwoClientAppListSyncTest() {}

  // SyncTest
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableSyncAppList);
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
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion());
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
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion());
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
