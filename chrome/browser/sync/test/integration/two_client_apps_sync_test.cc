// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/app_sync_data.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_app_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "sync/api/string_ordinal.h"

using apps_helper::AllProfilesHaveSameAppsAsVerifier;
using apps_helper::CopyNTPOrdinals;
using apps_helper::DisableApp;
using apps_helper::EnableApp;
using apps_helper::FixNTPOrdinalCollisions;
using apps_helper::GetAppLaunchOrdinalForApp;
using apps_helper::HasSameAppsAsVerifier;
using apps_helper::IncognitoDisableApp;
using apps_helper::IncognitoEnableApp;
using apps_helper::InstallApp;
using apps_helper::InstallAppsPendingForSync;
using apps_helper::InstallPlatformApp;
using apps_helper::SetAppLaunchOrdinalForApp;
using apps_helper::SetPageOrdinalForApp;
using apps_helper::UninstallApp;
using sync_integration_test_util::AwaitCommitActivityCompletion;

namespace {

extensions::ExtensionRegistry* GetExtensionRegistry(Profile* profile) {
  return extensions::ExtensionRegistry::Get(profile);
}

ExtensionService* GetExtensionService(Profile* profile) {
  return extensions::ExtensionSystem::Get(profile)->extension_service();
}

}  // namespace

class TwoClientAppsSyncTest : public SyncTest {
 public:
  TwoClientAppsSyncTest() : SyncTest(TWO_CLIENT) {}

  virtual ~TwoClientAppsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientAppsSyncTest);
};

class LegacyTwoClientAppsSyncTest : public SyncTest {
 public:
  LegacyTwoClientAppsSyncTest() : SyncTest(TWO_CLIENT_LEGACY) {}

  virtual ~LegacyTwoClientAppsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LegacyTwoClientAppsSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, StartWithNoApps) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, StartWithSameApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// Install some apps on both clients, some on only one client, some on only the
// other, and sync.  Both clients should end up with all apps, and the app and
// page ordinals should be identical.
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, StartWithDifferentApps) {
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
    InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
    CopyNTPOrdinals(GetProfile(0), verifier(), i);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
    CopyNTPOrdinals(GetProfile(1), verifier(), i);
  }

  const int kNumPlatformApps = 5;
  for (int j = 0; j < kNumPlatformApps; ++i, ++j) {
    InstallPlatformApp(GetProfile(1), i);
    InstallPlatformApp(verifier(), i);
    CopyNTPOrdinals(GetProfile(1), verifier(), i);
  }

  FixNTPOrdinalCollisions(verifier());

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));

  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// Install some apps on both clients, then sync.  Then install some apps on only
// one client, some on only the other, and then sync again.  Both clients should
// end up with all apps, and the app and page ordinals should be identical.
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, InstallDifferentApps) {
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
    InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
    CopyNTPOrdinals(GetProfile(0), verifier(), i);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    InstallApp(GetProfile(1), i);
    InstallApp(verifier(), i);
    CopyNTPOrdinals(GetProfile(1), verifier(), i);
  }

  FixNTPOrdinalCollisions(verifier());

  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));

  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// TCM ID - 3711279.
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, Add) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// TCM ID - 3706267.
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, Uninstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  UninstallApp(GetProfile(0), 0);
  UninstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// Install an app on one client, then sync. Then uninstall the app on the first
// client and sync again. Now install a new app on the first client and sync.
// Both client should only have the second app, with identical app and page
// ordinals.
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, UninstallThenInstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  UninstallApp(GetProfile(0), 0);
  UninstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  InstallApp(GetProfile(0), 1);
  InstallApp(verifier(), 1);
  ASSERT_TRUE(AwaitQuiescence());
  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// TCM ID - 3699295.
