// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_

#include "base/feature_list.h"

namespace autofill {
namespace features {

// All features in alphabetical order.
extern const base::Feature kAutofillAddressNormalizer;
extern const base::Feature kAutofillCreditCardDropdownGooglePayBranding;
extern const base::Feature kAutofillEnforceMinRequiredFieldsForHeuristics;
extern const base::Feature kAutofillEnforceMinRequiredFieldsForQuery;
extern const base::Feature kAutofillEnforceMinRequiredFieldsForUpload;
extern const base::Feature kAutofillRestrictUnownedFieldsToFormlessCheckout;
extern const base::Feature kAutofillShowTypePredictions;
extern const base::Feature kAutofillUpstreamUseGooglePayBranding;
extern const base::Feature kAutofillUseNewSettingsNameInDropdown;

}  // namespace features
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
