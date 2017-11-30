// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/profile_management_switches.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/browser/scoped_account_consistency.h"
#include "components/signin/core/browser/signin_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

#if BUILDFLAG(ENABLE_MIRROR)

TEST(ProfileManagementSwitchesTest, GetAccountConsistencyMethodMirror) {
  // Mirror is enabled by default on some platforms.
  EXPECT_EQ(AccountConsistencyMethod::kMirror, GetAccountConsistencyMethod());
  EXPECT_TRUE(IsAccountConsistencyMirrorEnabled());
  EXPECT_FALSE(IsDiceMigrationEnabled());
  EXPECT_FALSE(IsDiceFixAuthErrorsEnabled());
}

#else

TEST(ProfileManagementSwitchesTest, GetAccountConsistencyMethod) {
  SetGaiaOriginIsolatedCallback(base::Bind([] { return true; }));
  base::MessageLoop loop;
  sync_preferences::TestingPrefServiceSyncable pref_service;
  RegisterAccountConsistencyProfilePrefs(pref_service.registry());
  std::unique_ptr<BooleanPrefMember> dice_pref_member =
      CreateDicePrefMember(&pref_service);

  // By default account consistency is disabled.
  EXPECT_EQ(AccountConsistencyMethod::kDisabled, GetAccountConsistencyMethod());

  struct TestCase {
    AccountConsistencyMethod method;
    bool expect_mirror_enabled;
    bool expect_dice_fix_auth_errors;
    bool expect_dice_prepare_migration;
    bool expect_dice_prepare_migration_new_endpoint;
    bool expect_dice_migration;
    bool expect_dice_enabled_for_profile;
  } test_cases[] = {
    {AccountConsistencyMethod::kDisabled, false, false, false, false, false,
     false},
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    {AccountConsistencyMethod::kDiceFixAuthErrors, false, true, false, false,
     false, false},
    {AccountConsistencyMethod::kDicePrepareMigration, false, true, true, false,
     false, false},
    {AccountConsistencyMethod::kDicePrepareMigrationChromeSyncEndpoint, false,
     true, true, true, false, false},
    {AccountConsistencyMethod::kDiceMigration, false, true, true, true, true,
     false},
    {AccountConsistencyMethod::kDice, false, true, true, true, true, true},
#endif
    {AccountConsistencyMethod::kMirror, true, false, false, false, false, false}
  };

  for (const TestCase& test_case : test_cases) {
    ScopedAccountConsistency scoped_method(test_case.method);
    EXPECT_EQ(test_case.method, GetAccountConsistencyMethod());
    EXPECT_EQ(test_case.expect_mirror_enabled,
              IsAccountConsistencyMirrorEnabled());
    EXPECT_EQ(test_case.expect_dice_fix_auth_errors,
              IsDiceFixAuthErrorsEnabled());
    EXPECT_EQ(test_case.expect_dice_migration, IsDiceMigrationEnabled());
    EXPECT_EQ(test_case.expect_dice_prepare_migration,
              IsDicePrepareMigrationEnabled());
    EXPECT_EQ(test_case.expect_dice_prepare_migration_new_endpoint,
              IsDicePrepareMigrationChromeSyncEndpointEnabled());
    EXPECT_EQ(test_case.expect_dice_enabled_for_profile,
              IsDiceEnabledForProfile(&pref_service));
    EXPECT_EQ(test_case.expect_dice_enabled_for_profile,
              IsDiceEnabled(dice_pref_member.get()));
  }
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST(ProfileManagementSwitchesTest, DiceMigration) {
  base::MessageLoop loop;
  sync_preferences::TestingPrefServiceSyncable pref_service;
  RegisterAccountConsistencyProfilePrefs(pref_service.registry());
  std::unique_ptr<BooleanPrefMember> dice_pref_member =
      CreateDicePrefMember(&pref_service);

  {
    ScopedAccountConsistencyDiceMigration scoped_dice_migration;
    MigrateProfileToDice(&pref_service);
  }

  struct TestCase {
    AccountConsistencyMethod method;
    bool expect_dice_enabled_for_profile;
  } test_cases[] = {
      {AccountConsistencyMethod::kDisabled, false},
      {AccountConsistencyMethod::kDiceFixAuthErrors, false},
      {AccountConsistencyMethod::kDicePrepareMigration, false},
      {AccountConsistencyMethod::kDicePrepareMigrationChromeSyncEndpoint,
       false},
      {AccountConsistencyMethod::kDiceMigration, true},
      {AccountConsistencyMethod::kDice, true},
      {AccountConsistencyMethod::kMirror, false}};

  for (const TestCase& test_case : test_cases) {
    ScopedAccountConsistency scoped_method(test_case.method);
    EXPECT_EQ(test_case.expect_dice_enabled_for_profile,
              IsDiceEnabledForProfile(&pref_service));
    EXPECT_EQ(test_case.expect_dice_enabled_for_profile,
              IsDiceEnabled(dice_pref_member.get()));
  }
}

// Tests that Dice is disabled when site isolation is disabled.
TEST(ProfileManagementSwitchesTest, GaiaSiteIsolation) {
  ScopedAccountConsistencyDicePrepareMigration scoped_dice;
  ASSERT_TRUE(IsDicePrepareMigrationEnabled());

  SetGaiaOriginIsolatedCallback(base::Bind([] { return false; }));
  EXPECT_FALSE(IsDicePrepareMigrationEnabled());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#endif  // BUILDFLAG(ENABLE_MIRROR)

}  // namespace signin
