// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_COMMON_TEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_COMMON_TEST_H_

namespace content {
class BrowserContext;
}

namespace autofill {

class AutofillProfile;
class CreditCard;
struct FormData;
struct FormFieldData;

// Common utilities shared amongst Autofill tests.
namespace test {

// Provides a quick way to populate a FormField with c-strings.
void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         FormFieldData* field);

// Populates |form| with data corresponding to a simple address form.
// Note that this actually appends fields to the form data, which can be useful
// for building up more complex test forms.
void CreateTestAddressFormData(FormData* form);

// Returns a profile full of dummy info.
AutofillProfile GetFullProfile();

// Returns a profile full of dummy info, different to the above.
AutofillProfile GetFullProfile2();

// Returns a verified profile full of dummy info.
AutofillProfile GetVerifiedProfile();

// Returns a verified profile full of dummy info, different to the above.
AutofillProfile GetVerifiedProfile2();

// Returns a credit card full of dummy info.
CreditCard GetCreditCard();

// Returns a verified credit card full of dummy info.
CreditCard GetVerifiedCreditCard();

// A unit testing utility that is common to a number of the Autofill unit
// tests.  |SetProfileInfo| provides a quick way to populate a profile with
// c-strings.
void SetProfileInfo(AutofillProfile* profile,
    const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone);

void SetProfileInfoWithGuid(AutofillProfile* profile,
    const char* guid, const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone);

// A unit testing utility that is common to a number of the Autofill unit
// tests.  |SetCreditCardInfo| provides a quick way to populate a credit card
// with c-strings.
void SetCreditCardInfo(CreditCard* credit_card,
    const char* name_on_card, const char* card_number,
    const char* expiration_month, const char* expiration_year);

// TODO(isherman): We should do this automatically for all tests, not manually
// on a per-test basis: http://crbug.com/57221
// Disables or mocks out code that would otherwise reach out to system services.
void DisableSystemServices(content::BrowserContext* browser_context);

}  // namespace test
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_COMMON_TEST_H_
