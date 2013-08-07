// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_WALLET_FORM_FIELD_ERROR_H_
#define COMPONENTS_AUTOFILL_BROWSER_WALLET_FORM_FIELD_ERROR_H_

#include "base/strings/string16.h"
#include "components/autofill/core/browser/field_types.h"

namespace base {
class DictionaryValue;
}

namespace autofill {
namespace wallet {

// Class for representing a single Wallet server side validation error.
class FormFieldError {
 public:
  // The validation error returned from the server.
  enum ErrorType {
    UNKNOWN_ERROR,
    INVALID_PHONE_NUMBER,
    INVALID_POSTAL_CODE,
    // Bad street address.
    INVALID_ADDRESS,
    // Bad CVC.
    INVALID_CARD_DETAILS,
    // Wallet sends this when ZIP is invalid for the given city.
    INVALID_CITY,
    // Catch-all for many errors. E.g., no address given, no address ID,
    // invalid card number. Wallet should only send us this error for invalid
    // card number.
    INVALID_INSTRUMENT,
    // Wallet sends this when ZIP is invalid for the given state.
    INVALID_STATE,
    REQUIRED_FIELD_NOT_SET,
    // TODO(ahutter): Add INVALID_COUNTRY when user can select country in the
    // chooser.
  };

  // The section of the "form" where the error occurred.
  enum Location {
    UNKNOWN_LOCATION,
    PAYMENT_INSTRUMENT,
    SHIPPING_ADDRESS,
    // Currently Sugar uses the billing address as user's legal address. So any
    // error in billing address will be accompanied by an error in legal
    // address. The client side should map LEGAL_ADDRESS to the billing address.
    // This will ensure compatibility in case Sugar starts having a separate
    // legal address form.
    LEGAL_ADDRESS,
  };

  FormFieldError(ErrorType error_type, Location location);
  ~FormFieldError();

  ErrorType error_type() const { return error_type_; }
  Location location() const { return location_; }

  // Gets the appropriate field type for |location| and |error_type|.
  ServerFieldType GetAutofillType() const;

  // Gets a user facing error message appropriate for |location| and
  // |error_type|.
  base::string16 GetErrorMessage() const;

  // Creates an instance of FormFieldError from the input dictionary.
  static FormFieldError CreateFormFieldError(
      const base::DictionaryValue& dictionary);

  bool operator==(const FormFieldError& other) const;

 private:
  // The type of error as defined by the Wallet server.
  ErrorType error_type_;

  // The location of the error as defined by the Wallet server.
  Location location_;

  // This class is intentionally copyable and assignable.
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_WALLET_FORM_FIELD_ERROR_H_
