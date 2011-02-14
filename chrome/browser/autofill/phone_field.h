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

  enum PHONE_TYPE {
    PHONE_TYPE_FIRST = 0,
    HOME_PHONE = PHONE_TYPE_FIRST,
    FAX_PHONE,

    // Must be last.
    PHONE_TYPE_MAX,
  };

  // Some field names are different for phone and fax.
  string16 GetAreaRegex() const;
  string16 GetPhoneRegex() const;
  string16 GetPrefixRegex() const;
  string16 GetSuffixRegex() const;
  string16 GetExtensionRegex() const;

  // |field| - field to fill up on successful parsing.
  // |iter| - in/out. Form field iterator, points to the first field that is
  //   attempted to be parsed. If parsing successful, points to the first field
  //   after parsed fields.
  // |regular_phone| - true if the parsed phone is a HOME phone, false
  //   otherwise.
  static bool ParseInternal(PhoneField* field,
                            std::vector<AutoFillField*>::const_iterator* iter,
                            bool regular_phone);

  void SetPhoneType(PHONE_TYPE phone_type);

  // Field types are different as well, so we create a temporary phone number,
  // to get relevant field types.
  scoped_ptr<PhoneNumber> number_;
  PHONE_TYPE phone_type_;

  // Always present; holds suffix if prefix is present.
  AutoFillField* phone_;

  AutoFillField* area_code_;  // optional
  AutoFillField* prefix_;     // optional
  AutoFillField* extension_;  // optional

  DISALLOW_COPY_AND_ASSIGN(PhoneField);
};

#endif  // CHROME_BROWSER_AUTOFILL_PHONE_FIELD_H_
