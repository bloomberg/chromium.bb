// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_uma_util.h"

#include <memory>

#include "base/test/scoped_command_line.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/engine/fake_sync_engine.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::ProfileSyncServiceMock;

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

  browser_sync::ProfileSyncService* GetProfileSyncService() {
    return ProfileSyncServiceFactory::GetForProfile(profile());
  }

  ProfileSyncServiceMock* SetMockSyncService() {
    ProfileSyncServiceMock* mock_sync_service =
        static_cast<ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
                profile(), BuildMockProfileSyncService));
    EXPECT_CALL(*mock_sync_service, CanSyncStart())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_sync_service, IsSyncActive())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
        .WillRepeatedly(testing::Return(false));

    syncer::ModelTypeSet preferred_types;
    preferred_types.Put(syncer::PROXY_TABS);
    preferred_types.Put(syncer::PRIORITY_PREFERENCES);
    EXPECT_CALL(*mock_sync_service, GetPreferredDataTypes())
        .WillRepeatedly(testing::Return(preferred_types));

    return mock_sync_service;
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
  SetSafeBrowsing(true);
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));

  FakeSignIn();
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));

  SetMockSyncService();
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(
      profile()->GetOffTheRecordProfile()));
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));
}

// Test that PermissionUmaUtil::IsOptedIntoPermissionActionReporting returns
// false if Permission Action Reporting is not enabled.
TEST_F(PermissionUmaUtilTest, IsOptedIntoPermissionActionReportingFlagCheck) {
  SetSafeBrowsing(true);
  FakeSignIn();
  SetMockSyncService();
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kDisablePermissionActionReporting);
    EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));
  }  // Reset the command line.

  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnablePermissionActionReporting);
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));
}

// Test that PermissionUmaUtil::IsOptedIntoPermissionActionReporting returns
// false if Safe Browsing is disabled.
TEST_F(PermissionUmaUtilTest,
       IsOptedIntoPermissionActionReportingSafeBrowsingCheck) {
  FakeSignIn();
  SetMockSyncService();
  SetSafeBrowsing(true);
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));

  SetSafeBrowsing(false);
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));
}

// Test that PermissionUmaUtil::IsOptedIntoPermissionActionReporting returns
// false if Sync is disabled.
TEST_F(PermissionUmaUtilTest,
       IsOptedIntoPermissionActionReportingProfileSyncServiceCheck) {
  SetSafeBrowsing(true);
  FakeSignIn();
  ProfileSyncServiceMock* mock_sync_service = SetMockSyncService();
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));

  EXPECT_CALL(*mock_sync_service, CanSyncStart())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));
}

// Test that PermissionUmaUtil::IsOptedIntoPermissionActionReporting returns
// false if Tab Sync and Pref Sync are not both enabled.
TEST_F(PermissionUmaUtilTest,
       IsOptedIntoPermissionActionReportingSyncPreferenceCheck) {
  SetSafeBrowsing(true);
  FakeSignIn();
  ProfileSyncServiceMock* mock_sync_service = SetMockSyncService();
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));

  // Reset the preferred types to return an empty set. The opted-in method will
  // return false because it needs both Tab and Pref Sync in the preferred types
  // in order to succeed.
  syncer::ModelTypeSet preferred_types;
  EXPECT_CALL(*mock_sync_service, GetPreferredDataTypes())
      .WillOnce(testing::Return(preferred_types));
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));

  // The opted-in method will be false with only Tab Sync on.
  preferred_types.Put(syncer::PROXY_TABS);
  EXPECT_CALL(*mock_sync_service, GetPreferredDataTypes())
      .WillOnce(testing::Return(preferred_types));
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));

  // The opted-in method will be false with only Pref Sync on.
  preferred_types.Remove(syncer::PROXY_TABS);
  preferred_types.Put(syncer::PRIORITY_PREFERENCES);
  EXPECT_CALL(*mock_sync_service, GetPreferredDataTypes())
      .WillOnce(testing::Return(preferred_types));
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));
}

// Test that PermissionUmaUtil::IsOptedIntoPermissionActionReporting returns
// false if Sync is not active.
TEST_F(PermissionUmaUtilTest,
       IsOptedIntoPermissionActionReportingProfileSyncServiceActiveCheck) {
  SetSafeBrowsing(true);
  FakeSignIn();
  ProfileSyncServiceMock* mock_sync_service = SetMockSyncService();
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));

  EXPECT_CALL(*mock_sync_service, IsSyncActive())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));
}

// Test that PermissionUmaUtil::IsOptedIntoPermissionActionReporting returns
// false if a custom Sync passphrase is set.
TEST_F(PermissionUmaUtilTest,
       IsOptedIntoPermissionActionReportingSyncPassphraseCheck) {
  SetSafeBrowsing(true);
  FakeSignIn();
  ProfileSyncServiceMock* mock_sync_service = SetMockSyncService();
  EXPECT_TRUE(IsOptedIntoPermissionActionReporting(profile()));

  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(IsOptedIntoPermissionActionReporting(profile()));
}
