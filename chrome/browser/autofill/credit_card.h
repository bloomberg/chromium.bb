// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CREDIT_CARD_H_
#define CHROME_BROWSER_AUTOFILL_CREDIT_CARD_H_

#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/form_group.h"

// A form group that stores credit card information.
class CreditCard : public FormGroup {
 public:
  explicit CreditCard(const string16& label);

  // FormGroup implementation:
  FormGroup* Clone() const;
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
  virtual void FindInfoMatches(const AutoFillType& type,
                               const string16& info,
                               std::vector<string16>* matched_text) const;
  virtual string16 GetFieldText(const AutoFillType& type) const;
  virtual string16 GetPreviewText(const AutoFillType& type) const;
  virtual void SetInfo(const AutoFillType& type, const string16& value);
  string16 Label() const { return label_; }

  // The month and year are zero if not present.
  int Expiration4DigitYear() const { return expiration_year_; }
  int Expiration2DigitYear() const { return expiration_year_ % 100; }
  string16 ExpirationMonthAsString() const;
  string16 Expiration4DigitYearAsString() const;
  string16 Expiration2DigitYearAsString() const;

  // Sets |expiration_month_| to the integer conversion of |text|.
  void SetExpirationMonthFromString(const string16& text);

  // Sets |expiration_year_| to the integer conversion of |text|.
  void SetExpirationYearFromString(const string16& text);

  string16 number() const { return number_; }
  string16 name_on_card() const { return name_on_card_; }
  string16 type() const { return type_; }
  string16 verification_code() const { return verification_code_; }
  string16 last_four_digits() const { return last_four_digits_; }
  int expiration_month() const { return expiration_month_; }
  int expiration_year() const { return expiration_year_; }

  void set_number(const string16& number) { number_ = number; }
  void set_name_on_card(const string16& name_on_card) {
    name_on_card_ = name_on_card;
  }
  void set_type(const string16& type) { type_ = type; }
  void set_verification_code(const string16& verification_code) {
    verification_code_ = verification_code;
  }
  void set_last_four_digits(const string16& last_four_digits) {
    last_four_digits_ = last_four_digits;
  }

  // These setters verify that the month and year are within appropriate
  // ranges.
  void set_expiration_month(int expiration_month);
  void set_expiration_year(int expiration_year);

 private:
  explicit CreditCard(const CreditCard& card);
  void operator=(const CreditCard& card);

  // A helper function for FindInfoMatches that only handles matching the info
  // with the requested field type.
  bool FindInfoMatchesHelper(const AutoFillFieldType& field_type,
                             const string16& info,
                             string16* match) const;

  // Returns true if |text| matches the name on the card.  The comparison is
  // case-insensitive.
  bool IsNameOnCard(const string16& text) const;

  // Uses the Luhn formula to validate the credit card number in |text|.
  static bool IsCreditCardNumber(const string16& text);

  // Returns true if |text| matches the expiration month of the card.
  bool IsExpirationMonth(const string16& text) const;

  // Returns true if |text| matches the CVV of the card.  The comparison is
  // case-insensitive.
  bool IsVerificationCode(const string16& text) const;

  // Returns true if the integer value of |text| matches the 2-digit expiration
  // year.
  bool Is2DigitExpirationYear(const string16& text) const;

  // Returns true if the integer value of |text| matches the 4-digit expiration
  // year.
  bool Is4DigitExpirationYear(const string16& text) const;

  // Returns true if |text| matches the type of the card.  The comparison is
  // case-insensitive.
  bool IsCardType(const string16& text) const;

  // Converts |date| to an integer form.  Returns true if the conversion
  // succeeded.
  bool ConvertDate(const string16& date, int* num) const;

  // The credit card values.
  string16 number_;  // The encrypted credit card number.
  string16 name_on_card_;  // The cardholder's name.
  string16 type_;  // The type of the card.
  string16 verification_code_;  // The CVV.

  // Stores the last four digits of the credit card number.
  string16 last_four_digits_;

  // These members are zero if not present.
  int expiration_month_;
  int expiration_year_;

  // This is the display name of the card set by the user, e.g., Amazon Visa.
  string16 label_;
};

#endif  // CHROME_BROWSER_AUTOFILL_CREDIT_CARD_H_
