// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_service.h"

class SingleClientSupervisedUserSettingsSyncTest : public SyncTest {
 public:
  SingleClientSupervisedUserSettingsSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientSupervisedUserSettingsSyncTest() override {}

  // SyncTest overrides:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kSupervisedUserId, "asdf");
  }
};

void TestAuthErrorCallback(const GoogleServiceAuthError& error) {}

IN_PROC_BROWSER_TEST_F(SingleClientSupervisedUserSettingsSyncTest, Sanity) {
  ASSERT_TRUE(SetupClients());

  EXPECT_EQ(num_clients(), 1);
  Profile* profile = GetProfile(0);

  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);

  supervised_user_service->Init();

  // This call triggers a separate, supervised user-specific codepath
  // that does not normally execute for sync.
  supervised_user_service->OnSupervisedUserRegistered(
      base::Bind(&TestAuthErrorCallback),
      // Use the Browser's main profile as the custodian.
      browser()->profile(),
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      std::string("token value doesn't matter in tests"));

  ASSERT_TRUE(GetClient(0)->AwaitBackendInitialization());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // TODO(pvalenzuela): Add additional tests and some verification of sync-
  // specific features (e.g., assert certain datatypes are syncing).
}
