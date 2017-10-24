// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are functions to access various profile-management flags but with
// possible overrides from Experiements.  This is done inside chrome/common
// because it is accessed by files through the chrome/ directory tree.

#ifndef COMPONENTS_SIGNIN_CORE_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_
#define COMPONENTS_SIGNIN_CORE_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_

#include <memory>

#include "base/feature_list.h"
#include "components/prefs/pref_member.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace signin {

// Account consistency feature. Only used on platforms where Mirror is not
// always enabled (ENABLE_MIRROR is false).
extern const base::Feature kAccountConsistencyFeature;

// The account consistency method feature parameter name.
extern const char kAccountConsistencyFeatureMethodParameter[];

// Account consistency method feature values.
extern const char kAccountConsistencyFeatureMethodMirror[];
extern const char kAccountConsistencyFeatureMethodDiceFixAuthErrors[];
extern const char kAccountConsistencyFeatureMethodDiceMigration[];
extern const char kAccountConsistencyFeatureMethodDice[];

// TODO(https://crbug.com/777774): Cleanup this enum and remove related
// functions once Dice is fully rolled out, and/or Mirror code is removed on
// desktop.
enum class AccountConsistencyMethod {
  // No account consistency.
  kDisabled,

  // Account management UI in the avatar bubble.
  kMirror,

  // No account consistency, but Dice fixes authentication errors.
  kDiceFixAuthErrors,

  // Account management UI on Gaia webpages is enabled once the accounts become
  // consistent.
  kDiceMigration,

  // Account management UI on Gaia webpages is enabled. If accounts are not
  // consistent when this is enabled, the account reconcilor enforces the
  // consistency.
  kDice
};

////////////////////////////////////////////////////////////////////////////////
// AccountConsistencyMethod related functions:

// Returns the account consistency method.
AccountConsistencyMethod GetAccountConsistencyMethod();

// Checks whether Mirror account consistency is enabled. If enabled, the account
// management UI is available in the avatar bubble.
bool IsAccountConsistencyMirrorEnabled();

// Returns true if the account consistency method is kDiceFixAuthErrors,
// kDiceMigration or kDice.
bool IsDiceFixAuthErrorsEnabled();

// Returns true if Dice account consistency is enabled or if the Dice migration
// process is in progress (account consistency method is kDice or
// kDiceMigration).
// To check wether Dice is enabled (i.e. the migration is complete), use
// IsDiceEnabledForProfile().
// WARNING: returns false when the method is kDiceFixAuthErrors.
bool IsDiceMigrationEnabled();

////////////////////////////////////////////////////////////////////////////////
// Functions to test if Dice is enabled for a user profile:

// If true, then account management is done through Gaia webpages.
// Can only be used on the UI thread.
bool IsDiceEnabledForProfile(PrefService* user_prefs);

// If true, then account management is done through Gaia webpages.
// Can be called on any thread, using a pref member obtained with
// CreateDicePrefMember().
// On the UI thread, consider using IsDiceEnabledForProfile() instead.
// Example usage:
//
// // On UI thread:
// std::unique_ptr<BooleanPrefMember> pref_member = GetDicePrefMember(prefs);
// pref_member->MoveToThread(io_thread);
//
// // Later, on IO thread:
// bool dice_enabled = GetDicePrefMember(pref_member.get());
bool IsDiceEnabled(BooleanPrefMember* dice_pref_member);

// Gets a pref member suitable to use with IsDiceEnabled().
std::unique_ptr<BooleanPrefMember> CreateDicePrefMember(
    PrefService* user_prefs);

// Called to migrate a profile to Dice. After this call, it is enabled forever.
void MigrateProfileToDice(PrefService* user_prefs);

////////////////////////////////////////////////////////////////////////////////
// Other functions:

// Register account consistency user preferences.
void RegisterAccountConsistencyProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

// Whether the chrome.identity API should be multi-account.
bool IsExtensionsMultiAccount();

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_
