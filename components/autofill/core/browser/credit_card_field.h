// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_FIELD_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_field.h"

namespace autofill {

class AutofillField;
class AutofillScanner;

class CreditCardField : public FormField {
 public:
  static FormField* Parse(AutofillScanner* scanner);

 protected:
  // FormField:
  virtual bool ClassifyField(ServerFieldTypeMap* map) const OVERRIDE;

 private:
  friend class CreditCardFieldTest;

  CreditCardField();

  const AutofillField* cardholder_;  // Optional.

  // Occasionally pages have separate fields for the cardholder's first and
  // last names; for such pages cardholder_ holds the first name field and
  // we store the last name field here.
  // (We could store an embedded NameField object here, but we don't do so
  // because the text patterns for matching a cardholder name are different
  // than for ordinary names, and because cardholder names never have titles,
  // middle names or suffixes.)
  const AutofillField* cardholder_last_;

  // TODO(jhawkins): Parse the select control.
  const AutofillField* type_;  // Optional.
  const AutofillField* number_;  // Required.

  // The 3-digit card verification number; we don't currently fill this.
  const AutofillField* verification_;

  // Either |expiration_date_| or both |expiration_month_| and
  // |expiration_year_| are required.
  const AutofillField* expiration_month_;
  const AutofillField* expiration_year_;
  const AutofillField* expiration_date_;

  // True if the year is detected to be a 2-digit year; otherwise, we assume
  // a 4-digit year.
  bool is_two_digit_year_;

  DISALLOW_COPY_AND_ASSIGN(CreditCardField);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_FIELD_H_
