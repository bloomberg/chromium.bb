// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_H_
#define CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_H_

#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/form_group.h"

// A form group that stores phone number information.
class PhoneNumber : public FormGroup {
 public:
  PhoneNumber() {}
  virtual ~PhoneNumber() {}

  // FormGroup implementation:
  virtual FormGroup* Clone() const = 0;
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
  virtual void FindInfoMatches(const AutoFillType& type,
                               const string16& info,
                               std::vector<string16>* matched_text) const;
  virtual string16 GetFieldText(const AutoFillType& type) const;
  virtual void SetInfo(const AutoFillType& type, const string16& value);

  string16 country_code() const { return country_code_; }
  string16 city_code() const { return city_code_; }
  string16 number() const { return number_; }
  string16 extension() const { return extension_; }
  string16 CityAndNumber() const { return city_code_ + number_; }

  // Returns the entire phone number as a string, without punctuation.
  virtual string16 WholeNumber() const;

  void set_country_code(const string16& country_code) {
    country_code_ = country_code;
  }
  void set_city_code(const string16& city_code) { city_code_ = city_code; }
  void set_number(const string16& number);
  void set_extension(const string16& extension) { extension_ = extension; }

 protected:
  explicit PhoneNumber(const PhoneNumber& phone_number);
  void operator=(const PhoneNumber& phone_number);

  // A helper function for FindInfoMatches that only handles matching the info
  // with the requested field type.
  bool FindInfoMatchesHelper(const FieldTypeSubGroup& subgroup,
                             const string16& info,
                             string16* match) const;

  // The numbers will be digits only (no punctuation), so any call to the IsX()
  // functions should first call StripPunctuation on the text.
  virtual bool IsNumber(const string16& text) const;
  virtual bool IsCityCode(const string16& text) const;
  virtual bool IsCountryCode(const string16& text) const;
  virtual bool IsCityAndNumber(const string16& text) const;
  virtual bool IsWholeNumber(const string16& text) const;

  // The following functions should return the field type for each part of the
  // phone number.  Currently, these are either fax or home phone number types.
  virtual AutoFillFieldType GetNumberType() const = 0;
  virtual AutoFillFieldType GetCityCodeType() const = 0;
  virtual AutoFillFieldType GetCountryCodeType() const = 0;
  virtual AutoFillFieldType GetCityAndNumberType() const = 0;
  virtual AutoFillFieldType GetWholeNumberType() const = 0;

  // Verifies that |number| is a valid phone number.
  bool Validate(const string16& number) const;

  // Removes any punctuation characters from |number|.
  void StripPunctuation(string16* number) const;

  // The pieces of the phone number.
  string16 country_code_;
  string16 city_code_;  // city or area code.
  string16 number_;
  string16 extension_;
};

#endif  // CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_H_
