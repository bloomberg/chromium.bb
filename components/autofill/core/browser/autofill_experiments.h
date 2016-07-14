// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_

#include <string>

namespace base {
struct Feature;
}

namespace sync_driver {
class SyncService;
}

class PrefService;

namespace autofill {

extern const base::Feature kAutofillProfileCleanup;
extern const base::Feature kAutofillCreditCardSigninPromo;
extern const char kCreditCardSigninPromoImpressionLimitParamKey[];

// Returns true if autofill should be enabled. See also
// IsInAutofillSuggestionsDisabledExperiment below.
bool IsAutofillEnabled(const PrefService* pref_service);

// Returns true if autofill suggestions are disabled via experiment. The
// disabled experiment isn't the same as disabling autofill completely since we
// still want to run detection code for metrics purposes. This experiment just
// disables providing suggestions.
bool IsInAutofillSuggestionsDisabledExperiment();

// Returns whether the Autofill profile cleanup feature is enabled.
bool IsAutofillProfileCleanupEnabled();

// Returns whether the Autofill credit card signin promo should be shown.
bool IsAutofillCreditCardSigninPromoEnabled();

// Returns the maximum number of impressions of the credit card signin promo, or
// 0 if there are no limits.
int GetCreditCardSigninPromoImpressionLimit();

// Returns true if the user should be offered to locally store unmasked cards.
// This controls whether the option is presented at all rather than the default
// response of the option.
bool OfferStoreUnmaskedCards();

// Returns true if uploading credit cards to Wallet servers is enabled. This
// requires the appropriate flags and user settings to be true and the user to
// be a member of a supported domain.
bool IsCreditCardUploadEnabled(const PrefService* pref_service,
                               const sync_driver::SyncService* sync_service,
                               const std::string& user_email);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_
