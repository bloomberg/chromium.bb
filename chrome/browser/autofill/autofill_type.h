// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_TYPE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_TYPE_H_
#pragma once

#include <map>
#include <set>

#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"

// The high-level description of AutoFill types, used to categorize form fields
// and for associating form fields with form values in the Web Database.
class AutoFillType {
 public:
  enum FieldTypeGroup {
    NO_GROUP,
    CONTACT_INFO,
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

  struct AutoFillTypeDefinition {
    FieldTypeGroup group;
    FieldTypeSubGroup subgroup;
  };

  explicit AutoFillType(AutoFillFieldType field_type);
  AutoFillType(const AutoFillType& autofill_type);
  AutoFillType& operator=(const AutoFillType& autofill_type);

  AutoFillFieldType field_type() const;
  FieldTypeGroup group() const;
  FieldTypeSubGroup subgroup() const;

  // Maps |field_type| to a field type that can be directly stored in a profile
  // (in the sense that it makes sense to call |AutoFillProfile::SetInfo()| with
  // the returned field type as the first parameter).
  static AutoFillFieldType GetEquivalentFieldType(AutoFillFieldType field_type);

 private:
  AutoFillFieldType field_type_;
};

typedef AutoFillType::FieldTypeGroup FieldTypeGroup;
typedef AutoFillType::FieldTypeSubGroup FieldTypeSubGroup;
typedef std::set<AutoFillFieldType> FieldTypeSet;
typedef std::map<string16, AutoFillFieldType> FieldTypeMap;

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_TYPE_H_
