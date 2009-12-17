// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/name_field.h"

#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_type.h"

NameField* NameField::Parse(std::vector<AutoFillField*>::const_iterator* iter,
                            bool is_ecml) {
  // Try FirstLastNameField first since it's more specific.
  NameField* field = FirstLastNameField::Parse(iter, is_ecml);
  if (field == NULL && !is_ecml)
    field = FullNameField::Parse(iter);
  return field;
}

FullNameField* FullNameField::Parse(
    std::vector<AutoFillField*>::const_iterator* iter) {
  // Exclude labels containing the string "username", which typically
  // denotes a login ID rather than the user's actual name.
  AutoFillField* field = **iter;
  if (Match(field, ASCIIToUTF16("username")))
    return NULL;

  // Searching for any label containing the word "name" is too general;
  // for example, Travelocity_Edit travel profile.html contains a field
  // "Travel Profile Name".
  if (ParseText(iter, ASCIIToUTF16("^name|full name|your name|customer name"),
                &field))
    return new FullNameField(field);

  return NULL;
}

FirstLastNameField* FirstLastNameField::Parse1(
    std::vector<AutoFillField*>::const_iterator* iter) {
  // Some pages (e.g. Overstock_comBilling.html, SmithsonianCheckout.html)
  // have the label "Name" followed by two or three text fields.
  FirstLastNameField v;
  std::vector<AutoFillField*>::const_iterator q = *iter;

  AutoFillField* next;
  if (ParseText(&q, ASCIIToUTF16("^name"), &v.first_name_) &&
      ParseText(&q, ASCIIToUTF16(""), &next)) {
    if (ParseText(&q, ASCIIToUTF16(""), &v.last_name_)) {
      // There are three name fields; assume that the middle one is a
      // middle initial (it is, at least, on SmithsonianCheckout.html).
      v.middle_name_ = next;
      v.middle_initial_ = true;
    } else {  // only two name fields
      v.last_name_ = next;
    }

    *iter = q;
    return new FirstLastNameField(v);
  }

  return NULL;
}

FirstLastNameField* FirstLastNameField::Parse2(
    std::vector<AutoFillField*>::const_iterator* iter) {
  FirstLastNameField v;
  std::vector<AutoFillField*>::const_iterator q = *iter;

  // A fair number of pages use the names "fname" and "lname" for naming
  // first and last name fields (examples from the test suite:
  // BESTBUY_COM - Sign In2.html; Crate and Barrel Check Out.html;
  // dell_checkout1.html).  At least one UK page (The China Shop2.html)
  // asks, in stuffy English style, for just initials and a surname,
  // so we match "initials" here (and just fill in a first name there,
  // American-style).
  if (!ParseText(&q, ASCIIToUTF16("first name|initials|fname"), &v.first_name_))
    return NULL;

  // We check for a middle initial before checking for a middle name
  // because at least one page (PC Connection.html) has a field marked
  // as both (the label text is "MI" and the element name is
  // "txtmiddlename"); such a field probably actually represents a
  // middle initial.
  if (ParseText(&q, ASCIIToUTF16("^mi|middle initial|m.i."), &v.middle_name_)) {
    v.middle_initial_ = true;
  } else {
    ParseText(&q, ASCIIToUTF16("middle name|mname"), &v.middle_name_);
  }

  if (!ParseText(&q, ASCIIToUTF16("last name|lname|surname"), &v.last_name_))
    return NULL;

  *iter = q;
  return new FirstLastNameField(v);
}

FirstLastNameField* FirstLastNameField::ParseEcmlName(
    std::vector<AutoFillField*>::const_iterator* iter) {
  FirstLastNameField field;
  std::vector<AutoFillField*>::const_iterator q = *iter;

  string16 pattern = GetEcmlPattern(kEcmlShipToFirstName,
                                    kEcmlBillToFirstName, '|');
  if (!ParseText(&q, pattern, &field.first_name_))
    return NULL;

  pattern = GetEcmlPattern(kEcmlShipToMiddleName, kEcmlBillToMiddleName, '|');
  ParseText(&q, pattern, &field.middle_name_);

  pattern = GetEcmlPattern(kEcmlShipToLastName, kEcmlBillToLastName, '|');
  if (ParseText(&q, pattern, &field.last_name_)) {
    *iter = q;
    return new FirstLastNameField(field);
  }

  return NULL;
}

FirstLastNameField* FirstLastNameField::Parse(
    std::vector<AutoFillField*>::const_iterator* iter,
    bool is_ecml) {
  if (is_ecml) {
    return ParseEcmlName(iter);
  } else {
    FirstLastNameField* v = Parse1(iter);
    if (v != NULL)
      return v;

    return Parse2(iter);
  }
}

bool FirstLastNameField::GetFieldInfo(FieldTypeMap* field_type_map) const {
  bool ok = Add(field_type_map, first_name_, AutoFillType(NAME_FIRST));
  DCHECK(ok);
  ok = ok && Add(field_type_map, last_name_, AutoFillType(NAME_LAST));
  DCHECK(ok);
  AutoFillType type = middle_initial_ ?
      AutoFillType(NAME_MIDDLE_INITIAL) : AutoFillType(NAME_MIDDLE);
  ok = ok && Add(field_type_map, middle_name_, type);
  DCHECK(ok);

  return ok;
}

FirstLastNameField::FirstLastNameField()
    : first_name_(NULL),
      middle_name_(NULL),
      last_name_(NULL),
      middle_initial_(false) {
}

FirstLastNameField::FirstLastNameField(const FirstLastNameField& field)
    : first_name_(field.first_name_),
      middle_name_(field.middle_name_),
      last_name_(field.last_name_),
      middle_initial_(field.middle_initial_) {
}
