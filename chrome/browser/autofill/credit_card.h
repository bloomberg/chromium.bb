// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CREDIT_CARD_H_
#define CHROME_BROWSER_AUTOFILL_CREDIT_CARD_H_
#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_group.h"

// A form group that stores credit card information.
class CreditCard : public FormGroup {
 public:
  explicit CreditCard(const std::string& guid);

  // For use in STL containers.
  CreditCard();
  CreditCard(const CreditCard& credit_card);
  virtual ~CreditCard();

  // FormGroup implementation:
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
  virtual void GetAvailableFieldTypes(FieldTypeSet* available_types) const;
  virtual string16 GetInfo(AutofillFieldType type) const;
  virtual void SetInfo(AutofillFieldType type, const string16& value);
  // Credit card preview summary, for example: ******1234, Exp: 01/2020
  virtual const string16 Label() const;

  // Special method to set value for HTML5 month input type.
  void SetInfoForMonthInputType(const string16& value);

  // The number altered for display, for example: ******1234
  string16 ObfuscatedNumber() const;
  // The last four digits of the credit card number.
  string16 LastFourDigits() const;

  const std::string& type() const { return type_; }

  // The guid is the primary identifier for |CreditCard| objects.
  const std::string guid() const { return guid_; }
  void set_guid(const std::string& guid) { guid_ = guid; }

  // For use in STL containers.
  void operator=(const CreditCard& credit_card);

  // Comparison for Sync.  Returns 0 if the credit card is the same as |this|,
  // or < 0, or > 0 if it is different.  The implied ordering can be used for
  // culling duplicates.  The ordering is based on collation order of the
  // textual contents of the fields.
  // GUIDs, labels, and unique IDs are not compared, only the values of the
  // credit cards themselves.
  int Compare(const CreditCard& credit_card) const;

  // Used by tests.
  bool operator==(const CreditCard& credit_card) const;
  bool operator!=(const CreditCard& credit_card) const;

  // Return a version of |number| that has any separator characters removed.
  static const string16 StripSeparators(const string16& number);

  // Returns true if |text| looks like a valid credit card number.
  // Uses the Luhn formula to validate the number.
  static bool IsValidCreditCardNumber(const string16& text);

  // Returns true if there are no values (field types) set.
  bool IsEmpty() const;

  // Returns the credit card number.
  const string16& number() const { return number_; }

 private:
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

  // Sets |number_| to the stripped version of |number|, containing only digits.
  void SetNumber(const string16& number);

  // These setters verify that the month and year are within appropriate
  // ranges.
  void SetExpirationMonth(int expiration_month);
  void SetExpirationYear(int expiration_year);

  // Returns true if |text| matches the name on the card.  The comparison is
  // case-insensitive.
  bool IsNameOnCard(const string16& text) const;

  // Returns true if |text| matches the card number.
  bool IsNumber(const string16& text) const;

  // Returns true if |text| matches the expiration month of the card.
  bool IsExpirationMonth(const string16& text) const;

  // Returns true if the integer value of |text| matches the 2-digit expiration
  // year.
  bool Is2DigitExpirationYear(const string16& text) const;

  // Returns true if the integer value of |text| matches the 4-digit expiration
  // year.
  bool Is4DigitExpirationYear(const string16& text) const;

  string16 number_;  // The credit card number.
  string16 name_on_card_;  // The cardholder's name.
  std::string type_;  // The type of the card.

  // These members are zero if not present.
  int expiration_month_;
  int expiration_year_;

  // The guid of this credit card.
  std::string guid_;
};

// So we can compare CreditCards with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const CreditCard& credit_card);

// The string identifiers for credit card icon resources.
extern const char* const kAmericanExpressCard;
extern const char* const kDinersCard;
extern const char* const kDiscoverCard;
extern const char* const kGenericCard;
extern const char* const kJCBCard;
extern const char* const kMasterCard;
extern const char* const kSoloCard;
extern const char* const kVisaCard;

#endif  // CHROME_BROWSER_AUTOFILL_CREDIT_CARD_H_
