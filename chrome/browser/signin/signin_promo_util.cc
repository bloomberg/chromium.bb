// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "net/base/network_change_notifier.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_mutator.h"

namespace signin {

bool ShouldShowPromo(Profile* profile) {
#if defined(OS_CHROMEOS)
  // There's no need to show the sign in promo on cros since cros users are
  // already logged in.
  return false;
#else

  // Don't bother if we don't have any kind of network connection.
  if (net::NetworkChangeNotifier::IsOffline())
    return false;

  // Consider original profile even if an off-the-record profile was
  // passed to this method as sign-in state is only defined for the
  // primary profile.
  Profile* original_profile = profile->GetOriginalProfile();

  // Don't show for supervised profiles.
  if (original_profile->IsSupervised())
    return false;

  // Don't show if sign-in is not allowed.
  if (!original_profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed))
    return false;

  // Display the signin promo if the user is not signed in.
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(original_profile);
  if (identity_manager->HasPrimaryAccount() ||
      identity_manager->GetPrimaryAccountMutator()
          ->LegacyIsPrimaryAccountAuthInProgress()) {
    return false;
  }

  return true;
#endif
}

}  // namespace signin
