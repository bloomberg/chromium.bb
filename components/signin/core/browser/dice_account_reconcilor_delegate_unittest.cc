// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"

#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/scoped_account_consistency.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

// Checks that Dice migration happens when the reconcilor is created.
TEST(DiceAccountReconcilorDelegateTest, MigrateAtCreation) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  signin::DiceAccountReconcilorDelegate::RegisterProfilePrefs(
      pref_service.registry());
  signin::RegisterAccountConsistencyProfilePrefs(pref_service.registry());

  {
    // Migration does not happen if SetDiceMigrationOnStartup() is not called.
    signin::ScopedAccountConsistencyDiceMigration scoped_dice_migration;
    EXPECT_FALSE(signin::IsDiceEnabledForProfile(&pref_service));
    signin::DiceAccountReconcilorDelegate delegate(&pref_service, false);
    EXPECT_FALSE(delegate.IsReadyForDiceMigration(false /* is_new_profile */));
    EXPECT_FALSE(signin::IsDiceEnabledForProfile(&pref_service));
  }

  signin::DiceAccountReconcilorDelegate::SetDiceMigrationOnStartup(
      &pref_service, true);

  {
    // Migration does not happen if Dice is not enabled.
    signin::ScopedAccountConsistencyDiceFixAuthErrors scoped_dice_fix_errors;
    EXPECT_FALSE(signin::IsDiceEnabledForProfile(&pref_service));
    signin::DiceAccountReconcilorDelegate delegate(&pref_service, false);
    EXPECT_TRUE(delegate.IsReadyForDiceMigration(false /* is_new_profile */));
    EXPECT_FALSE(signin::IsDiceEnabledForProfile(&pref_service));
  }

  {
    // Migration happens.
    signin::ScopedAccountConsistencyDiceMigration scoped_dice_migration;
    EXPECT_FALSE(signin::IsDiceEnabledForProfile(&pref_service));
    signin::DiceAccountReconcilorDelegate delegate(&pref_service, false);
    EXPECT_TRUE(delegate.IsReadyForDiceMigration(false /* is_new_profile */));
    EXPECT_TRUE(signin::IsDiceEnabledForProfile(&pref_service));
  }
}

// Checks that new profiles are migrated at creation.
TEST(DiceAccountReconcilorDelegateTest, NewProfile) {
  signin::ScopedAccountConsistencyDiceMigration scoped_dice_migration;
  sync_preferences::TestingPrefServiceSyncable pref_service;
  signin::DiceAccountReconcilorDelegate::RegisterProfilePrefs(
      pref_service.registry());
  signin::RegisterAccountConsistencyProfilePrefs(pref_service.registry());
  EXPECT_FALSE(signin::IsDiceEnabledForProfile(&pref_service));
  signin::DiceAccountReconcilorDelegate delegate(&pref_service, true);
  EXPECT_TRUE(signin::IsDiceEnabledForProfile(&pref_service));
}
