// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PHONE_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_PHONE_FIELD_H_
#pragma once

#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/form_field.h"
#include "chrome/browser/autofill/phone_number.h"

class AutoFillField;

// A phone number in one of the following formats:
// - area code, prefix, suffix
// - area code, number
// - number
class PhoneField : public FormField {
 public:
  virtual ~PhoneField();

  static PhoneField* Parse(std::vector<AutoFillField*>::const_iterator* iter,
                           bool is_ecml);
  static PhoneField* ParseECML(
      std::vector<AutoFillField*>::const_iterator* iter);

  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const;

 private:
  PhoneField();

  enum PhoneType {
    PHONE_TYPE_FIRST = 0,
    HOME_PHONE = PHONE_TYPE_FIRST,
    FAX_PHONE,

    // Must be last.
    PHONE_TYPE_MAX,
  };

  // Some field names are different for phone and fax.
  string16 GetCountryRegex() const;
  // This string includes all area code separators, including NoText.
  string16 GetAreaRegex() const;
  // Separator of the area code in the case fields are formatted without
  // any text indicating what fields are (e.g. field1 "(" field2 ")" field3 "-"
  // field4 means Country Code, Area Code, Prefix, Suffix)
  string16 GetAreaNoTextRegex() const;
  string16 GetPhoneRegex() const;
  string16 GetPrefixSeparatorRegex() const;
  string16 GetPrefixRegex() const;
  string16 GetSuffixSeparatorRegex() const;
  string16 GetSuffixRegex() const;
  string16 GetExtensionRegex() const;

  // This is for easy description of the possible parsing paths of the phone
  // fields.
  enum RegexType {
    REGEX_COUNTRY,
    REGEX_AREA,
    REGEX_AREA_NOTEXT,
    REGEX_PHONE,
    REGEX_PREFIX_SEPARATOR,
    REGEX_PREFIX,
    REGEX_SUFFIX_SEPARATOR,
    REGEX_SUFFIX,
    REGEX_EXTENSION,

    // Separates regexps in grammar.
    REGEX_SEPARATOR,
  };

  string16 GetRegExp(RegexType regex_id) const;

  // |field| - field to fill up on successful parsing.
  // |iter| - in/out. Form field iterator, points to the first field that is
  //   attempted to be parsed. If parsing successful, points to the first field
  //   after parsed fields.
  // |regular_phone| - true if the parsed phone is a HOME phone, false
  //   otherwise.
  static bool ParseInternal(PhoneField* field,
                            std::vector<AutoFillField*>::const_iterator* iter,
                            bool regular_phone);

  void SetPhoneType(PhoneType phone_type);

  // Field types are different as well, so we create a temporary phone number,
  // to get relevant field types.
  scoped_ptr<PhoneNumber> number_;
  PhoneType phone_type_;


  // Parsed fields.
  enum PhonePart {
    FIELD_NONE = -1,
    FIELD_COUNTRY_CODE,
    FIELD_AREA_CODE,
    FIELD_PHONE,
    FIELD_SUFFIX,
    FIELD_EXTENSION,

    FIELD_MAX,
  };

  // FIELD_PHONE is always present; holds suffix if prefix is present.
  // The rest could be NULL.
  AutoFillField* parsed_phone_fields_[FIELD_MAX];

  static struct Parser {
    RegexType regex;       // Field matching reg-ex.
    PhonePart phone_part;  // Index of the field.
    int max_size;          // Max size of the field to match. 0 means any.
  } phone_field_grammars_[];

  DISALLOW_COPY_AND_ASSIGN(PhoneField);
};

#endif  // CHROME_BROWSER_AUTOFILL_PHONE_FIELD_H_
