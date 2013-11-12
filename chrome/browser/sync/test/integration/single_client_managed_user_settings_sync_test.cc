// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_user_constants.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/managed_mode/managed_user_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_settings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_switches.h"

class SingleClientManagedUserSettingsSyncTest : public SyncTest {
 public:
  SingleClientManagedUserSettingsSyncTest() : SyncTest(SINGLE_CLIENT) {}

  virtual ~SingleClientManagedUserSettingsSyncTest() {}

  // SyncTest overrides:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kManagedUserId, "asdf");
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientManagedUserSettingsSyncTest, Sanity) {
  ASSERT_TRUE(SetupClients());
  for (int i = 0; i < num_clients(); ++i) {
    Profile* profile = GetProfile(i);
    // Managed users are prohibited from signing into the browser. Currently
    // that means they're also unable to sync anything, so override that for
    // this test.
    // TODO(pamg): Remove this override (and the managed user setting it
    // requires) once sync and signin are properly separated for managed users.
    // See http://crbug.com/239785.
    ManagedUserSettingsService* settings_service =
        ManagedUserSettingsServiceFactory::GetForProfile(profile);
    scoped_ptr<base::Value> allow_signin(new base::FundamentalValue(true));
    settings_service->SetLocalSettingForTesting(managed_users::kSigninAllowed,
                                                allow_signin.Pass());

    // The user should not be signed in.
    std::string username;
    // ProfileSyncServiceHarness sets the password, which can't be empty.
    std::string password = "password";
    GetClient(i)->SetCredentials(username, password);
  }
  ASSERT_TRUE(SetupSync());
}
