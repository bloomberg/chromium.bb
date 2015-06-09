// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/form_field_error.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace autofill {
namespace wallet {

namespace {

FormFieldError::ErrorType ErrorTypeFromString(const std::string& error_type) {
  if (base::LowerCaseEqualsASCII(error_type, "unknown_error"))
    return FormFieldError::UNKNOWN_ERROR;
  if (base::LowerCaseEqualsASCII(error_type, "invalid_phone_number"))
    return FormFieldError::INVALID_PHONE_NUMBER;
  if (base::LowerCaseEqualsASCII(error_type, "invalid_postal_code"))
    return FormFieldError::INVALID_POSTAL_CODE;
  if (base::LowerCaseEqualsASCII(error_type, "invalid_address"))
    return FormFieldError::INVALID_ADDRESS;
  if (base::LowerCaseEqualsASCII(error_type, "invalid_card_details"))
    return FormFieldError::INVALID_CARD_DETAILS;
  if (base::LowerCaseEqualsASCII(error_type, "invalid_city"))
    return FormFieldError::INVALID_CITY;
  if (base::LowerCaseEqualsASCII(error_type, "invalid_instrument"))
    return FormFieldError::INVALID_INSTRUMENT;
  if (base::LowerCaseEqualsASCII(error_type, "invalid_state"))
    return FormFieldError::INVALID_STATE;
  if (base::LowerCaseEqualsASCII(error_type, "required_field_not_set"))
    return FormFieldError::REQUIRED_FIELD_NOT_SET;
  return FormFieldError::UNKNOWN_ERROR;
}

FormFieldError::Location LocationFromString(const std::string& location) {
  if (base::LowerCaseEqualsASCII(location, "unknown_location"))
    return FormFieldError::UNKNOWN_LOCATION;
  if (base::LowerCaseEqualsASCII(location, "payment_instrument"))
    return FormFieldError::PAYMENT_INSTRUMENT;
  if (base::LowerCaseEqualsASCII(location, "shipping_address"))
    return FormFieldError::SHIPPING_ADDRESS;
  if (base::LowerCaseEqualsASCII(location, "legal_address"))
    return FormFieldError::LEGAL_ADDRESS;
  return FormFieldError::UNKNOWN_LOCATION;
}

}  // namespace

FormFieldError::FormFieldError(ErrorType error_type, Location location)
  : error_type_(error_type),
    location_(location) {}

FormFieldError::~FormFieldError() {}

ServerFieldType FormFieldError::GetAutofillType() const {
  switch (error_type_) {
    case INVALID_PHONE_NUMBER:
      if (location_ == LEGAL_ADDRESS || location_ == PAYMENT_INSTRUMENT)
        return PHONE_BILLING_WHOLE_NUMBER;
      if (location_ == SHIPPING_ADDRESS)
        return PHONE_HOME_WHOLE_NUMBER;
      break;

    case INVALID_POSTAL_CODE:
    case INVALID_CITY:
    case INVALID_STATE:
      if (location_ == LEGAL_ADDRESS || location_ == PAYMENT_INSTRUMENT)
        return ADDRESS_BILLING_ZIP;
      if (location_ == SHIPPING_ADDRESS)
        return ADDRESS_HOME_ZIP;
      break;

    case INVALID_ADDRESS:
      if (location_ == LEGAL_ADDRESS || location_ == PAYMENT_INSTRUMENT)
        return ADDRESS_BILLING_LINE1;
      if (location_ == SHIPPING_ADDRESS)
        return ADDRESS_HOME_LINE1;
      break;

    case INVALID_CARD_DETAILS:
      return CREDIT_CARD_VERIFICATION_CODE;

    case INVALID_INSTRUMENT:
      return CREDIT_CARD_NUMBER;

    case REQUIRED_FIELD_NOT_SET:
    case UNKNOWN_ERROR:
      return MAX_VALID_FIELD_TYPE;
  }

  return MAX_VALID_FIELD_TYPE;
}

// TODO(ahutter): L10n after UX provides strings.
base::string16 FormFieldError::GetErrorMessage() const {
  switch (error_type_) {
    case INVALID_PHONE_NUMBER:
      return base::ASCIIToUTF16("Not a valid phone number");

    case INVALID_POSTAL_CODE:
      return base::ASCIIToUTF16("Not a valid zip code");

    case INVALID_CITY:
      return base::ASCIIToUTF16("Zip code is not valid for the entered city");

    case INVALID_STATE:
      return base::ASCIIToUTF16("Zip code is not valid for the entered state");

    case INVALID_ADDRESS:
      return base::ASCIIToUTF16("Not a valid street address");

    case INVALID_CARD_DETAILS:
      return base::ASCIIToUTF16("Not a valid CVN");

    case INVALID_INSTRUMENT:
      return base::ASCIIToUTF16("Not a valid CC#");

    case REQUIRED_FIELD_NOT_SET:
      return base::ASCIIToUTF16("Required field is missing");

    case UNKNOWN_ERROR:
      return base::ASCIIToUTF16("An unknown error occurred");
  }

  NOTREACHED();
  return base::string16();
}

// static
FormFieldError FormFieldError::CreateFormFieldError(
    const base::DictionaryValue& dictionary) {
  FormFieldError form_field_error(UNKNOWN_ERROR, UNKNOWN_LOCATION);

  std::string error_type;
  if (dictionary.GetString("type", &error_type))
    form_field_error.error_type_ = ErrorTypeFromString(error_type);

  std::string location;
  if (dictionary.GetString("location", &location))
    form_field_error.location_ = LocationFromString(location);

  return form_field_error;
}

bool FormFieldError::operator==(const FormFieldError& other) const {
  return error_type_ == other.error_type_ && location_ == other.location_;
}

}  // namespace wallet
}  // namespace autofill
