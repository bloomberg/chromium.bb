// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_experiments.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {

bool IsAutofillEnabled(const PrefService* pref_service) {
  std::string group_name =
      base::FieldTrialList::FindFullName("AutofillEnabled");
  if (group_name == "Disabled")  // Assume enabled when no experiment.
    return false;

  // When experiment enables autofill, fall back on the user pref.
  return pref_service->GetBoolean(prefs::kAutofillEnabled);
}

bool OfferStoreUnmaskedCards() {
#if defined(ENABLE_SAVE_WALLET_CARDS_LOCALLY)
  // Query the field trial before checking command line flags to ensure UMA
  // reports the correct group.
  std::string group_name =
      base::FieldTrialList::FindFullName("OfferStoreUnmaskedWalletCards");

  // The checkbox can be forced on or off with flags.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOfferStoreUnmaskedWalletCards))
    return true;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableOfferStoreUnmaskedWalletCards))
    return false;

  // Otherwise use the field trial to show the checkbox or not.
  return group_name != "Disabled";
#else
  return false;
#endif
}

}  // namespace autofill
