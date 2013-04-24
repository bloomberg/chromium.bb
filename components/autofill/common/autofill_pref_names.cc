// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/common/autofill_pref_names.h"

namespace autofill {
namespace prefs {

// Boolean that is true when auxiliary Autofill profiles are enabled.
// Currently applies to Address Book "me" card on Mac.  False on Win and Linux.
const char kAutofillAuxiliaryProfilesEnabled[] =
    "autofill.auxiliary_profiles_enabled";

// Boolean that is true if Autofill is enabled and allowed to save profile data.
const char kAutofillEnabled[] = "autofill.enabled";

// Double that indicates negative (for not matched forms) upload rate.
const char kAutofillNegativeUploadRate[] = "autofill.negative_upload_rate";

// Double that indicates positive (for matched forms) upload rate.
const char kAutofillPositiveUploadRate[] = "autofill.positive_upload_rate";

}  // namespace prefs
}  // namespace autofill
