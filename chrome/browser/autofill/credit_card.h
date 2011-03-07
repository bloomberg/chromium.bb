// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CREDIT_CARD_H_
#define CHROME_BROWSER_AUTOFILL_CREDIT_CARD_H_
#pragma once

#include <vector>

#include "base/string16.h"
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
  virtual FormGroup* Clone() const;
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
  virtual void GetAvailableFieldTypes(FieldTypeSet* available_types) const;
  virtual void FindInfoMatches(const AutofillType& type,
                               const string16& info,
                               std::vector<string16>* matched_text) const;
  virtual string16 GetFieldText(const AutofillType& type) const;
  virtual string16 GetPreviewText(const AutofillType& type) const;
  virtual void SetInfo(const AutofillType& type, const string16& value);
  virtual const string16 Label() const;

  // Special method to set value for HTML5 month input type.
  void SetInfoForMonthInputType(const string16& value);

  // The number altered for display, for example: ******1234
  string16 ObfuscatedNumber() const;
  // Credit card preview summary, for example: ******1234, Exp: 01/2020
  string16 PreviewSummary() const;
  // The last four digits of the credit card number.
  string16 LastFourDigits() const;

  const string16& type() const { return type_; }

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

  // Returns true if |value| is a credit card number.  Uses the Luhn formula to
  // validate the number.
  static bool IsCreditCardNumber(const string16& text);

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

  const string16& name_on_card() const { return name_on_card_; }
  const string16& last_four_digits() const { return last_four_digits_; }
  int expiration_month() const { return expiration_month_; }
  int expiration_year() const { return expiration_year_; }

  void set_number(const string16& number);
  void set_name_on_card(const string16& name_on_card) {
    name_on_card_ = name_on_card;
  }
  void set_type(const string16& type) { type_ = type; }
  void set_last_four_digits(const string16& last_four_digits) {
    last_four_digits_ = last_four_digits;
  }

  // These setters verify that the month and year are within appropriate
  // ranges.
  void set_expiration_month(int expiration_month);
  void set_expiration_year(int expiration_year);

  // A helper function for FindInfoMatches that only handles matching the info
  // with the requested field type.
  bool FindInfoMatchesHelper(const AutofillFieldType& field_type,
                             const string16& info,
                             string16* match) const;

  // Returns true if |text| matches the name on the card.  The comparison is
  // case-insensitive.
  bool IsNameOnCard(const string16& text) const;

  // Returns true if |text| matches the expiration month of the card.
  bool IsExpirationMonth(const string16& text) const;

  // Returns true if the integer value of |text| matches the 2-digit expiration
  // year.
  bool Is2DigitExpirationYear(const string16& text) const;

  // Returns true if the integer value of |text| matches the 4-digit expiration
  // year.
  bool Is4DigitExpirationYear(const string16& text) const;

  // Converts |date| to an integer form.  Returns true if the conversion
  // succeeded.
  bool ConvertDate(const string16& date, int* num) const;

  string16 number_;  // The credit card number.
  string16 name_on_card_;  // The cardholder's name.
  string16 type_;  // The type of the card.

  // Stores the last four digits of the credit card number.
  string16 last_four_digits_;

  // These members are zero if not present.
  int expiration_month_;
  int expiration_year_;

  // This is the display name of the card set by the user, e.g., Amazon Visa.
  string16 label_;

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
