// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/autofill_type.h"

#include <ostream>

#include "base/logging.h"

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
  switch (field_type_) {
    case NAME_FIRST:
    case NAME_MIDDLE:
    case NAME_LAST:
    case NAME_MIDDLE_INITIAL:
    case NAME_FULL:
    case NAME_SUFFIX:
      return NAME;

    case EMAIL_ADDRESS:
      return EMAIL;

    case PHONE_HOME_NUMBER:
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_COUNTRY_CODE:
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_WHOLE_NUMBER:
      return PHONE;

    case ADDRESS_HOME_LINE1:
    case ADDRESS_HOME_LINE2:
    case ADDRESS_HOME_APT_NUM:
    case ADDRESS_HOME_CITY:
    case ADDRESS_HOME_STATE:
    case ADDRESS_HOME_ZIP:
    case ADDRESS_HOME_COUNTRY:
      return ADDRESS_HOME;

    case ADDRESS_BILLING_LINE1:
    case ADDRESS_BILLING_LINE2:
    case ADDRESS_BILLING_APT_NUM:
    case ADDRESS_BILLING_CITY:
    case ADDRESS_BILLING_STATE:
    case ADDRESS_BILLING_ZIP:
    case ADDRESS_BILLING_COUNTRY:
      return ADDRESS_BILLING;

    case CREDIT_CARD_NAME:
    case CREDIT_CARD_NUMBER:
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case CREDIT_CARD_TYPE:
    case CREDIT_CARD_VERIFICATION_CODE:
      return CREDIT_CARD;

    case COMPANY_NAME:
      return COMPANY;

    default:
      return NO_GROUP;
  }
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
