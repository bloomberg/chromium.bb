// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_type.h"

#include <ostream>

#include "base/logging.h"

namespace {

const AutofillType::AutofillTypeDefinition kUnknownAutofillTypeDefinition = {
  /* UNKNOWN_TYPE */ AutofillType::NO_GROUP, AutofillType::NO_SUBGROUP
};

AutofillType::AutofillTypeDefinition kAutofillTypeDefinitions[] = {
  // NO_SERVER_DATA
  { AutofillType::NO_GROUP, AutofillType::NO_SUBGROUP },
  // UNKNOWN_TYPE
  kUnknownAutofillTypeDefinition,
  // EMPTY_TYPE
  { AutofillType::NO_GROUP, AutofillType::NO_SUBGROUP },

  // NAME_FIRST
  { AutofillType::NAME, AutofillType::NO_SUBGROUP },
  // NAME_MIDDLE
  { AutofillType::NAME, AutofillType::NO_SUBGROUP },
  // NAME_LAST
  { AutofillType::NAME, AutofillType::NO_SUBGROUP },
  // NAME_MIDDLE_INITIAL
  { AutofillType::NAME, AutofillType::NO_SUBGROUP },
  // NAME_FULL
  { AutofillType::NAME, AutofillType::NO_SUBGROUP },
  // NAME_SUFFIX
  { AutofillType::NAME, AutofillType::NO_SUBGROUP },

  // EMAIL_ADDRESS
  { AutofillType::EMAIL, AutofillType::NO_SUBGROUP },

  // PHONE_HOME_NUMBER
  { AutofillType::PHONE_HOME, AutofillType::PHONE_NUMBER },
  // PHONE_HOME_CITY_CODE
  { AutofillType::PHONE_HOME, AutofillType::PHONE_CITY_CODE },
  // PHONE_HOME_COUNTRY_CODE
  { AutofillType::PHONE_HOME, AutofillType::PHONE_COUNTRY_CODE },
  // PHONE_HOME_CITY_AND_NUMBER
  { AutofillType::PHONE_HOME, AutofillType::PHONE_CITY_AND_NUMBER },
  // PHONE_HOME_WHOLE_NUMBER
  { AutofillType::PHONE_HOME, AutofillType::PHONE_WHOLE_NUMBER },

  // Work phone numbers (values [15,19]) are deprecated.
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,

  // PHONE_FAX_NUMBER
  { AutofillType::PHONE_FAX, AutofillType::PHONE_NUMBER },
  // PHONE_FAX_CITY_CODE
  { AutofillType::PHONE_FAX, AutofillType::PHONE_CITY_CODE },
  // PHONE_FAX_COUNTRY_CODE
  { AutofillType::PHONE_FAX, AutofillType::PHONE_COUNTRY_CODE },
  // PHONE_FAX_CITY_AND_NUMBER
  { AutofillType::PHONE_FAX, AutofillType::PHONE_CITY_AND_NUMBER },
  // PHONE_FAX_WHOLE_NUMBER
  { AutofillType::PHONE_FAX, AutofillType::PHONE_WHOLE_NUMBER },

  // Cell phone numbers (values [25, 29]) are deprecated.
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,

  // ADDRESS_HOME_LINE1
  { AutofillType::ADDRESS_HOME, AutofillType::ADDRESS_LINE1 },
  // ADDRESS_HOME_LINE2
  { AutofillType::ADDRESS_HOME, AutofillType::ADDRESS_LINE2 },
  // ADDRESS_HOME_APT_NUM
  { AutofillType::ADDRESS_HOME, AutofillType::ADDRESS_APT_NUM },
  // ADDRESS_HOME_CITY
  { AutofillType::ADDRESS_HOME, AutofillType::ADDRESS_CITY },
  // ADDRESS_HOME_STATE
  { AutofillType::ADDRESS_HOME, AutofillType::ADDRESS_STATE },
  // ADDRESS_HOME_ZIP
  { AutofillType::ADDRESS_HOME, AutofillType::ADDRESS_ZIP },
  // ADDRESS_HOME_COUNTRY
  { AutofillType::ADDRESS_HOME, AutofillType::ADDRESS_COUNTRY },

  // ADDRESS_BILLING_LINE1
  { AutofillType::ADDRESS_BILLING, AutofillType::ADDRESS_LINE1 },
  // ADDRESS_BILLING_LINE2
  { AutofillType::ADDRESS_BILLING, AutofillType::ADDRESS_LINE2 },
  // ADDRESS_BILLING_APT_NUM
  { AutofillType::ADDRESS_BILLING, AutofillType::ADDRESS_APT_NUM },
  // ADDRESS_BILLING_CITY
  { AutofillType::ADDRESS_BILLING, AutofillType::ADDRESS_CITY },
  // ADDRESS_BILLING_STATE
  { AutofillType::ADDRESS_BILLING, AutofillType::ADDRESS_STATE },
  // ADDRESS_BILLING_ZIP
  { AutofillType::ADDRESS_BILLING, AutofillType::ADDRESS_ZIP },
  // ADDRESS_BILLING_COUNTRY
  { AutofillType::ADDRESS_BILLING, AutofillType::ADDRESS_COUNTRY },

  // ADDRESS_SHIPPING values [44,50] are deprecated.
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,
  kUnknownAutofillTypeDefinition,

  // CREDIT_CARD_NAME
  { AutofillType::CREDIT_CARD, AutofillType::NO_SUBGROUP },
  // CREDIT_CARD_NUMBER
  { AutofillType::CREDIT_CARD, AutofillType::NO_SUBGROUP },
  // CREDIT_CARD_EXP_MONTH
  { AutofillType::CREDIT_CARD, AutofillType::NO_SUBGROUP },
  // CREDIT_CARD_EXP_2_DIGIT_YEAR
  { AutofillType::CREDIT_CARD, AutofillType::NO_SUBGROUP },
  // CREDIT_CARD_EXP_4_DIGIT_YEAR
  { AutofillType::CREDIT_CARD, AutofillType::NO_SUBGROUP },
  // CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
  { AutofillType::CREDIT_CARD, AutofillType::NO_SUBGROUP },
  // CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR
  { AutofillType::CREDIT_CARD, AutofillType::NO_SUBGROUP },
  // CREDIT_CARD_TYPE
  { AutofillType::CREDIT_CARD, AutofillType::NO_SUBGROUP },
  // CREDIT_CARD_VERIFICATION_CODE
  { AutofillType::CREDIT_CARD, AutofillType::NO_SUBGROUP },

  // COMPANY_NAME
  { AutofillType::COMPANY, AutofillType::NO_SUBGROUP },
};

}  // namespace

