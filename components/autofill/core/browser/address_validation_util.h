// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_VALIDATION_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_VALIDATION_UTIL_H_

#include "components/autofill/core/browser/autofill_profile.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"

namespace autofill {
namespace address_validation_util {

// Validates the address fields of the |profile|.
// Returns the ValidityState of the |profile| according to its address fields.
AutofillProfile::ValidityState ValidateAddress(
    AutofillProfile* profile,
    AddressValidator* address_validator);

}  // namespace address_validation_util
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_VALIDATION_UTIL_H_
