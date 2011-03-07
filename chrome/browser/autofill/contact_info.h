// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CONTACT_INFO_H_
#define CHROME_BROWSER_AUTOFILL_CONTACT_INFO_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/form_group.h"

typedef std::vector<string16> NameTokens;

// A form group that stores contact information.
class ContactInfo : public FormGroup {
 public:
  ContactInfo();
  virtual ~ContactInfo();

  // FormGroup implementation:
  virtual FormGroup* Clone() const;
  virtual void GetPossibleFieldTypes(const string16& text,
                                     FieldTypeSet* possible_types) const;
  virtual void GetAvailableFieldTypes(FieldTypeSet* available_types) const;
  virtual void FindInfoMatches(const AutofillType& type,
                               const string16& info,
                               std::vector<string16>* matched_text) const;
  virtual string16 GetFieldText(const AutofillType& type) const;
  virtual void SetInfo(const AutofillType& type, const string16& value);

 private:
  friend class ContactInfoTest;
  explicit ContactInfo(const ContactInfo& contact_info);
  void operator=(const ContactInfo& info);

  // Returns the full name, which can include up to the first, middle, middle
  // initial, last name, and suffix.
  string16 FullName() const;

  // Returns the middle initial if |middle_| is non-empty.  Returns an empty
  // string otherwise.
  string16 MiddleInitial() const;

  const string16& first() const { return first_; }
  const string16& middle() const { return middle_; }
  const string16& last() const { return last_; }
  const string16& suffix() const { return suffix_; }
  const string16& email() const { return email_; }
  const string16& company_name() const { return company_name_; }

  // A helper function for FindInfoMatches that only handles matching the info
  // with the requested field type.
  bool FindInfoMatchesHelper(const AutofillFieldType& field_type,
                             const string16& info,
                             string16* matched_text) const;

  // Returns true if |text| is the first name.
  bool IsFirstName(const string16& text) const;

  // Returns true if |text| is the middle name.
  bool IsMiddleName(const string16& text) const;

  // Returns true if |text| is the last name.
  bool IsLastName(const string16& text) const;

  // Returns true if |text| is the suffix.
  bool IsSuffix(const string16& text) const;

  // Returns true if |text| is the middle initial.
  bool IsMiddleInitial(const string16& text) const;

  // Returns true if |text| is the last name.
  bool IsFullName(const string16& text) const;

  // Returns true if all of the tokens in |text| match the tokens in
  // |name_tokens|.
  bool IsNameMatch(const string16& text, const NameTokens& name_tokens) const;

  // Returns true if |word| is one of the tokens in |name_tokens|.
  bool IsWordInName(const string16& word, const NameTokens& name_tokens) const;

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

  void set_suffix(const string16& suffix) { suffix_ = suffix; }

  // List of tokens in each part of the name.
  NameTokens first_tokens_;
  NameTokens middle_tokens_;
  NameTokens last_tokens_;

  // Contact information data.
  string16 first_;
  string16 middle_;
  string16 last_;
  string16 suffix_;
  string16 email_;
  string16 company_name_;
};

#endif  // CHROME_BROWSER_AUTOFILL_CONTACT_INFO_H_