AutofillType::AutofillType(AutofillFieldType field_type) {
  if ((field_type < NO_SERVER_DATA || field_type >= MAX_VALID_FIELD_TYPE) ||
      (field_type >= 15 && field_type <= 19) ||
      (field_type >= 25 && field_type <= 29) ||
      (field_type >= 44 && field_type <= 50))
    field_type_ = UNKNOWN_TYPE;
  else
    field_type_ = field_type;
}

AutofillType::AutofillType(const AutofillType& autofill_type) {
  *this = autofill_type;
}

AutofillType& AutofillType::operator=(const AutofillType& autofill_type) {
  if (this != &autofill_type)
    this->field_type_ = autofill_type.field_type_;
  return *this;
}

AutofillFieldType AutofillType::field_type() const {
  return field_type_;
}

FieldTypeGroup AutofillType::group() const {
  return kAutofillTypeDefinitions[field_type_].group;
}

FieldTypeSubGroup AutofillType::subgroup() const {
  return kAutofillTypeDefinitions[field_type_].subgroup;
}

// static
AutofillFieldType AutofillType::GetEquivalentFieldType(
    AutofillFieldType field_type) {
  // When billing information is requested from the profile we map to the
  // home address equivalents.
  switch (field_type) {
    case ADDRESS_BILLING_LINE1:
      return ADDRESS_HOME_LINE1;

    case ADDRESS_BILLING_LINE2:
      return ADDRESS_HOME_LINE2;

    case ADDRESS_BILLING_APT_NUM:
      return ADDRESS_HOME_APT_NUM;

    case ADDRESS_BILLING_CITY:
      return ADDRESS_HOME_CITY;

    case ADDRESS_BILLING_STATE:
      return ADDRESS_HOME_STATE;

    case ADDRESS_BILLING_ZIP:
      return ADDRESS_HOME_ZIP;

    case ADDRESS_BILLING_COUNTRY:
      return ADDRESS_HOME_COUNTRY;

    default:
      return field_type;
  }
}

