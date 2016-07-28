// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_uma_util.h"

#include "base/test/scoped_command_line.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/browser_sync/common/browser_sync_switches.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync_driver/glue/sync_backend_host_mock.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kTestingGaiaId[] = "gaia_id";
constexpr char kTestingUsername[] = "fake_username";
}  // namespace

class PermissionUmaUtilTest : public testing::Test {
 protected:
  PermissionUmaUtilTest() {}

  static bool IsOptedIntoPermissionActionReporting(Profile* profile) {
    return PermissionUmaUtil::IsOptedIntoPermissionActionReporting(profile);
  }

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              BuildFakeSigninManagerBase);
    profile_ = builder.Build();
  }

  void FakeSignIn() {
    SigninManagerBase* signin_manager =
        static_cast<FakeSigninManagerForTesting*>(
            SigninManagerFactory::GetForProfile(profile()));
    signin_manager->SetAuthenticatedAccountInfo(kTestingGaiaId,
                                                kTestingUsername);
  }

  void SetSafeBrowsing(bool enabled) {
    PrefService* preferences = profile_->GetPrefs();
    preferences->SetBoolean(prefs::kSafeBrowsingEnabled, enabled);
  }

  ProfileSyncService* GetProfileSyncService() {
    return ProfileSyncServiceFactory::GetForProfile(profile());
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
};

// Test that PermissionUmaUtil::IsOptedIntoPermissionActionReporting returns
// true if Safe Browsing is enabled, Permission Action Reporting flag is
// enabled, not in incognito mode and signed in with default sync preferences.
TEST_F(PermissionUmaUtilTest, IsOptedIntoPermissionActionReportingSignInCheck) {
  base::test::ScopedCommandLine scoped_command_line;
  SetSafeBrowsing(true);
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnablePermissionActionReporting);
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));

  FakeSignIn();
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(
      profile()->GetOffTheRecordProfile()));
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));
}

// Test that PermissionUmaUtil::IsOptedIntoPermissionActionReporting returns
// false if Permission Action Reporting is not enabled.
TEST_F(PermissionUmaUtilTest, IsOptedIntoPermissionActionReportingFlagCheck) {
  SetSafeBrowsing(true);
  FakeSignIn();
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kEnablePermissionActionReporting);
    EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));
  }  // Reset the command line.

  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisablePermissionActionReporting);
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));
}

// Test that PermissionUmaUtil::IsOptedIntoPermissionActionReporting returns
// false if Safe Browsing is disabled.
TEST_F(PermissionUmaUtilTest,
       IsOptedIntoPermissionActionReportingSafeBrowsingCheck) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnablePermissionActionReporting);
  FakeSignIn();
  SetSafeBrowsing(true);
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));

  SetSafeBrowsing(false);
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));
}

// Test that PermissionUmaUtil::IsOptedIntoPermissionActionReporting returns
// false if Sync is disabled.
TEST_F(PermissionUmaUtilTest,
       IsOptedIntoPermissionActionReportingProfileSyncServiceCheck) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnablePermissionActionReporting);
  SetSafeBrowsing(true);
  FakeSignIn();
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));

  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisableSync);
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));
}

// Test that PermissionUmaUtil::IsOptedIntoPermissionActionReporting returns
// false if Tab Sync and Pref Sync are not both enabled.
TEST_F(PermissionUmaUtilTest,
       IsOptedIntoPermissionActionReportingSyncPreferenceCheck) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnablePermissionActionReporting);
  SetSafeBrowsing(true);
  FakeSignIn();
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));

  sync_driver::SyncPrefs sync_prefs(profile()->GetPrefs());
  const syncer::ModelTypeSet registered_types =
      GetProfileSyncService()->GetRegisteredDataTypes();
  sync_prefs.SetKeepEverythingSynced(false);

  sync_prefs.SetPreferredDataTypes(
      registered_types, syncer::ModelTypeSet(syncer::PROXY_TABS));
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));

  sync_prefs.SetPreferredDataTypes(
      registered_types, syncer::ModelTypeSet(syncer::PRIORITY_PREFERENCES));
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));

  sync_prefs.SetPreferredDataTypes(
      registered_types,
      syncer::ModelTypeSet(syncer::PROXY_TABS, syncer::PRIORITY_PREFERENCES));
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));
}
