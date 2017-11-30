// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/buildflag.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_features.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;

// Manages the account consistency mode for each profile.
class AccountConsistencyModeManager {
 public:
  explicit AccountConsistencyModeManager(Profile* profile);
  ~AccountConsistencyModeManager();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Schedules migration to happen at next startup.
  void SetReadyForDiceMigration(bool is_ready);
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(AccountConsistencyModeManagerTest,
                           MigrateAtCreation);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Schedules migration to happen at next startup. Exposed as a static function
  // for testing.
  static void SetDiceMigrationOnStartup(PrefService* prefs, bool migrate);

  // Returns true if migration can happen on the next startup.
  bool IsReadyForDiceMigration();
#endif

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AccountConsistencyModeManager);
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_H_
