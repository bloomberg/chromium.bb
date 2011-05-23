// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_H_
#define CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/form_group.h"

// A form group that stores phone number information.
class PhoneNumber : public FormGroup {
 public:
  PhoneNumber();
  explicit PhoneNumber(AutofillType::FieldTypeGroup phone_group);
  PhoneNumber(const PhoneNumber& number);
  virtual ~PhoneNumber();

  PhoneNumber& operator=(const PhoneNumber& number);

  // FormGroup implementation:
  virtual void GetMatchingTypes(const string16& text,
                                FieldTypeSet* matching_types) const;
  virtual void GetNonEmptyTypes(FieldTypeSet* non_empty_typess) const;
  virtual string16 GetInfo(AutofillFieldType type) const;
  virtual void SetInfo(AutofillFieldType type, const string16& value);

  // Validates |number_| and translates it into digits-only format.
  // Locale must be set.
  bool NormalizePhone();

  // Size and offset of the prefix and suffix portions of phone numbers.
  static const int kPrefixOffset = 0;
  static const int kPrefixLength = 3;
  static const int kSuffixOffset = 3;
  static const int kSuffixLength = 4;

  // Sets locale for normalizing phone numbers. Must be called if you get
  // normalized number or use NormalizePhone() function;
  // Setting it to "", actually sets it to default locale - "US".
  void set_locale(const std::string& locale);

  // The following functions should return the field type for each part of the
  // phone number.  Currently, these are either fax or home phone number types.
  AutofillFieldType GetNumberType() const;
  AutofillFieldType GetCityCodeType() const;
  AutofillFieldType GetCountryCodeType() const;
  AutofillFieldType GetCityAndNumberType() const;
  AutofillFieldType GetWholeNumberType() const;

  // The class used to combine home phone or fax parts into a whole number.
  class PhoneCombineHelper {
   public:
    explicit PhoneCombineHelper(AutofillType::FieldTypeGroup phone_group)
        : phone_group_(phone_group) { }
    // Sets PHONE_HOME/FAX_CITY_CODE, PHONE_HOME/FAX_COUNTRY_CODE,
    // PHONE_HOME/FAX_CITY_AND_NUMBER, PHONE_HOME/FAX_NUMBER and returns true.
    // For all other field types returs false.
    bool SetInfo(AutofillFieldType type, const string16& value);
    // Returns true if parsing was successful, false otherwise.
    bool ParseNumber(const std::string& locale, string16* value);

    bool empty() const { return phone_.empty(); }
   private:
    string16 country_;
    string16 city_;
    string16 phone_;
    AutofillType::FieldTypeGroup phone_group_;
  };

 private:
  FRIEND_TEST_ALL_PREFIXES(PhoneNumberTest, Matcher);

  // The numbers will be digits only (no punctuation), so any call to the IsX()
  // functions should first call StripPunctuation on the text.
  bool IsNumber(const string16& text, const string16& number) const;
  bool IsWholeNumber(const string16& text) const;

  static void StripPunctuation(string16* number);

  // Phone group -  currently it is PHONE_HOME and PHONE_FAX.
  AutofillType::FieldTypeGroup phone_group_;
  // Locale for phone normalizing.
  std::string locale_;
  // The pieces of the phone number.
  string16 number_;
};

#endif  // CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_H_
