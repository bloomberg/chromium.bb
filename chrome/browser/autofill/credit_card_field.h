// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CREDIT_CARD_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_CREDIT_CARD_FIELD_H_
#pragma once

#include <vector>

#include "chrome/browser/autofill/form_field.h"

class AutofillField;

class CreditCardField : public FormField {
 public:
  // FormField implementation:
  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const;
  virtual FormFieldType GetFormFieldType() const;

  static CreditCardField* Parse(
      std::vector<AutofillField*>::const_iterator* iter,
      bool is_ecml);

 private:
  CreditCardField();

  AutofillField* cardholder_;  // Optional.

  // Occasionally pages have separate fields for the cardholder's first and
  // last names; for such pages cardholder_ holds the first name field and
  // we store the last name field here.
  // (We could store an embedded NameField object here, but we don't do so
  // because the text patterns for matching a cardholder name are different
  // than for ordinary names, and because cardholder names never have titles,
  // middle names or suffixes.)
  AutofillField* cardholder_last_;

  AutofillField* type_;  // Optional.  TODO(jhawkins): Parse the select control.
  AutofillField* number_;  // Required.

  // The 3-digit card verification number; we don't currently fill this.
  AutofillField* verification_;

  // Both required.  TODO(jhawkins): Parse the select control.
  AutofillField* expiration_month_;
  AutofillField* expiration_year_;

  DISALLOW_COPY_AND_ASSIGN(CreditCardField);
};

#endif  // CHROME_BROWSER_AUTOFILL_CREDIT_CARD_FIELD_H_
