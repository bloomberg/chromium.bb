// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_ecml.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_field.h"

// Field names from the ECML specification; see RFC 3106.  We've
// made these names lowercase since we convert labels and field names to
// lowercase before searching.

// Shipping fields.
const char kEcmlShipToTitle[] = "ecom_shipto_postal_name_prefix";
const char kEcmlShipToFirstName[] = "ecom_shipto_postal_name_first";
const char kEcmlShipToMiddleName[] = "ecom_shipto_postal_name_middle";
const char kEcmlShipToLastName[] = "ecom_shipto_postal_name_last";
const char kEcmlShipToNameSuffix[] = "ecom_shipto_postal_name_suffix";
const char kEcmlShipToCompanyName[] = "ecom_shipto_postal_company";
const char kEcmlShipToAddress1[] = "ecom_shipto_postal_street_line1";
const char kEcmlShipToAddress2[] = "ecom_shipto_postal_street_line2";
const char kEcmlShipToAddress3[] = "ecom_shipto_postal_street_line3";
const char kEcmlShipToCity[] = "ecom_shipto_postal_city";
const char kEcmlShipToStateProv[] = "ecom_shipto_postal_stateprov";
const char kEcmlShipToPostalCode[] = "ecom_shipto_postal_postalcode";
const char kEcmlShipToCountry[] = "ecom_shipto_postal_countrycode";
const char kEcmlShipToPhone[] = "ecom_shipto_telecom_phone_number";
const char kEcmlShipToEmail[] = "ecom_shipto_online_email";

// Billing fields.
const char kEcmlBillToTitle[] = "ecom_billto_postal_name_prefix";
const char kEcmlBillToFirstName[] = "ecom_billto_postal_name_first";
const char kEcmlBillToMiddleName[] = "ecom_billto_postal_name_middle";
const char kEcmlBillToLastName[] = "ecom_billto_postal_name_last";
const char kEcmlBillToNameSuffix[] = "ecom_billto_postal_name_suffix";
const char kEcmlBillToCompanyName[] = "ecom_billto_postal_company";
const char kEcmlBillToAddress1[] = "ecom_billto_postal_street_line1";
const char kEcmlBillToAddress2[] = "ecom_billto_postal_street_line2";
const char kEcmlBillToAddress3[] = "ecom_billto_postal_street_line3";
const char kEcmlBillToCity[] = "ecom_billto_postal_city";
const char kEcmlBillToStateProv[] = "ecom_billto_postal_stateprov";
const char kEcmlBillToPostalCode[] = "ecom_billto_postal_postalcode";
const char kEcmlBillToCountry[] = "ecom_billto_postal_countrycode";
const char kEcmlBillToPhone[] = "ecom_billto_telecom_phone_number";
const char kEcmlBillToEmail[] = "ecom_billto_online_email";

// Credit card fields.
const char kEcmlCardHolder[] = "ecom_payment_card_name";
const char kEcmlCardType[] = "ecom_payment_card_type";
const char kEcmlCardNumber[] = "ecom_payment_card_number";
const char kEcmlCardVerification[] = "ecom_payment_card_verification";
const char kEcmlCardExpireDay[] = "ecom_payment_card_expdate_day";
const char kEcmlCardExpireMonth[] = "ecom_payment_card_expdate_month";
const char kEcmlCardExpireYear[] = "ecom_payment_card_expdate_year";

namespace autofill {

// Checks if any of the |form|'s labels are named according to the ECML
// standard.  Returns true if at least one ECML named element is found.
bool HasECMLField(const std::vector<AutofillField*>& fields) {
  struct EcmlField {
    const char* name_;
    const int length_;
  } ecml_fields[] = {
#define ECML_STRING_ENTRY(x) { x, arraysize(x) - 1 },
    ECML_STRING_ENTRY(kEcmlShipToTitle)
    ECML_STRING_ENTRY(kEcmlShipToFirstName)
    ECML_STRING_ENTRY(kEcmlShipToMiddleName)
    ECML_STRING_ENTRY(kEcmlShipToLastName)
    ECML_STRING_ENTRY(kEcmlShipToNameSuffix)
    ECML_STRING_ENTRY(kEcmlShipToCompanyName)
    ECML_STRING_ENTRY(kEcmlShipToAddress1)
    ECML_STRING_ENTRY(kEcmlShipToAddress2)
    ECML_STRING_ENTRY(kEcmlShipToAddress3)
    ECML_STRING_ENTRY(kEcmlShipToCity)
    ECML_STRING_ENTRY(kEcmlShipToStateProv)
    ECML_STRING_ENTRY(kEcmlShipToPostalCode)
    ECML_STRING_ENTRY(kEcmlShipToCountry)
    ECML_STRING_ENTRY(kEcmlShipToPhone)
    ECML_STRING_ENTRY(kEcmlShipToEmail)
    ECML_STRING_ENTRY(kEcmlBillToTitle)
    ECML_STRING_ENTRY(kEcmlBillToFirstName)
    ECML_STRING_ENTRY(kEcmlBillToMiddleName)
    ECML_STRING_ENTRY(kEcmlBillToLastName)
    ECML_STRING_ENTRY(kEcmlBillToNameSuffix)
    ECML_STRING_ENTRY(kEcmlBillToCompanyName)
    ECML_STRING_ENTRY(kEcmlBillToAddress1)
    ECML_STRING_ENTRY(kEcmlBillToAddress2)
    ECML_STRING_ENTRY(kEcmlBillToAddress3)
    ECML_STRING_ENTRY(kEcmlBillToCity)
    ECML_STRING_ENTRY(kEcmlBillToStateProv)
    ECML_STRING_ENTRY(kEcmlBillToPostalCode)
    ECML_STRING_ENTRY(kEcmlBillToCountry)
    ECML_STRING_ENTRY(kEcmlBillToPhone)
    ECML_STRING_ENTRY(kEcmlBillToPhone)
    ECML_STRING_ENTRY(kEcmlBillToEmail)
    ECML_STRING_ENTRY(kEcmlCardHolder)
    ECML_STRING_ENTRY(kEcmlCardType)
    ECML_STRING_ENTRY(kEcmlCardNumber)
    ECML_STRING_ENTRY(kEcmlCardVerification)
    ECML_STRING_ENTRY(kEcmlCardExpireMonth)
    ECML_STRING_ENTRY(kEcmlCardExpireYear)
#undef ECML_STRING_ENTRY
  };

  const string16 ecom(ASCIIToUTF16("ecom"));
  for (std::vector<AutofillField*>::const_iterator field = fields.begin();
       field != fields.end();
       ++field) {
    const string16& utf16_name = (*field)->name;
    if (StartsWith(utf16_name, ecom, true)) {
      std::string name(UTF16ToUTF8(utf16_name));
      for (size_t i = 0; i < ARRAYSIZE_UNSAFE(ecml_fields); ++i) {
        if (base::strncasecmp(name.c_str(), ecml_fields[i].name_,
                              ecml_fields[i].length_) == 0) {
          return true;
        }
      }
    }
  }

  return false;
}

string16 GetEcmlPattern(const char* ecml_name) {
  return ASCIIToUTF16(std::string("^") + ecml_name);
}

string16 GetEcmlPattern(const char* ecml_name1,
                        const char* ecml_name2,
                        char pattern_operator) {
  return ASCIIToUTF16(StringPrintf("^%s%c^%s",
                                   ecml_name1, pattern_operator, ecml_name2));
}

}  // namespace autofill
