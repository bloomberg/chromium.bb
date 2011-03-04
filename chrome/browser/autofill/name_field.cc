// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/name_field.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "grit/autofill_resources.h"
#include "ui/base/l10n/l10n_util.h"

NameField* NameField::Parse(std::vector<AutofillField*>::const_iterator* iter,
                            bool is_ecml) {
  // Try FirstLastNameField first since it's more specific.
  NameField* field = FirstLastNameField::Parse(iter, is_ecml);
  if (field == NULL && !is_ecml)
    field = FullNameField::Parse(iter);
  return field;
}

bool FullNameField::GetFieldInfo(FieldTypeMap* field_type_map) const {
  bool ok = Add(field_type_map, field_, AutoFillType(NAME_FULL));
  DCHECK(ok);
  return true;
}

FullNameField* FullNameField::Parse(
    std::vector<AutofillField*>::const_iterator* iter) {
  // Exclude labels containing the string "username", which typically
  // denotes a login ID rather than the user's actual name.
  AutofillField* field = **iter;
  if (Match(field, l10n_util::GetStringUTF16(IDS_AUTOFILL_USERNAME_RE), false))
    return NULL;

  // Searching for any label containing the word "name" is too general;
  // for example, Travelocity_Edit travel profile.html contains a field
  // "Travel Profile Name".
  const string16 name_match = l10n_util::GetStringUTF16(IDS_AUTOFILL_NAME_RE);
  if (ParseText(iter, name_match, &field))
    return new FullNameField(field);

  return NULL;
}

FullNameField::FullNameField(AutofillField* field)
    : field_(field) {
}

FirstLastNameField* FirstLastNameField::Parse1(
    std::vector<AutofillField*>::const_iterator* iter) {
  // Some pages (e.g. Overstock_comBilling.html, SmithsonianCheckout.html)
  // have the label "Name" followed by two or three text fields.
  scoped_ptr<FirstLastNameField> v(new FirstLastNameField);
  std::vector<AutofillField*>::const_iterator q = *iter;

  AutofillField* next;
  if (ParseText(&q,
                l10n_util::GetStringUTF16(IDS_AUTOFILL_NAME_SPECIFIC_RE),
                &v->first_name_) &&
      ParseEmptyText(&q, &next)) {
    if (ParseEmptyText(&q, &v->last_name_)) {
      // There are three name fields; assume that the middle one is a
      // middle initial (it is, at least, on SmithsonianCheckout.html).
      v->middle_name_ = next;
      v->middle_initial_ = true;
    } else {  // only two name fields
      v->last_name_ = next;
    }

    *iter = q;
    return v.release();
  }

  return NULL;
}

FirstLastNameField* FirstLastNameField::Parse2(
    std::vector<AutofillField*>::const_iterator* iter) {
  scoped_ptr<FirstLastNameField> v(new FirstLastNameField);
  std::vector<AutofillField*>::const_iterator q = *iter;

  // A fair number of pages use the names "fname" and "lname" for naming
  // first and last name fields (examples from the test suite:
  // BESTBUY_COM - Sign In2.html; Crate and Barrel Check Out.html;
  // dell_checkout1.html).  At least one UK page (The China Shop2.html)
  // asks, in stuffy English style, for just initials and a surname,
  // so we match "initials" here (and just fill in a first name there,
  // American-style).
  // The ".*first$" matches fields ending in "first" (example in sample8.html).
  string16 match = l10n_util::GetStringUTF16(IDS_AUTOFILL_FIRST_NAME_RE);
  if (!ParseText(&q, match, &v->first_name_))
    return NULL;

  // We check for a middle initial before checking for a middle name
  // because at least one page (PC Connection.html) has a field marked
  // as both (the label text is "MI" and the element name is
  // "txtmiddlename"); such a field probably actually represents a
  // middle initial.
  match = l10n_util::GetStringUTF16(IDS_AUTOFILL_MIDDLE_INITIAL_RE);
  if (ParseText(&q, match, &v->middle_name_)) {
    v->middle_initial_ = true;
  } else {
    match = l10n_util::GetStringUTF16(IDS_AUTOFILL_MIDDLE_NAME_RE);
    ParseText(&q, match, &v->middle_name_);
  }

  // The ".*last$" matches fields ending in "last" (example in sample8.html).
  match = l10n_util::GetStringUTF16(IDS_AUTOFILL_LAST_NAME_RE);
  if (!ParseText(&q, match, &v->last_name_))
    return NULL;

  *iter = q;
  return v.release();
}

FirstLastNameField* FirstLastNameField::ParseEcmlName(
    std::vector<AutofillField*>::const_iterator* iter) {
  scoped_ptr<FirstLastNameField> field(new FirstLastNameField);
  std::vector<AutofillField*>::const_iterator q = *iter;

  string16 pattern = GetEcmlPattern(kEcmlShipToFirstName,
                                    kEcmlBillToFirstName, '|');
  if (!ParseText(&q, pattern, &field->first_name_))
    return NULL;

  pattern = GetEcmlPattern(kEcmlShipToMiddleName, kEcmlBillToMiddleName, '|');
  ParseText(&q, pattern, &field->middle_name_);

  pattern = GetEcmlPattern(kEcmlShipToLastName, kEcmlBillToLastName, '|');
  if (ParseText(&q, pattern, &field->last_name_)) {
    *iter = q;
    return field.release();
  }

  return NULL;
}

FirstLastNameField* FirstLastNameField::Parse(
    std::vector<AutofillField*>::const_iterator* iter,
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

