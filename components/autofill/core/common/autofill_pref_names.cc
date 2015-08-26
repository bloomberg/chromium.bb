// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_pref_names.h"

namespace autofill {
namespace prefs {

// Boolean that is true if Autofill is enabled and allowed to save profile data.
const char kAutofillEnabled[] = "autofill.enabled";

// Double that indicates negative (for not matched forms) upload rate.
const char kAutofillNegativeUploadRate[] = "autofill.negative_upload_rate";

// Double that indicates positive (for matched forms) upload rate.
const char kAutofillPositiveUploadRate[] = "autofill.positive_upload_rate";

// Boolean that's true when Wallet card and address import is enabled by the
// user. This will only be available to the user if the overall feature is
// enabled.
const char kAutofillWalletImportEnabled[] = "autofill.wallet_import_enabled";

// Boolean that allows the "Don't ask again for this card" checkbox to be
// sticky.
const char kAutofillWalletImportStorageCheckboxState[] =
    "autofill.wallet_import_storage_checkbox_state";

// Enables/disables the Wallet card and address feature. Set via sync
// experiment.
const char kAutofillWalletSyncExperimentEnabled[] =
    "autofill.wallet_import_sync_experiment_enabled";

}  // namespace prefs
}  // namespace autofill
