// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_switches.h"

class SingleClientSupervisedUserSettingsSyncTest : public SyncTest {
 public:
  SingleClientSupervisedUserSettingsSyncTest() : SyncTest(SINGLE_CLIENT) {}

  virtual ~SingleClientSupervisedUserSettingsSyncTest() {}

  // SyncTest overrides:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kSupervisedUserId, "asdf");
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientSupervisedUserSettingsSyncTest, Sanity) {
  ASSERT_TRUE(SetupClients());
  for (int i = 0; i < num_clients(); ++i) {
    Profile* profile = GetProfile(i);
    // Supervised users are prohibited from signing into the browser. Currently
    // that means they're also unable to sync anything, so override that for
    // this test.
    // TODO(pamg): Remove this override (and the supervised user setting it
    // requires) once sync and signin are properly separated for supervised
    // users.
    // See http://crbug.com/239785.
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForProfile(profile);
    scoped_ptr<base::Value> allow_signin(new base::FundamentalValue(true));
    settings_service->SetLocalSettingForTesting(
        supervised_users::kSigninAllowed,
        allow_signin.Pass());

    // The user should not be signed in.
    std::string username;
    // ProfileSyncServiceHarness sets the password, which can't be empty.
    std::string password = "password";
    GetClient(i)->SetCredentials(username, password);
  }
  ASSERT_TRUE(SetupSync());
}
