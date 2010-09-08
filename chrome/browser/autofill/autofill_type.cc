// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_type.h"

#include "base/basictypes.h"

namespace {

const AutoFillType::AutoFillTypeDefinition kUnknownAutoFillTypeDefinition = {
  /* UNKNOWN_TYPE */ AutoFillType::NO_GROUP, AutoFillType::NO_SUBGROUP
};

AutoFillType::AutoFillTypeDefinition kAutoFillTypeDefinitions[] = {
  // NO_SERVER_DATA
  { AutoFillType::NO_GROUP, AutoFillType::NO_SUBGROUP },
  // UNKNOWN_TYPE
  kUnknownAutoFillTypeDefinition,
  // EMPTY_TYPE
  { AutoFillType::NO_GROUP, AutoFillType::NO_SUBGROUP },

  // NAME_FIRST
  { AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP },
  // NAME_MIDDLE
  { AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP },
  // NAME_LAST
  { AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP },
  // NAME_MIDDLE_INITIAL
  { AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP },
  // NAME_FULL
  { AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP },
  // NAME_SUFFIX
  { AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP },

  // EMAIL_ADDRESS
  { AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP },

  // PHONE_HOME_NUMBER
  { AutoFillType::PHONE_HOME, AutoFillType::PHONE_NUMBER },
  // PHONE_HOME_CITY_CODE
  { AutoFillType::PHONE_HOME, AutoFillType::PHONE_CITY_CODE },
  // PHONE_HOME_COUNTRY_CODE
  { AutoFillType::PHONE_HOME, AutoFillType::PHONE_COUNTRY_CODE },
  // PHONE_HOME_CITY_AND_NUMBER
  { AutoFillType::PHONE_HOME, AutoFillType::PHONE_CITY_AND_NUMBER },
  // PHONE_HOME_WHOLE_NUMBER
  { AutoFillType::PHONE_HOME, AutoFillType::PHONE_WHOLE_NUMBER },

  // Work phone numbers (values [15,19]) are deprecated.
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,

  // PHONE_FAX_NUMBER
  { AutoFillType::PHONE_FAX, AutoFillType::PHONE_NUMBER },
  // PHONE_FAX_CITY_CODE
  { AutoFillType::PHONE_FAX, AutoFillType::PHONE_CITY_CODE },
  // PHONE_FAX_COUNTRY_CODE
  { AutoFillType::PHONE_FAX, AutoFillType::PHONE_COUNTRY_CODE },
  // PHONE_FAX_CITY_AND_NUMBER
  { AutoFillType::PHONE_FAX, AutoFillType::PHONE_CITY_AND_NUMBER },
  // PHONE_FAX_WHOLE_NUMBER
  { AutoFillType::PHONE_FAX, AutoFillType::PHONE_WHOLE_NUMBER },

  // Cell phone numbers (values [25, 29]) are deprecated.
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,

  // ADDRESS_HOME_LINE1
  { AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_LINE1 },
  // ADDRESS_HOME_LINE2
  { AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_LINE2 },
  // ADDRESS_HOME_APT_NUM
  { AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_APT_NUM },
  // ADDRESS_HOME_CITY
  { AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_CITY },
  // ADDRESS_HOME_STATE
  { AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_STATE },
  // ADDRESS_HOME_ZIP
  { AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_ZIP },
  // ADDRESS_HOME_COUNTRY
  { AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_COUNTRY },

  // ADDRESS_BILLING_LINE1
  { AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_LINE1 },
  // ADDRESS_BILLING_LINE2
  { AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_LINE2 },
  // ADDRESS_BILLING_APT_NUM
  { AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_APT_NUM },
  // ADDRESS_BILLING_CITY
  { AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_CITY },
  // ADDRESS_BILLING_STATE
  { AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_STATE },
  // ADDRESS_BILLING_ZIP
  { AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_ZIP },
  // ADDRESS_BILLING_COUNTRY
  { AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_COUNTRY },

  // ADDRESS_SHIPPING values [44,50] are deprecated.
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,
  kUnknownAutoFillTypeDefinition,

  // CREDIT_CARD_NAME
  { AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP },
  // CREDIT_CARD_NUMBER
  { AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP },
  // CREDIT_CARD_EXP_MONTH
  { AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP },
  // CREDIT_CARD_EXP_2_DIGIT_YEAR
  { AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP },
  // CREDIT_CARD_EXP_4_DIGIT_YEAR
  { AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP },
  // CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
  { AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP },
  // CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR
  { AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP },
  // CREDIT_CARD_TYPE
  { AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP },
  // CREDIT_CARD_VERIFICATION_CODE
  { AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP },

  // COMPANY_NAME
  { AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP },
};

}  // namespace

AutoFillType::AutoFillType(AutoFillFieldType field_type) {
  if ((field_type < NO_SERVER_DATA || field_type >= MAX_VALID_FIELD_TYPE) ||
      (field_type >= 15 && field_type <= 19) ||
      (field_type >= 25 && field_type <= 29) ||
      (field_type >= 44 && field_type <= 50))
    field_type_ = UNKNOWN_TYPE;
  else
    field_type_ = field_type;
}

AutoFillType::AutoFillType(const AutoFillType& autofill_type) {
  *this = autofill_type;
}

AutoFillType& AutoFillType::operator=(const AutoFillType& autofill_type) {
  if (this != &autofill_type)
    this->field_type_ = autofill_type.field_type_;
  return *this;
}

AutoFillFieldType AutoFillType::field_type() const {
  return field_type_;
}

FieldTypeGroup AutoFillType::group() const {
  return kAutoFillTypeDefinitions[field_type_].group;
}

FieldTypeSubGroup AutoFillType::subgroup() const {
  return kAutoFillTypeDefinitions[field_type_].subgroup;
}
