// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CONTACT_INFO_H_
#define CHROME_BROWSER_AUTOFILL_CONTACT_INFO_H_
#pragma once

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_group.h"

// A form group that stores name information.
class NameInfo : public FormGroup {
 public:
  NameInfo();
  NameInfo(const NameInfo& info);
  virtual ~NameInfo();

  NameInfo& operator=(const NameInfo& info);

  // FormGroup:
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
  virtual void GetAvailableFieldTypes(FieldTypeSet* available_types) const;
  virtual string16 GetInfo(AutofillFieldType type) const;
  virtual void SetInfo(AutofillFieldType type, const string16& value);

 private:
  FRIEND_TEST_ALL_PREFIXES(NameInfoTest, TestSetFullName);

  // Returns the full name, which can include up to the first, middle, and last
  // name.
  string16 FullName() const;

  // Returns the middle initial if |middle_| is non-empty.  Returns an empty
  // string otherwise.
  string16 MiddleInitial() const;

  const string16& first() const { return first_; }
  const string16& middle() const { return middle_; }
  const string16& last() const { return last_; }

  // Returns true if |text| is the first name.
  bool IsFirstName(const string16& text) const;

  // Returns true if |text| is the middle name.
  bool IsMiddleName(const string16& text) const;

  // Returns true if |text| is the last name.
  bool IsLastName(const string16& text) const;

  // Returns true if |text| is the middle initial.
  bool IsMiddleInitial(const string16& text) const;

  // Returns true if |text| is the last name.
  bool IsFullName(const string16& text) const;

  // Returns true if all of the tokens in |text| match the tokens in
  // |name_tokens|.
  bool IsNameMatch(const string16& text,
                   const std::vector<string16>& name_tokens) const;

  // Returns true if |word| is one of the tokens in |name_tokens|.
  bool IsWordInName(const string16& word,
                    const std::vector<string16>& name_tokens) const;

  // Sets |first_| to |first| and |first_tokens_| to the set of tokens in
  // |first|, made lowercase.
  void SetFirst(const string16& first);

  // Sets |middle_| to |middle| and |middle_tokens_| to the set of tokens in
  // |middle|, made lowercase.
  void SetMiddle(const string16& middle);

  // Sets |last_| to |last| and |last_tokens_| to the set of tokens in |last|,
  // made lowercase.
  void SetLast(const string16& last);

  // Sets |first_|, |middle_|, |last_| and |*_tokens_| to the tokenized
  // |full|. It is tokenized on a space only.
  void SetFullName(const string16& full);

  // List of tokens in each part of the name.
  std::vector<string16> first_tokens_;
  std::vector<string16> middle_tokens_;
  std::vector<string16> last_tokens_;

  string16 first_;
  string16 middle_;
  string16 last_;
};

class EmailInfo : public FormGroup {
 public:
  EmailInfo();
  EmailInfo(const EmailInfo& info);
  virtual ~EmailInfo();

  EmailInfo& operator=(const EmailInfo& info);

  // FormGroup:
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
  virtual void GetAvailableFieldTypes(FieldTypeSet* available_types) const;
  virtual string16 GetInfo(AutofillFieldType type) const;
  virtual void SetInfo(AutofillFieldType type, const string16& value);

 private:
  string16 email_;
};

class CompanyInfo : public FormGroup {
 public:
  CompanyInfo();
  CompanyInfo(const CompanyInfo& info);
  virtual ~CompanyInfo();

  CompanyInfo& operator=(const CompanyInfo& info);

  // FormGroup:
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
  virtual void GetAvailableFieldTypes(FieldTypeSet* available_types) const;
  virtual string16 GetInfo(AutofillFieldType type) const;
  virtual void SetInfo(AutofillFieldType type, const string16& value);

 private:
  string16 company_name_;
};

#endif  // CHROME_BROWSER_AUTOFILL_CONTACT_INFO_H_
