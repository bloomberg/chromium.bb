// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_

#include "build/build_config.h"

namespace password_manager {
namespace prefs {

// Alphabetical list of preference names specific to the PasswordManager
// component.

// Boolean controlling whether the password manager allows automatic signing in
// through Credential Manager API.
extern const char kCredentialsEnableAutosignin[];

// The value of this preference controls whether the Password Manager will save
// credentials. When it is false, it doesn't ask if you want to save passwords
// but will continue to fill passwords.
// TODO(melandory): Preference should also control autofill behavior for the
// passwords.
extern const char kCredentialsEnableService[];

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
// The current state of the migration to LoginDB from Keyring/Kwallet on Linux.
extern const char kMigrationToLoginDBStep[];
#endif

#if defined(OS_WIN)
// Whether the password was blank, only valid if OS password was last changed
// on or before the value contained in kOsPasswordLastChanged.
extern const char kOsPasswordBlank[];

// The number of seconds since epoch that the OS password was last changed.
extern const char kOsPasswordLastChanged[];
#endif

#if defined(OS_MACOSX)
// The current status of migrating the passwords from the Keychain to the
// database. Stores a value from MigrationStatus.
extern const char kKeychainMigrationStatus[];

// The date of when passwords were cleaned up for MacOS users who previously
// lost access to their password because of encryption key modification in
// Keychain.
extern const char kPasswordRecovery[];
#endif

// Boolean that indicated whether first run experience for the auto sign-in
// prompt was shown or not.
extern const char kWasAutoSignInFirstRunExperienceShown[];

// Boolean that indicated if user interacted with the Chrome Sign in promo.
extern const char kWasSignInPasswordPromoClicked[];

// Number of times the Chrome Sign in promo popped up.
extern const char kNumberSignInPasswordPromoShown[];

// True if the counters for the sign in promo were reset for M79.
// Safe to remove for M82.
extern const char kSignInPasswordPromoRevive[];

// Dictionary of accounts that have opted in to use the Google account storage
// for passwords (map from hash of Gaia ID to bool).
extern const char kAccountStorageOptedInAccounts[];

// Boolean that is true when the default password store is the Google account
// store, and false when the profile store is the default store.
extern const char kIsAccountStoreDefault[];

// String that represents the sync password hash.
extern const char kSyncPasswordHash[];

// String that represents the sync password length and salt. Its format is
// encrypted and converted to base64 string "<password length, as ascii
// int>.<16 char salt>".
extern const char kSyncPasswordLengthAndHashSalt[];

// Indicates the time (in seconds) when last cleaning of obsolete HTTP
// credentials was performed.
extern const char kLastTimeObsoleteHttpCredentialsRemoved[];

// List that contains captured password hashes.
extern const char kPasswordHashDataList[];

// Integer indicating the state of the password manager onboarding experience.
extern const char kPasswordManagerOnboardingState[];

// Boolean indicating whether Chrome should check whether the credentials
// submitted by the user were part of a leak.
extern const char kPasswordLeakDetectionEnabled[];

// Boolean indicating whether this profile was ever eligible for password
// manager onboarding. If the profile was eligible, then the feature flag
// will be checked and this will be set to true. This is then used for
// subsequent feature checks to ensure data completeness.
extern const char kWasOnboardingFeatureCheckedBefore[];

}  // namespace prefs
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
