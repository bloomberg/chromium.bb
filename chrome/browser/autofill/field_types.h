// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_FIELD_TYPES_H_
#define CHROME_BROWSER_AUTOFILL_FIELD_TYPES_H_
#pragma once

#include <set>

typedef enum _AddressType {
  kGenericAddress = 0,
  kBillingAddress,
  kShippingAddress
} AddressType;

// NOTE: This list MUST not be modified.  The server aggregates and stores these
// types over several versions, so we must remain fully compatible with the
// Autofill server, which is itself backward-compatible.  The list must be kept
// up to date with the Autofill server list.
//
// This is the list of all valid field types.
typedef enum _FieldType {
  // Server indication that it has no data for the requested field.
  NO_SERVER_DATA = 0,
  // Client indication that the text entered did not match anything in the
  // personal data.
  UNKNOWN_TYPE = 1,
  // The "empty" type indicates that the user hasn't entered anything
  // in this field.
  EMPTY_TYPE = 2,
  // Personal Information categorization types.
  NAME_FIRST = 3,
  NAME_MIDDLE = 4,
  NAME_LAST = 5,
  NAME_MIDDLE_INITIAL = 6,
  NAME_FULL = 7,
  NAME_SUFFIX = 8,
  EMAIL_ADDRESS = 9,
  PHONE_HOME_NUMBER = 10,
  PHONE_HOME_CITY_CODE = 11,
  PHONE_HOME_COUNTRY_CODE = 12,
  PHONE_HOME_CITY_AND_NUMBER = 13,
  PHONE_HOME_WHOLE_NUMBER = 14,

  // Work phone numbers (values [15,19]) are deprecated.

  PHONE_FAX_NUMBER = 20,
  PHONE_FAX_CITY_CODE = 21,
  PHONE_FAX_COUNTRY_CODE = 22,
  PHONE_FAX_CITY_AND_NUMBER = 23,
  PHONE_FAX_WHOLE_NUMBER = 24,

  // Cell phone numbers (values [25, 29]) are deprecated.

  ADDRESS_HOME_LINE1 = 30,
  ADDRESS_HOME_LINE2 = 31,
  ADDRESS_HOME_APT_NUM = 32,
  ADDRESS_HOME_CITY = 33,
  ADDRESS_HOME_STATE = 34,
  ADDRESS_HOME_ZIP = 35,
  ADDRESS_HOME_COUNTRY = 36,
  ADDRESS_BILLING_LINE1 = 37,
  ADDRESS_BILLING_LINE2 = 38,
  ADDRESS_BILLING_APT_NUM = 39,
  ADDRESS_BILLING_CITY = 40,
  ADDRESS_BILLING_STATE = 41,
  ADDRESS_BILLING_ZIP = 42,
  ADDRESS_BILLING_COUNTRY = 43,

  // ADDRESS_SHIPPING values [44,50] are deprecated.

  CREDIT_CARD_NAME = 51,
  CREDIT_CARD_NUMBER = 52,
  CREDIT_CARD_EXP_MONTH = 53,
  CREDIT_CARD_EXP_2_DIGIT_YEAR = 54,
  CREDIT_CARD_EXP_4_DIGIT_YEAR = 55,
  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR = 56,
  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR = 57,
  CREDIT_CARD_TYPE = 58,
  CREDIT_CARD_VERIFICATION_CODE = 59,

  COMPANY_NAME = 60,

  // No new types can be added.

  MAX_VALID_FIELD_TYPE = 61,
} AutofillFieldType;

typedef std::set<AutofillFieldType> FieldTypeSet;

#endif  // CHROME_BROWSER_AUTOFILL_FIELD_TYPES_H_
