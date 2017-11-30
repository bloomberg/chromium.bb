// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"

#include <vector>

#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/scoped_account_consistency.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DiceAccountReconcilorDelegateTest, RevokeTokens) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  signin::RegisterAccountConsistencyProfilePrefs(pref_service.registry());
  TestSigninClient client(&pref_service);
  signin::DiceAccountReconcilorDelegate delegate(&client);
  {
    // Dice is enabled, don't revoke.
    signin::ScopedAccountConsistencyDice scoped_dice;
    EXPECT_FALSE(delegate.ShouldRevokeAllSecondaryTokensBeforeReconcile(
        std::vector<gaia::ListedAccount>()));
  }
  {
    signin::ScopedAccountConsistencyDiceMigration scoped_dice_migration;
    // Gaia accounts are not empty, don't revoke.
    gaia::ListedAccount gaia_account;
    gaia_account.id = "other";
    EXPECT_FALSE(delegate.ShouldRevokeAllSecondaryTokensBeforeReconcile(
        std::vector<gaia::ListedAccount>{gaia_account}));
    // Revoke.
    EXPECT_TRUE(delegate.ShouldRevokeAllSecondaryTokensBeforeReconcile(
        std::vector<gaia::ListedAccount>()));
  }
}
