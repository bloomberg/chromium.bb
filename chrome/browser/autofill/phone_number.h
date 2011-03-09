// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_H_
#define CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_H_
#pragma once

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/autofill/form_group.h"

// A form group that stores phone number information.
class PhoneNumber : public FormGroup {
 public:
  PhoneNumber();
  explicit PhoneNumber(const PhoneNumber& number);
  virtual ~PhoneNumber();

  PhoneNumber& operator=(const PhoneNumber& number);

  // FormGroup implementation:
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
  virtual void GetAvailableFieldTypes(FieldTypeSet* available_types) const;
  virtual void FindInfoMatches(const AutofillType& type,
                               const string16& info,
                               std::vector<string16>* matched_text) const;
  virtual string16 GetFieldText(const AutofillType& type) const;
  virtual void SetInfo(const AutofillType& type, const string16& value);

  // Parses |value| to extract the components of a phone number.  |number|
  // returns the trailing 7 digits, |city_code| returns the next 3 digits, and
  // |country_code| returns any remaining digits.
  // Separator characters are stripped before parsing the digits.
  // Returns true if parsing was successful, false otherwise.
  static bool ParsePhoneNumber(const string16& value,
                               string16* number,
                               string16* city_code,
                               string16* country_code);

  // Size and offset of the prefix and suffix portions of phone numbers.
  static const int kPrefixOffset = 0;
  static const int kPrefixLength = 3;
  static const int kSuffixOffset = 3;
  static const int kSuffixLength = 4;

  // The following functions should return the field type for each part of the
  // phone number.  Currently, these are either fax or home phone number types.
  virtual AutofillFieldType GetNumberType() const = 0;
  virtual AutofillFieldType GetCityCodeType() const = 0;
  virtual AutofillFieldType GetCountryCodeType() const = 0;
  virtual AutofillFieldType GetCityAndNumberType() const = 0;
  virtual AutofillFieldType GetWholeNumberType() const = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(PhoneNumberTest, Matcher);

  const string16& country_code() const { return country_code_; }
  const string16& city_code() const { return city_code_; }
  const string16& number() const { return number_; }
  const string16& extension() const { return extension_; }
  string16 CityAndNumber() const { return city_code_ + number_; }

  // Returns the entire phone number as a string, without punctuation.
  virtual string16 WholeNumber() const;

  void set_country_code(const string16& country_code) {
    country_code_ = country_code;
  }
  void set_city_code(const string16& city_code) { city_code_ = city_code; }
  void set_number(const string16& number);
  void set_extension(const string16& extension) { extension_ = extension; }
  void set_whole_number(const string16& whole_number);

  // A helper function for FindInfoMatches that only handles matching the info
  // with the requested field type.
  bool FindInfoMatchesHelper(const FieldTypeSubGroup& subgroup,
                             const string16& info,
                             string16* match) const;

  // The numbers will be digits only (no punctuation), so any call to the IsX()
  // functions should first call StripPunctuation on the text.
  bool IsNumber(const string16& text) const;
  bool IsCityCode(const string16& text) const;
  bool IsCountryCode(const string16& text) const;
  bool IsCityAndNumber(const string16& text) const;
  bool IsWholeNumber(const string16& text) const;

  // Verifies that |number| is a valid phone number.
  bool Validate(const string16& number) const;

  // Removes any punctuation characters from |number|.
  static void StripPunctuation(string16* number);

  // The pieces of the phone number.
  string16 country_code_;
  string16 city_code_;  // city or area code.
  string16 number_;
  string16 extension_;
};

#endif  // CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_H_
