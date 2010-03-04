// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_common_unittest.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"

namespace autofill_unittest {

void SetProfileInfo(AutoFillProfile* profile,
    const char* label, const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone, const char* fax) {
  profile->set_label(ASCIIToUTF16(label));
  profile->SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16(first_name));
  profile->SetInfo(AutoFillType(NAME_MIDDLE), ASCIIToUTF16(middle_name));
  profile->SetInfo(AutoFillType(NAME_LAST), ASCIIToUTF16(last_name));
  profile->SetInfo(AutoFillType(EMAIL_ADDRESS), ASCIIToUTF16(email));
  profile->SetInfo(AutoFillType(COMPANY_NAME), ASCIIToUTF16(company));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_LINE1), ASCIIToUTF16(address1));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_LINE2), ASCIIToUTF16(address2));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_CITY), ASCIIToUTF16(city));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_STATE), ASCIIToUTF16(state));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_ZIP), ASCIIToUTF16(zipcode));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16(country));
  profile->SetInfo(AutoFillType(PHONE_HOME_NUMBER), ASCIIToUTF16(phone));
  profile->SetInfo(AutoFillType(PHONE_FAX_NUMBER), ASCIIToUTF16(fax));
}

void SetCreditCardInfo(CreditCard* credit_card,
    const char* label, const char* name_on_card, const char* type,
    const char* card_number, const char* expiration_month,
    const char* expiration_year, const char* verification_code,
    const char* billing_address, const char* shipping_address) {
  credit_card->set_label(ASCIIToUTF16(label));
  credit_card->SetInfo(AutoFillType(CREDIT_CARD_NAME),
      ASCIIToUTF16(name_on_card));
  credit_card->SetInfo(AutoFillType(CREDIT_CARD_TYPE), ASCIIToUTF16(type));
  credit_card->SetInfo(AutoFillType(CREDIT_CARD_NUMBER),
      ASCIIToUTF16(card_number));
  credit_card->SetInfo(AutoFillType(CREDIT_CARD_EXP_MONTH),
      ASCIIToUTF16(expiration_month));
  credit_card->SetInfo(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
      ASCIIToUTF16(expiration_year));
  credit_card->SetInfo(AutoFillType(CREDIT_CARD_VERIFICATION_CODE),
      ASCIIToUTF16(verification_code));
  credit_card->set_billing_address(ASCIIToUTF16(billing_address));
  credit_card->set_shipping_address(ASCIIToUTF16(shipping_address));
}

}  // namespace