// static
std::string AutofillType::FieldTypeToString(AutofillFieldType type) {
  switch (type) {
    case NO_SERVER_DATA:
      return "NO_SERVER_DATA";
    case UNKNOWN_TYPE:
      return "UNKNOWN_TYPE";
    case EMPTY_TYPE:
      return "EMPTY_TYPE";
    case NAME_FIRST:
      return "NAME_FIRST";
    case NAME_MIDDLE:
      return "NAME_MIDDLE";
    case NAME_LAST:
      return "NAME_LAST";
    case NAME_MIDDLE_INITIAL:
      return "NAME_MIDDLE_INITIAL";
    case NAME_FULL:
      return "NAME_FULL";
    case NAME_SUFFIX:
      return "NAME_SUFFIX";
    case EMAIL_ADDRESS:
      return "EMAIL_ADDRESS";
    case PHONE_HOME_NUMBER:
      return "PHONE_HOME_NUMBER";
    case PHONE_HOME_CITY_CODE:
      return "PHONE_HOME_CITY_CODE";
    case PHONE_HOME_COUNTRY_CODE:
      return "PHONE_HOME_COUNTRY_CODE";
    case PHONE_HOME_CITY_AND_NUMBER:
      return "PHONE_HOME_CITY_AND_NUMBER";
    case PHONE_HOME_WHOLE_NUMBER:
      return "PHONE_HOME_WHOLE_NUMBER";
    case PHONE_FAX_NUMBER:
      return "PHONE_FAX_NUMBER";
    case PHONE_FAX_CITY_CODE:
      return "PHONE_FAX_CITY_CODE";
    case PHONE_FAX_COUNTRY_CODE:
      return "PHONE_FAX_COUNTRY_CODE";
    case PHONE_FAX_CITY_AND_NUMBER:
      return "PHONE_FAX_CITY_AND_NUMBER";
    case PHONE_FAX_WHOLE_NUMBER:
      return "PHONE_FAX_WHOLE_NUMBER";
    case ADDRESS_HOME_LINE1:
      return "ADDRESS_HOME_LINE1";
    case ADDRESS_HOME_LINE2:
      return "ADDRESS_HOME_LINE2";
    case ADDRESS_HOME_APT_NUM:
      return "ADDRESS_HOME_APT_NUM";
    case ADDRESS_HOME_CITY:
      return "ADDRESS_HOME_CITY";
    case ADDRESS_HOME_STATE:
      return "ADDRESS_HOME_STATE";
    case ADDRESS_HOME_ZIP:
      return "ADDRESS_HOME_ZIP";
    case ADDRESS_HOME_COUNTRY:
      return "ADDRESS_HOME_COUNTRY";
    case ADDRESS_BILLING_LINE1:
      return "ADDRESS_BILLING_LINE1";
    case ADDRESS_BILLING_LINE2:
      return "ADDRESS_BILLING_LINE2";
    case ADDRESS_BILLING_APT_NUM:
      return "ADDRESS_BILLING_APT_NUM";
    case ADDRESS_BILLING_CITY:
      return "ADDRESS_BILLING_CITY";
    case ADDRESS_BILLING_STATE:
      return "ADDRESS_BILLING_STATE";
    case ADDRESS_BILLING_ZIP:
      return "ADDRESS_BILLING_ZIP";
    case ADDRESS_BILLING_COUNTRY:
      return "ADDRESS_BILLING_COUNTRY";
    case CREDIT_CARD_NAME:
      return "CREDIT_CARD_NAME";
    case CREDIT_CARD_NUMBER:
      return "CREDIT_CARD_NUMBER";
    case CREDIT_CARD_EXP_MONTH:
      return "CREDIT_CARD_EXP_MONTH";
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return "CREDIT_CARD_EXP_2_DIGIT_YEAR";
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return "CREDIT_CARD_EXP_4_DIGIT_YEAR";
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      return "CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR";
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return "CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR";
    case CREDIT_CARD_TYPE:
      return "CREDIT_CARD_TYPE";
    case CREDIT_CARD_VERIFICATION_CODE:
      return "CREDIT_CARD_VERIFICATION_CODE";
    case COMPANY_NAME:
      return "COMPANY_NAME";
    default:
      NOTREACHED() << "Invalid AutofillFieldType value.";
  }
  return std::string();
}

