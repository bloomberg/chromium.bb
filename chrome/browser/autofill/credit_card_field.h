// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CREDIT_CARD_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_CREDIT_CARD_FIELD_H_
#pragma once

#include <vector>

#include "chrome/browser/autofill/form_field.h"

class AutoFillField;

class CreditCardField : public FormField {
 public:
  // FormField implementation:
  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const;
  virtual FormFieldType GetFormFieldType() const { return kCreditCardType; }

  static CreditCardField* Parse(
      std::vector<AutoFillField*>::const_iterator* iter,
      bool is_ecml);

 private:
  CreditCardField();

  AutoFillField* cardholder_;  // Optional.

  // Occasionally pages have separate fields for the cardholder's first and
  // last names; for such pages cardholder_ holds the first name field and
  // we store the last name field here.
  // (We could store an embedded NameField object here, but we don't do so
  // because the text patterns for matching a cardholder name are different
  // than for ordinary names, and because cardholder names never have titles,
  // middle names or suffixes.)
  AutoFillField* cardholder_last_;

  AutoFillField* type_;  // Optional.  TODO(jhawkins): Parse the select control.
  AutoFillField* number_;  // Required.

  // Both required.  TODO(jhawkins): Parse the select control.
  AutoFillField* expiration_month_;
  AutoFillField* expiration_year_;

  DISALLOW_COPY_AND_ASSIGN(CreditCardField);
};

#endif  // CHROME_BROWSER_AUTOFILL_CREDIT_CARD_FIELD_H_