// Flaky: http://crbug.com/226055
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, DISABLED_Merge) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

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
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// TCM ID - 7723126.
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, UpdateEnableDisableApp) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(1), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  DisableApp(GetProfile(0), 0);
  DisableApp(verifier(), 0);
  ASSERT_TRUE(HasSameAppsAsVerifier(0));
  ASSERT_FALSE(HasSameAppsAsVerifier(1));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  EnableApp(GetProfile(1), 0);
  EnableApp(verifier(), 0);
  ASSERT_TRUE(HasSameAppsAsVerifier(1));
  ASSERT_FALSE(HasSameAppsAsVerifier(0));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// TCM ID - 7706637.
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, UpdateIncognitoEnableDisable) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(1), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  IncognitoEnableApp(GetProfile(0), 0);
  IncognitoEnableApp(verifier(), 0);
  ASSERT_TRUE(HasSameAppsAsVerifier(0));
  ASSERT_FALSE(HasSameAppsAsVerifier(1));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  IncognitoDisableApp(GetProfile(1), 0);
  IncognitoDisableApp(verifier(), 0);
  ASSERT_TRUE(HasSameAppsAsVerifier(1));
  ASSERT_FALSE(HasSameAppsAsVerifier(0));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// TCM ID - 3718276.
IN_PROC_BROWSER_TEST_F(LegacyTwoClientAppsSyncTest, DisableApps) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncer::APPS));
  InstallApp(GetProfile(0), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(HasSameAppsAsVerifier(0));
  ASSERT_FALSE(HasSameAppsAsVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncer::APPS));
  ASSERT_TRUE(AwaitQuiescence());

  InstallAppsPendingForSync(GetProfile(0));
  InstallAppsPendingForSync(GetProfile(1));
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// Disable sync for the second client and then install an app on the first
// client, then enable sync on the second client. Both clients should have the
// same app with identical app and page ordinals.
// TCM ID - 3720303.
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

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
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// Install the same app on both clients, then sync. Change the page ordinal on
// one client and sync. Both clients should have the updated page ordinal for
// the app.
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, UpdatePageOrdinal) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  syncer::StringOrdinal initial_page =
      syncer::StringOrdinal::CreateInitialOrdinal();
  InstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(1), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  syncer::StringOrdinal second_page = initial_page.CreateAfter();
  SetPageOrdinalForApp(GetProfile(0), 0, second_page);
  SetPageOrdinalForApp(verifier(), 0, second_page);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// Install the same app on both clients, then sync. Change the app launch
// ordinal on one client and sync. Both clients should have the updated app
// launch ordinal for the app.
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, UpdateAppLaunchOrdinal) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  InstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(1), 0);
  InstallApp(verifier(), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  syncer::StringOrdinal initial_position =
      GetAppLaunchOrdinalForApp(GetProfile(0), 0);

  syncer::StringOrdinal second_position = initial_position.CreateAfter();
  SetAppLaunchOrdinalForApp(GetProfile(0), 0, second_position);
  SetAppLaunchOrdinalForApp(verifier(), 0, second_position);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// Adjust the CWS location within a page on the first client and sync. Adjust
// which page the CWS appears on and sync. Both clients should have the same
// page and app launch ordinal values for the CWS.
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, UpdateCWSOrdinals) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  // Change the app launch ordinal.
  syncer::StringOrdinal cws_app_launch_ordinal =
      extensions::ExtensionPrefs::Get(GetProfile(0))
          ->app_sorting()
          ->GetAppLaunchOrdinal(extension_misc::kWebStoreAppId);
  extensions::ExtensionPrefs::Get(GetProfile(0))
      ->app_sorting()
      ->SetAppLaunchOrdinal(extension_misc::kWebStoreAppId,
                            cws_app_launch_ordinal.CreateAfter());
  extensions::ExtensionPrefs::Get(verifier())
      ->app_sorting()
      ->SetAppLaunchOrdinal(extension_misc::kWebStoreAppId,
                            cws_app_launch_ordinal.CreateAfter());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  // Change the page ordinal.
  syncer::StringOrdinal cws_page_ordinal =
      extensions::ExtensionPrefs::Get(GetProfile(1))
          ->app_sorting()
          ->GetPageOrdinal(extension_misc::kWebStoreAppId);
  extensions::ExtensionPrefs::Get(GetProfile(1))->app_sorting()->SetPageOrdinal(
      extension_misc::kWebStoreAppId, cws_page_ordinal.CreateAfter());
  extensions::ExtensionPrefs::Get(verifier())->app_sorting()->SetPageOrdinal(
      extension_misc::kWebStoreAppId, cws_page_ordinal.CreateAfter());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

