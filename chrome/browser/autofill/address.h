// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ADDRESS_H_
#define CHROME_BROWSER_AUTOFILL_ADDRESS_H_
#pragma once

#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_group.h"

// A form group that stores address information.
class Address : public FormGroup {
 public:
  Address();
  Address(const Address& address);
  virtual ~Address();

  Address& operator=(const Address& address);

  // FormGroup:
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
  virtual void GetAvailableFieldTypes(FieldTypeSet* available_types) const;
  virtual string16 GetInfo(AutofillFieldType type) const;
  virtual void SetInfo(AutofillFieldType type, const string16& value);

  const std::string& country_code() const { return country_code_; }
  void set_country_code(const std::string& country_code) {
    country_code_ = country_code;
  }

  // Sets all of the fields to the empty string.
  void Clear();

 private:
  // Vector of tokens in an address line.
  typedef std::vector<string16> LineTokens;

  // Returns the localized country name corresponding to |country_code_|.
  string16 Country() const;

  void set_line1(const string16& line1);
  void set_line2(const string16& line2);

  // Sets the |country_code_| based on |country|, which should be a localized
  // country name.
  void SetCountry(const string16& country);

  // The following functions match |text| against the various values of the
  // address, returning true on match.
  virtual bool IsLine1(const string16& text) const;
  virtual bool IsLine2(const string16& text) const;
  virtual bool IsCity(const string16& text) const;
  virtual bool IsState(const string16& text) const;
  virtual bool IsCountry(const string16& text) const;
  virtual bool IsZipCode(const string16& text) const;

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
  string16 city_;
  string16 state_;
  std::string country_code_;
  string16 zip_code_;
};

#endif  // CHROME_BROWSER_AUTOFILL_ADDRESS_H_
