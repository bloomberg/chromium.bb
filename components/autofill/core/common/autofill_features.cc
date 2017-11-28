// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

namespace autofill {
namespace features {

// Controls whether the AddressNormalizer is supplied. If available, it may be
// used to normalize address and will incur fetching rules from the server.
const base::Feature kAutofillAddressNormalizer{
    "AutofillAddressNormalizer", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether or not a minimum number of fields is required before
// heuristic field type prediction is run for a form.
const base::Feature kAutofillEnforceMinRequiredFieldsForHeuristics{
    "AutofillEnforceMinRequiredFieldsForHeuristics",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether or not a minimum number of fields is required before
// crowd-sourced field type predictions are queried for a form.
const base::Feature kAutofillEnforceMinRequiredFieldsForQuery{
    "AutofillEnforceMinRequiredFieldsForQuery",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether or not a minimum number of fields is required before
// field type votes are uploaded to the crowd-sourcing server.
const base::Feature kAutofillEnforceMinRequiredFieldsForUpload{
    "AutofillEnforceMinRequiredFieldsForUpload",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls attaching the autofill type predictions to their respective
// element in the DOM.
const base::Feature kAutofillShowTypePredictions{
    "AutofillShowTypePredictions", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace autofill
