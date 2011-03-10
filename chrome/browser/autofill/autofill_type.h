// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_TYPE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_TYPE_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"

// The high-level description of AutoFill types, used to categorize form fields
// and for associating form fields with form values in the Web Database.
class AutofillType {
 public:
  enum FieldTypeGroup {
    NO_GROUP,
    NAME,
    EMAIL,
    COMPANY,
    ADDRESS_HOME,
    ADDRESS_BILLING,
    PHONE_HOME,
    PHONE_FAX,
    CREDIT_CARD,
  };

  enum FieldTypeSubGroup {
    NO_SUBGROUP,
    // Address subgroups.
    ADDRESS_LINE1,
    ADDRESS_LINE2,
    ADDRESS_APT_NUM,
    ADDRESS_CITY,
    ADDRESS_STATE,
    ADDRESS_ZIP,
    ADDRESS_COUNTRY,

    // Phone subgroups.
    PHONE_NUMBER,
    PHONE_CITY_CODE,
    PHONE_COUNTRY_CODE,
    PHONE_CITY_AND_NUMBER,
    PHONE_WHOLE_NUMBER
  };

  struct AutofillTypeDefinition {
    FieldTypeGroup group;
    FieldTypeSubGroup subgroup;
  };

  explicit AutofillType(AutofillFieldType field_type);
  AutofillType(const AutofillType& autofill_type);
  AutofillType& operator=(const AutofillType& autofill_type);

  AutofillFieldType field_type() const;
  FieldTypeGroup group() const;
  FieldTypeSubGroup subgroup() const;

  // Maps |field_type| to a field type that can be directly stored in a profile
  // (in the sense that it makes sense to call |AutofillProfile::SetInfo()| with
  // the returned field type as the first parameter).
  static AutofillFieldType GetEquivalentFieldType(AutofillFieldType field_type);

  // Utilities for serializing and deserializing an |AutofillFieldType|.
  static std::string FieldTypeToString(AutofillFieldType field_type);
  static AutofillFieldType StringToFieldType(const std::string& str);

 private:
  AutofillFieldType field_type_;
};

typedef AutofillType::FieldTypeGroup FieldTypeGroup;
typedef AutofillType::FieldTypeSubGroup FieldTypeSubGroup;
typedef std::set<AutofillFieldType> FieldTypeSet;
typedef std::map<string16, AutofillFieldType> FieldTypeMap;

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_TYPE_H_