// Adjust the launch type on the first client and sync. Both clients should
// have the same launch type values for the CWS.
IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, UpdateLaunchType) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  // Change the launch type to window.
  extensions::SetLaunchType(GetExtensionService(GetProfile(1)),
                            extension_misc::kWebStoreAppId,
                            extensions::LAUNCH_TYPE_WINDOW);
  extensions::SetLaunchType(GetExtensionService(verifier()),
                            extension_misc::kWebStoreAppId,
                            extensions::LAUNCH_TYPE_WINDOW);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  // Change the launch type to regular tab.
  extensions::SetLaunchType(GetExtensionService(GetProfile(1)),
                            extension_misc::kWebStoreAppId,
                            extensions::LAUNCH_TYPE_REGULAR);
  ASSERT_FALSE(HasSameAppsAsVerifier(1));
  extensions::SetLaunchType(GetExtensionService(verifier()),
                            extension_misc::kWebStoreAppId,
                            extensions::LAUNCH_TYPE_REGULAR);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, UnexpectedLaunchType) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  extensions::SetLaunchType(GetExtensionService(GetProfile(1)),
                            extension_misc::kWebStoreAppId,
                            extensions::LAUNCH_TYPE_REGULAR);
  extensions::SetLaunchType(GetExtensionService(verifier()),
                            extension_misc::kWebStoreAppId,
                            extensions::LAUNCH_TYPE_REGULAR);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  const extensions::Extension* extension =
      GetExtensionRegistry(GetProfile(1))->GetExtensionById(
          extension_misc::kWebStoreAppId,
          extensions::ExtensionRegistry::EVERYTHING);
  ASSERT_TRUE(extension);

  ExtensionSyncService* extension_sync_service =
      ExtensionSyncService::Get(GetProfile(1));

  extensions::AppSyncData original_data(
      extension_sync_service->GetAppSyncData(*extension));

  // Create an invalid launch type and ensure it doesn't get down-synced. This
  // simulates the case of a future launch type being added which old versions
  // don't yet understand.
  extensions::AppSyncData invalid_launch_type_data(
      *extension,
      original_data.extension_sync_data().enabled(),
      original_data.extension_sync_data().incognito_enabled(),
      original_data.extension_sync_data().remote_install(),
      original_data.app_launch_ordinal(),
      original_data.page_ordinal(),
      extensions::NUM_LAUNCH_TYPES);
  extension_sync_service->ProcessAppSyncData(invalid_launch_type_data);

  // The launch type should remain the same.
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppsSyncTest, BookmarkApp) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());

  size_t num_extensions =
      GetExtensionRegistry(GetProfile(0))->enabled_extensions().size();

  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL("http://www.chromium.org");
  web_app_info.title = base::UTF8ToUTF16("Test name");
  web_app_info.description = base::UTF8ToUTF16("Test description");
  ++num_extensions;
  {
    content::WindowedNotificationObserver windowed_observer(
        chrome::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    extensions::CreateOrUpdateBookmarkApp(GetExtensionService(GetProfile(0)),
                                          web_app_info);
    windowed_observer.Wait();
    EXPECT_EQ(num_extensions,
              GetExtensionRegistry(GetProfile(0))->enabled_extensions().size());
  }
  {
    content::WindowedNotificationObserver windowed_observer(
        chrome::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    extensions::CreateOrUpdateBookmarkApp(GetExtensionService(verifier()),
                                          web_app_info);
    windowed_observer.Wait();
    EXPECT_EQ(num_extensions,
              GetExtensionRegistry(verifier())->enabled_extensions().size());
  }
  {
    // Wait for the synced app to install.
    content::WindowedNotificationObserver windowed_observer(
        chrome::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    ASSERT_TRUE(AwaitQuiescence());
    windowed_observer.Wait();
    ASSERT_TRUE(AllProfilesHaveSameAppsAsVerifier());
  }
}
// TODO(akalin): Add tests exercising:
//   - Offline installation/uninstallation behavior
//   - App-specific properties
