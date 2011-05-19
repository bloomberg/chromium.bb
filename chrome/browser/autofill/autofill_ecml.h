// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_ECML_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_ECML_H_
#pragma once

#include <vector>

#include "base/string16.h"

class AutofillField;

// Shipping fields.
extern const char kEcmlShipToTitle[];
extern const char kEcmlShipToFirstName[];
extern const char kEcmlShipToMiddleName[];
extern const char kEcmlShipToLastName[];
extern const char kEcmlShipToNameSuffix[];
extern const char kEcmlShipToCompanyName[];
extern const char kEcmlShipToAddress1[];
extern const char kEcmlShipToAddress2[];
extern const char kEcmlShipToAddress3[];
extern const char kEcmlShipToCity[];
extern const char kEcmlShipToStateProv[];
extern const char kEcmlShipToPostalCode[];
extern const char kEcmlShipToCountry[];
extern const char kEcmlShipToPhone[];
extern const char kEcmlShipToEmail[];

// Billing fields.
extern const char kEcmlBillToTitle[];
extern const char kEcmlBillToFirstName[];
extern const char kEcmlBillToMiddleName[];
extern const char kEcmlBillToLastName[];
extern const char kEcmlBillToNameSuffix[];
extern const char kEcmlBillToCompanyName[];
extern const char kEcmlBillToAddress1[];
extern const char kEcmlBillToAddress2[];
extern const char kEcmlBillToAddress3[];
extern const char kEcmlBillToCity[];
extern const char kEcmlBillToStateProv[];
extern const char kEcmlBillToPostalCode[];
extern const char kEcmlBillToCountry[];
extern const char kEcmlBillToPhone[];
extern const char kEcmlBillToEmail[];

// Credit card fields.
extern const char kEcmlCardHolder[];
extern const char kEcmlCardType[];
extern const char kEcmlCardNumber[];
extern const char kEcmlCardVerification[];
extern const char kEcmlCardExpireDay[];
extern const char kEcmlCardExpireMonth[];
extern const char kEcmlCardExpireYear[];

// Note: ECML compliance checking has been modified to accommodate Google
// Checkout field name limitation. All ECML compliant web forms will be
// recognized correctly as such however the restrictions on having exactly
// ECML compliant names have been loosened to only require that field names
// be prefixed with an ECML compliant name in order to accommodate checkout.
// Additionally we allow the use of '.' as a word delimiter in addition to the
// ECML standard '_' (see FormField::FormField for details).
namespace autofill {

bool HasECMLField(const std::vector<AutofillField*>& fields);
string16 GetEcmlPattern(const char* ecml_name);
string16 GetEcmlPattern(const char* ecml_name1,
                        const char* ecml_name2,
                        char pattern_operator);
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_ECML_H_
