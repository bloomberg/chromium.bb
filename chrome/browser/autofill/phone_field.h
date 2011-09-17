// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PHONE_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_PHONE_FIELD_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/form_field.h"
#include "chrome/browser/autofill/phone_number.h"

class AutofillField;
class AutofillScanner;

// A phone number in one of the following formats:
// - area code, prefix, suffix
// - area code, number
// - number
class PhoneField : public FormField {
 public:
  virtual ~PhoneField();

  static FormField* Parse(AutofillScanner* scanner);

 protected:
  // FormField:
  virtual bool ClassifyField(FieldTypeMap* map) const OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest, ParseOneLinePhone);
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest, ParseTwoLinePhone);
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest, ThreePartPhoneNumber);
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest, ThreePartPhoneNumberPrefixSuffix);
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest, ThreePartPhoneNumberPrefixSuffix2);
  FRIEND_TEST_ALL_PREFIXES(PhoneFieldTest, CountryAndCityAndPhoneNumber);

  PhoneField();

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
  // TODO(isherman): This method doc is out of date.
  static bool ParseInternal(PhoneField* field, AutofillScanner* scanner);

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
  const AutofillField* parsed_phone_fields_[FIELD_MAX];

  static struct Parser {
    RegexType regex;       // Field matching reg-ex.
    PhonePart phone_part;  // Index of the field.
    size_t max_size;       // Max size of the field to match. 0 means any.
  } phone_field_grammars_[];

  DISALLOW_COPY_AND_ASSIGN(PhoneField);
};

#endif  // CHROME_BROWSER_AUTOFILL_PHONE_FIELD_H_
