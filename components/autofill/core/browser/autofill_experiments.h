// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_

#include <string>

#include "base/strings/string16.h"
#include "build/build_config.h"

class PrefService;

namespace base {
struct Feature;
}

namespace syncer {
class SyncService;
}

namespace autofill {

// Boolean Features

extern const base::Feature kAutofillExpandedPopupViews;
extern const base::Feature kAutofillScanCardholderName;
extern const base::Feature kAutofillUpstream;
extern const base::Feature kAutofillUpstreamAllowAllEmailDomains;
extern const base::Feature kAutofillUpstreamUpdatePromptExplanation;

// Parameterized Features (grouped with parameter name and options)
#if !defined(OS_ANDROID)
extern const base::Feature kAutofillDropdownLayoutExperiment;
extern const char kAutofillDropdownLayoutParameterName[];
extern const char kAutofillDropdownLayoutParameterLeadingIcon[];
extern const char kAutofillDropdownLayoutParameterTrailingIcon[];
#endif  // !defined(OS_ANDROID)

// Returns true if uploading credit cards to Wallet servers is enabled. This
// requires the appropriate flags and user settings to be true and the user to
// be a member of a supported domain.
bool IsCreditCardUploadEnabled(const PrefService* pref_service,
                               const syncer::SyncService* sync_service,
                               const std::string& user_email);

// Returns true if autofill suggestions are disabled via experiment. The
// disabled experiment isn't the same as disabling autofill completely since we
// still want to run detection code for metrics purposes. This experiment just
// disables providing suggestions.
bool IsInAutofillSuggestionsDisabledExperiment();

// Returns true if the user should be offered to locally store unmasked cards.
// This controls whether the option is presented at all rather than the default
// response of the option.
bool OfferStoreUnmaskedCards();

#if !defined(OS_ANDROID)
enum class ForcedPopupLayoutState {
  kDefault,       // No popup layout forced by experiment.
  kLeadingIcon,   // Experiment forces leading (left in LTR) icon layout.
  kTrailingIcon,  // Experiment forces trailing (right in LTR) icon layout.
};

// Returns kDefault if no experimental behavior is enabled for
// kAutofillDropdownLayoutExperiment; returns kLeftIcon or kRightIcon
// if the experiment param matches kAutofillDropdownLayoutParameterLeadingIcon
// or kAutofillDropdownLayoutParameterTrailingIcon, respectively.
ForcedPopupLayoutState GetForcedPopupLayoutState();
#endif  // !defined(OS_ANDROID)

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXPERIMENTS_H_
