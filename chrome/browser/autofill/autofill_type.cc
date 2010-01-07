// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_type.h"

#include "base/basictypes.h"
#include "base/logging.h"

// TODO(jhawkins) all of the Human Readable names will need to be added to a
// resource file and localized because they are used in the confirmation
// dialog.
AutoFillType::AutoFillTypeDefinition kAutoFillTypeDefinitions[] = {
  { NO_SERVER_DATA, AutoFillType::NO_GROUP, AutoFillType::NO_SUBGROUP, "No Server Data" },
  { UNKNOWN_TYPE, AutoFillType::NO_GROUP, AutoFillType::NO_SUBGROUP, "Unknown Type" },
  { EMPTY_TYPE, AutoFillType::NO_GROUP, AutoFillType::NO_SUBGROUP, "Empty Type" },

  { NAME_FIRST, AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP, "First Name" },
  { NAME_MIDDLE, AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP, "Middle Name" },
  { NAME_LAST, AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP, "Last Name" },
  { NAME_MIDDLE_INITIAL, AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP, "Middle Initial" },
  { NAME_FULL, AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP, "Full Name" },
  { NAME_SUFFIX, AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP, "Name Suffix" },

  { EMAIL_ADDRESS, AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP, "Email Address" },
  { COMPANY_NAME, AutoFillType::CONTACT_INFO, AutoFillType::NO_SUBGROUP, "Company Name" },

  { PHONE_HOME_NUMBER, AutoFillType::PHONE_HOME, AutoFillType::PHONE_NUMBER, "Home Phone Number" },
  { PHONE_HOME_CITY_CODE, AutoFillType::PHONE_HOME, AutoFillType::PHONE_CITY_CODE, "Home Area Code" },
  { PHONE_HOME_COUNTRY_CODE, AutoFillType::PHONE_HOME, AutoFillType::PHONE_COUNTRY_CODE, "Home Country Code" },
  { PHONE_HOME_CITY_AND_NUMBER, AutoFillType::PHONE_HOME, AutoFillType::PHONE_CITY_AND_NUMBER, "Home Phone Number" },
  { PHONE_HOME_WHOLE_NUMBER, AutoFillType::PHONE_HOME, AutoFillType::PHONE_WHOLE_NUMBER, "Home Phone Number" },

  { PHONE_FAX_NUMBER, AutoFillType::PHONE_FAX, AutoFillType::PHONE_NUMBER,  "Fax Number" },
  { PHONE_FAX_CITY_CODE, AutoFillType::PHONE_FAX, AutoFillType::PHONE_CITY_CODE, "Fax Area Code" },
  { PHONE_FAX_COUNTRY_CODE, AutoFillType::PHONE_FAX, AutoFillType::PHONE_COUNTRY_CODE, "Fax Country Code" },
  { PHONE_FAX_CITY_AND_NUMBER, AutoFillType::PHONE_FAX, AutoFillType::PHONE_CITY_AND_NUMBER, "Fax Number" },
  { PHONE_FAX_WHOLE_NUMBER, AutoFillType::PHONE_FAX, AutoFillType::PHONE_WHOLE_NUMBER, "Fax Number" },

  { ADDRESS_HOME_LINE1, AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_LINE1, "Home Address Line 1" },
  { ADDRESS_HOME_LINE2, AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_LINE2, "Home Address Line 2" },
  { ADDRESS_HOME_APT_NUM, AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_APT_NUM, "Home Address Apartment Number" },
  { ADDRESS_HOME_CITY, AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_CITY,  "Home Address City" },
  { ADDRESS_HOME_STATE, AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_STATE, "Home Address State" },
  { ADDRESS_HOME_ZIP, AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_ZIP, "Home Address Zip Code" },
  { ADDRESS_HOME_COUNTRY, AutoFillType::ADDRESS_HOME, AutoFillType::ADDRESS_COUNTRY, "Home Address Country" },

  { ADDRESS_BILLING_LINE1, AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_LINE1, "Billing Address Line 1" },
  { ADDRESS_BILLING_LINE2, AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_LINE2, "Billing Address Line 2" },
  { ADDRESS_BILLING_APT_NUM, AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_APT_NUM, "Billing Address Apartment Number" },
  { ADDRESS_BILLING_CITY, AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_CITY,  "Billing Address City" },
  { ADDRESS_BILLING_STATE, AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_STATE, "Billing Address State" },
  { ADDRESS_BILLING_ZIP, AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_ZIP, "Billing Address Zip Code" },
  { ADDRESS_BILLING_COUNTRY, AutoFillType::ADDRESS_BILLING, AutoFillType::ADDRESS_COUNTRY, "Billing Address Country" },

  { CREDIT_CARD_NAME, AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP, "Name on Credit Card" },
  { CREDIT_CARD_NUMBER, AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP, "Credit Card Number" },
  { CREDIT_CARD_EXP_MONTH, AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP, "Credit Card Expiration Month" },
  { CREDIT_CARD_EXP_2_DIGIT_YEAR, AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP, "Credit Card Expiration Year" },
  { CREDIT_CARD_EXP_4_DIGIT_YEAR, AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP, "Credit Card Expiration Year" },
  { CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP, "Credit Card Expiration Date" },
  { CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP, "Credit Card Expiration Date" },
  { CREDIT_CARD_TYPE, AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP, "Credit Card Type" },
  { CREDIT_CARD_VERIFICATION_CODE, AutoFillType::CREDIT_CARD, AutoFillType::NO_SUBGROUP, "Credit Card Verification Number" },

  { MAX_VALID_FIELD_TYPE, AutoFillType::NO_GROUP, AutoFillType::NO_SUBGROUP, "" },
};

bool AutoFillType::initialized_ = false;
AutoFillType AutoFillType::types_[MAX_VALID_FIELD_TYPE + 1];

AutoFillType::AutoFillType(AutoFillTypeDefinition* definition)
    : autofill_type_definition_(definition) {
  DCHECK(definition != NULL);
}

AutoFillType::AutoFillType(AutoFillFieldType field_type) {
  StaticInitialize();
  DCHECK(initialized_);

  if (field_type < 0 || field_type > MAX_VALID_FIELD_TYPE ||
      types_[field_type].autofill_type_definition_ == NULL) {
    field_type = UNKNOWN_TYPE;
  }

  autofill_type_definition_ = types_[field_type].autofill_type_definition_;
}

// TODO(jhawkins): refactor this class to not require static initialization.
void AutoFillType::StaticInitialize() {
  // Can be called more than once (in unit tests for example).
  if (initialized_)
    return;

  initialized_ = true;

  InitializeFieldTypeMap();
}

void AutoFillType::InitializeFieldTypeMap() {
  for (size_t i = 0; i < arraysize(kAutoFillTypeDefinitions); ++i) {
    AutoFillType type(&kAutoFillTypeDefinitions[i]);
    types_[type.field_type()] = type;
  }
}
