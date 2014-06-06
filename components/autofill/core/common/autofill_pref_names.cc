// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_pref_names.h"

namespace autofill {
namespace prefs {

// Boolean that is true when auxiliary Autofill profiles are enabled. This pref
// is only relevant to Android.
const char kAutofillAuxiliaryProfilesEnabled[] =
    "autofill.auxiliary_profiles_enabled";

// Boolean that is true if Autofill is enabled and allowed to save profile data.
const char kAutofillEnabled[] = "autofill.enabled";

// Boolean that is true when Chrome has attempted to access the user's Address
// Book on Mac. This preference is important since the first time Chrome
// attempts to access the user's Address Book, Cocoa presents a blocking dialog
// to the user.
const char kAutofillMacAddressBookQueried[] =
    "autofill.auxiliary_profiles_queried";

// Double that indicates negative (for not matched forms) upload rate.
const char kAutofillNegativeUploadRate[] = "autofill.negative_upload_rate";

// Double that indicates positive (for matched forms) upload rate.
const char kAutofillPositiveUploadRate[] = "autofill.positive_upload_rate";

// Whether Autofill should try to use the Mac Address Book. If this value is
// true, then kAutofillMacAddressBookQueried is expected to also be true.
const char kAutofillUseMacAddressBook[] = "autofill.use_mac_address_book";

}  // namespace prefs
}  // namespace autofill
