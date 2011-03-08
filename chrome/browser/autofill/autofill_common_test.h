// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_COMMON_TEST_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_COMMON_TEST_H_
#pragma once

class AutoFillProfile;
class CreditCard;
class Profile;

namespace webkit_glue {
struct FormField;
}  // namespace webkit_glue

// Common utilities shared amongst AutoFill tests.
namespace autofill_test {

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
    const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone, const char* fax);

void SetProfileInfoWithGuid(AutoFillProfile* profile,
    const char* guid, const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone, const char* fax);

// A unit testing utility that is common to a number of the AutoFill unit
// tests.  |SetCreditCardInfo| provides a quick way to populate a credit card
// with c-strings.
void SetCreditCardInfo(CreditCard* credit_card,
    const char* name_on_card, const char* card_number,
    const char* expiration_month, const char* expiration_year);

// TODO(isherman): We should do this automatically for all tests, not manually
// on a per-test basis: http://crbug.com/57221
// Disables or mocks out code that would otherwise reach out to system services.
void DisableSystemServices(Profile* profile);

}  // namespace autofill_test

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_COMMON_TEST_H_
