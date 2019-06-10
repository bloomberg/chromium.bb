// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The signin manager encapsulates some functionality tracking which user is
// signed in. See PrimaryAccountManager for full description of
// responsibilities. The class defined in this file provides functionality
// required by all platforms except Chrome OS.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_PRIMARY_ACCOUNT_POLICY_MANAGER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_PRIMARY_ACCOUNT_POLICY_MANAGER_H_

#include "build/build_config.h"

#if defined(OS_CHROMEOS)

#include "components/signin/core/browser/primary_account_manager.h"

#else

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/primary_account_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "net/cookies/canonical_cookie.h"

class PrefService;
class ProfileOAuth2TokenService;

namespace identity {
class IdentityManager;
}  // namespace identity

class PrimaryAccountPolicyManager : public PrimaryAccountManager {
 public:
  PrimaryAccountPolicyManager(
      SigninClient* client,
      ProfileOAuth2TokenService* token_service,
      AccountTrackerService* account_tracker_service,
      signin::AccountConsistencyMethod account_consistency);
  ~PrimaryAccountPolicyManager() override;

  // On platforms where PrimaryAccountManager is responsible for dealing with
  // invalid username policy updates, we need to check this during
  // initialization and sign the user out.
  void FinalizeInitBeforeLoadingRefreshTokens(
      PrefService* local_state) override;

 private:
  friend class identity::IdentityManager;
  FRIEND_TEST_ALL_PREFIXES(PrimaryAccountManagerTest, Prohibited);
  FRIEND_TEST_ALL_PREFIXES(PrimaryAccountManagerTest, TestAlternateWildcard);

  // Returns true if a signin to Chrome is allowed (by policy or pref).
  bool IsSigninAllowed() const;

  void OnSigninAllowedPrefChanged();
  void OnGoogleServicesUsernamePatternChanged();

  // Returns true if the passed username is allowed by policy.
  bool IsAllowedUsername(const std::string& username) const;

  // Helper object to listen for changes to signin preferences stored in non-
  // profile-specific local prefs (like kGoogleServicesUsernamePattern).
  PrefChangeRegistrar local_state_pref_registrar_;

  // Helper object to listen for changes to the signin allowed preference.
  BooleanPrefMember signin_allowed_;

  base::WeakPtrFactory<PrimaryAccountPolicyManager> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrimaryAccountPolicyManager);
};

#endif  // !defined(OS_CHROMEOS)

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_PRIMARY_ACCOUNT_POLICY_MANAGER_H_
