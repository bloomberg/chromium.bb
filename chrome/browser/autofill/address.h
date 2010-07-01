// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ADDRESS_H_
#define CHROME_BROWSER_AUTOFILL_ADDRESS_H_

#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/form_group.h"

// A form group that stores address information.
class Address : public FormGroup {
 public:
  Address() {}
  virtual ~Address() {}

  // FormGroup implementation:
  virtual FormGroup* Clone() const = 0;
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
  virtual void GetAvailableFieldTypes(FieldTypeSet* available_types) const;
  virtual void FindInfoMatches(const AutoFillType& type,
                               const string16& info,
                               std::vector<string16>* matched_text) const;
  virtual string16 GetFieldText(const AutoFillType& type) const;
  virtual void SetInfo(const AutoFillType& type, const string16& value);

  // Sets all of the fields to the empty string.
  void Clear();

  // Sets the values of this object to the values in |address|.
  void Clone(const Address& address);

 protected:
  explicit Address(const Address& address);

 private:
  // Vector of tokens in an address line.
  typedef std::vector<string16> LineTokens;

  const string16& line1() const { return line1_; }
  const string16& line2() const { return line2_; }
  const string16& apt_num() const { return apt_num_; }
  const string16& city() const { return city_; }
  const string16& state() const { return state_; }
  const string16& country() const { return country_; }
  const string16& zip_code() const { return zip_code_; }

  void set_line1(const string16& line1);
  void set_line2(const string16& line2);
  void set_apt_num(const string16& apt_num) { apt_num_ = apt_num; }
  void set_city(const string16& city) { city_ = city; }
  void set_state(const string16& state) { state_ = state; }
  void set_country(const string16& country) { country_ = country; }
  void set_zip_code(const string16& zip_code) { zip_code_ = zip_code; }

  void operator=(const Address& address);

  // The following functions match |text| against the various values of the
  // address, returning true on match.
  virtual bool IsLine1(const string16& text) const;
  virtual bool IsLine2(const string16& text) const;
  virtual bool IsAptNum(const string16& text) const;
  virtual bool IsCity(const string16& text) const;
  virtual bool IsState(const string16& text) const;
  virtual bool IsCountry(const string16& text) const;
  virtual bool IsZipCode(const string16& text) const;

  // The following functions should return the field type for each part of the
  // address.  Currently, these are either home or billing address types.
  virtual AutoFillFieldType GetLine1Type() const = 0;
  virtual AutoFillFieldType GetLine2Type() const = 0;
  virtual AutoFillFieldType GetAptNumType() const = 0;
  virtual AutoFillFieldType GetCityType() const = 0;
  virtual AutoFillFieldType GetStateType() const = 0;
  virtual AutoFillFieldType GetZipCodeType() const = 0;
  virtual AutoFillFieldType GetCountryType() const = 0;

  // A helper function for FindInfoMatches that only handles matching |info|
  // with the requested field subgroup.
  bool FindInfoMatchesHelper(const FieldTypeSubGroup& subgroup,
                             const string16& info,
                             string16* match) const;

  // Returns true if all of the tokens in |text| match the tokens in
  // |line_tokens|.
  bool IsLineMatch(const string16& text, const LineTokens& line_tokens) const;

  // Returns true if |word| is one of the tokens in |line_tokens|.
  bool IsWordInLine(const string16& word, const LineTokens& line_tokens) const;

  // List of tokens in each part of |line1_| and |line2_|.
  LineTokens line1_tokens_;
  LineTokens line2_tokens_;

  // The address.
  string16 line1_;
  string16 line2_;
  string16 apt_num_;
  string16 city_;
  string16 state_;
  string16 country_;
  string16 zip_code_;
};

#endif  // CHROME_BROWSER_AUTOFILL_ADDRESS_H_
