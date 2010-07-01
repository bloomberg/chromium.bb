// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_COMMON_UNITTEST_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_COMMON_UNITTEST_H_

class AutoFillProfile;
class CreditCard;

namespace webkit_glue {
class FormField;
}  // namespace webkit_glue

// Common utilities shared amongst AutoFill unit tests.
namespace autofill_unittest {

// Provides a quick way to populate a FormField with c-strings.
void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         webkit_glue::FormField* field);

// A unit testing utility that is common to a number of the AutoFill unit
// tests.  |SetProfileInfo| provides a quick way to populate a profile with
// c-strings.
void SetProfileInfo(AutoFillProfile* profile,
    const char* label, const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone, const char* fax);

// A unit testing utility that is common to a number of the AutoFill unit
// tests.  |SetCreditCardInfo| provides a quick way to populate a credit card
// with c-strings.
void SetCreditCardInfo(CreditCard* credit_card,
    const char* label, const char* name_on_card, const char* type,
    const char* card_number, const char* expiration_month,
    const char* expiration_year, const char* billing_address);

}  // namespace autofill_unittest

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_COMMON_UNITTEST_H_
