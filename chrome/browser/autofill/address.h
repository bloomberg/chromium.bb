// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ADDRESS_H_
#define CHROME_BROWSER_AUTOFILL_ADDRESS_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/form_group.h"

// A form group that stores address information.
class Address : public FormGroup {
 public:
  Address();
  virtual ~Address();

  // FormGroup:
  virtual FormGroup* Clone() const;
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

 private:
  // Vector of tokens in an address line.
  typedef std::vector<string16> LineTokens;

  explicit Address(const Address& address);

  void operator=(const Address& address);

  void set_line1(const string16& line1);
  void set_line2(const string16& line2);

  // The following functions match |text| against the various values of the
  // address, returning true on match.
  virtual bool IsLine1(const string16& text) const;
  virtual bool IsLine2(const string16& text) const;
  virtual bool IsCity(const string16& text) const;
  virtual bool IsState(const string16& text) const;
  virtual bool IsCountry(const string16& text) const;
  virtual bool IsZipCode(const string16& text) const;

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
  string16 city_;
  string16 state_;
  string16 country_;
  string16 zip_code_;
};

#endif  // CHROME_BROWSER_AUTOFILL_ADDRESS_H_