// static
AutofillFieldType AutofillType::StringToFieldType(const std::string& str) {
  if (str == "NO_SERVER_DATA")
    return NO_SERVER_DATA;
  if (str == "UNKNOWN_TYPE")
    return UNKNOWN_TYPE;
  if (str == "EMPTY_TYPE")
    return EMPTY_TYPE;
  if (str == "NAME_FIRST")
    return NAME_FIRST;
  if (str == "NAME_MIDDLE")
    return NAME_MIDDLE;
  if (str == "NAME_LAST")
    return NAME_LAST;
  if (str == "NAME_MIDDLE_INITIAL")
    return NAME_MIDDLE_INITIAL;
  if (str == "NAME_FULL")
    return NAME_FULL;
  if (str == "NAME_SUFFIX")
    return NAME_SUFFIX;
  if (str == "EMAIL_ADDRESS")
    return EMAIL_ADDRESS;
  if (str == "PHONE_HOME_NUMBER")
    return PHONE_HOME_NUMBER;
  if (str == "PHONE_HOME_CITY_CODE")
    return PHONE_HOME_CITY_CODE;
  if (str == "PHONE_HOME_COUNTRY_CODE")
    return PHONE_HOME_COUNTRY_CODE;
  if (str == "PHONE_HOME_CITY_AND_NUMBER")
    return PHONE_HOME_CITY_AND_NUMBER;
  if (str == "PHONE_HOME_WHOLE_NUMBER")
    return PHONE_HOME_WHOLE_NUMBER;
  if (str == "PHONE_FAX_NUMBER")
    return PHONE_FAX_NUMBER;
  if (str == "PHONE_FAX_CITY_CODE")
    return PHONE_FAX_CITY_CODE;
  if (str == "PHONE_FAX_COUNTRY_CODE")
    return PHONE_FAX_COUNTRY_CODE;
  if (str == "PHONE_FAX_CITY_AND_NUMBER")
    return PHONE_FAX_CITY_AND_NUMBER;
  if (str == "PHONE_FAX_WHOLE_NUMBER")
    return PHONE_FAX_WHOLE_NUMBER;
  if (str == "ADDRESS_HOME_LINE1")
    return ADDRESS_HOME_LINE1;
  if (str == "ADDRESS_HOME_LINE2")
    return ADDRESS_HOME_LINE2;
  if (str == "ADDRESS_HOME_APT_NUM")
    return ADDRESS_HOME_APT_NUM;
  if (str == "ADDRESS_HOME_CITY")
    return ADDRESS_HOME_CITY;
  if (str == "ADDRESS_HOME_STATE")
    return ADDRESS_HOME_STATE;
  if (str == "ADDRESS_HOME_ZIP")
    return ADDRESS_HOME_ZIP;
  if (str == "ADDRESS_HOME_COUNTRY")
    return ADDRESS_HOME_COUNTRY;
  if (str == "ADDRESS_BILLING_LINE1")
    return ADDRESS_BILLING_LINE1;
  if (str == "ADDRESS_BILLING_LINE2")
    return ADDRESS_BILLING_LINE2;
  if (str == "ADDRESS_BILLING_APT_NUM")
    return ADDRESS_BILLING_APT_NUM;
  if (str == "ADDRESS_BILLING_CITY")
    return ADDRESS_BILLING_CITY;
  if (str == "ADDRESS_BILLING_STATE")
    return ADDRESS_BILLING_STATE;
  if (str == "ADDRESS_BILLING_ZIP")
    return ADDRESS_BILLING_ZIP;
  if (str == "ADDRESS_BILLING_COUNTRY")
    return ADDRESS_BILLING_COUNTRY;
  if (str == "CREDIT_CARD_NAME")
    return CREDIT_CARD_NAME;
  if (str == "CREDIT_CARD_NUMBER")
    return CREDIT_CARD_NUMBER;
  if (str == "CREDIT_CARD_EXP_MONTH")
    return CREDIT_CARD_EXP_MONTH;
  if (str == "CREDIT_CARD_EXP_2_DIGIT_YEAR")
    return CREDIT_CARD_EXP_2_DIGIT_YEAR;
  if (str == "CREDIT_CARD_EXP_4_DIGIT_YEAR")
    return CREDIT_CARD_EXP_4_DIGIT_YEAR;
  if (str == "CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR")
    return CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
  if (str == "CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR")
    return CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;
  if (str == "CREDIT_CARD_TYPE")
    return CREDIT_CARD_TYPE;
  if (str == "CREDIT_CARD_VERIFICATION_CODE")
    return CREDIT_CARD_VERIFICATION_CODE;
  if (str == "COMPANY_NAME")
    return COMPANY_NAME;

  NOTREACHED() << "Unknown AutofillFieldType " << str;
  return UNKNOWN_TYPE;
}
