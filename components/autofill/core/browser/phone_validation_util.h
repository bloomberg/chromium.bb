// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PHONE_VALIDATION_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PHONE_VALIDATION_UTIL_H_

#include "components/autofill/core/browser/autofill_profile.h"

namespace autofill {
namespace phone_validation_util {

// Validates the phone number field of the |profile|.
// Returns the ValidityState of the |profile| according to its phone fields.
AutofillProfile::ValidityState ValidatePhoneNumber(AutofillProfile* profile);

}  // namespace phone_validation_util
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PHONE_VALIDATION_UTIL_H_
