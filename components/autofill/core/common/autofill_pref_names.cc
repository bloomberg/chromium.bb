// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_pref_names.h"

namespace autofill {
namespace prefs {

// Number of times the credit card signin promo has been shown.
const char kAutofillCreditCardSigninPromoImpressionCount[] =
    "autofill.credit_card_signin_promo_impression_count";

// Boolean that is true if Autofill is enabled and allowed to save profile data.
const char kAutofillEnabled[] = "autofill.enabled";

// Boolean that is true if Autofill address profiles were fixed regarding their
// bad use dates.
const char kAutofillProfileUseDatesFixed[] = "autofill.profile_use_dates_fixed";

// Boolean that's true when Wallet card and address import is enabled by the
// user.
const char kAutofillWalletImportEnabled[] = "autofill.wallet_import_enabled";

// Integer that is set to the last version where the profile deduping routine
// was run. This routine will be run once per version.
const char kAutofillLastVersionDeduped[] = "autofill.last_version_deduped";

// Boolean that is set to the last choice user made when prompted for saving an
// unmasked server card locally.
const char kAutofillWalletImportStorageCheckboxState[] =
    "autofill.wallet_import_storage_checkbox_state";

// Integer that is set to the last choice user made when prompted for saving a
// credit card. The prompt is for user's consent in saving the card in the
// server for signed in users and saving the card locally for non signed-in
// users.
const char kAutofillAcceptSaveCreditCardPromptState[] =
    "autofill.accept_save_credit_card_prompt_state";

}  // namespace prefs
}  // namespace autofill
