// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILL_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILL_UTIL_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

class AutofillField;

namespace fill_util {

// Set |field_data|'s value to |value|. Uses |field|, |address_language_code|,
// and |app_locale| as hints when filling exceptional cases like phone number
// values and <select> fields. Returns |true| if the field has been filled,
// |false| otherwise.
bool FillFormField(const AutofillField& field,
                   const base::string16& value,
                   const std::string& address_language_code,
                   const std::string& app_locale,
                   FormFieldData* field_data);

// Returns the phone number value for the given |field|. The returned value
// might be |number|, or could possibly be a meaningful subset |number|, if
// that's appropriate for the field.
base::string16 GetPhoneNumberValue(const AutofillField& field,
                                   const base::string16& number,
                                   const FormFieldData& field_data);

// Returns the index of the shortest entry in the given select field of which
// |value| is a substring. Returns -1 if no such entry exists.
int FindShortestSubstringMatchInSelect(const base::string16& value,
                                       bool ignore_whitespace,
                                       const FormFieldData* field);

}  // namespace fill_util
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILL_UTIL_H_
