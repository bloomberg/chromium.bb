// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/name_field.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_scanner.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "grit/autofill_resources.h"
#include "ui/base/l10n/l10n_util.h"

NameField* NameField::Parse(AutofillScanner* scanner, bool is_ecml) {
  if (scanner->IsEnd())
    return NULL;

  // Try FirstLastNameField first since it's more specific.
  NameField* field = FirstLastNameField::Parse(scanner, is_ecml);
  if (!field && !is_ecml)
    field = FullNameField::Parse(scanner);
  return field;
}

bool FullNameField::GetFieldInfo(FieldTypeMap* field_type_map) const {
  return Add(field_type_map, field_, NAME_FULL);
}

FullNameField* FullNameField::Parse(AutofillScanner* scanner) {
  // Exclude labels containing the string "username", which typically
  // denotes a login ID rather than the user's actual name.
  const AutofillField* field = scanner->Cursor();
  if (Match(field, l10n_util::GetStringUTF16(IDS_AUTOFILL_USERNAME_RE), false))
    return NULL;

  // Searching for any label containing the word "name" is too general;
  // for example, Travelocity_Edit travel profile.html contains a field
  // "Travel Profile Name".
  if (ParseText(scanner, l10n_util::GetStringUTF16(IDS_AUTOFILL_NAME_RE),
                &field))
    return new FullNameField(field);

  return NULL;
}

FullNameField::FullNameField(const AutofillField* field)
    : field_(field) {
}

bool FirstLastNameField::GetFieldInfo(FieldTypeMap* field_type_map) const {
  bool ok = Add(field_type_map, first_name_, NAME_FIRST);
  ok = ok && Add(field_type_map, last_name_, NAME_LAST);
  AutofillFieldType type = middle_initial_ ? NAME_MIDDLE_INITIAL : NAME_MIDDLE;
  ok = ok && Add(field_type_map, middle_name_, type);
  return ok;
}

FirstLastNameField* FirstLastNameField::ParseSpecificName(
    AutofillScanner* scanner) {
  // Some pages (e.g. Overstock_comBilling.html, SmithsonianCheckout.html)
  // have the label "Name" followed by two or three text fields.
  scoped_ptr<FirstLastNameField> v(new FirstLastNameField);
  scanner->SaveCursor();

  const AutofillField* next;
  if (ParseText(scanner,
                l10n_util::GetStringUTF16(IDS_AUTOFILL_NAME_SPECIFIC_RE),
                &v->first_name_) &&
      ParseEmptyText(scanner, &next)) {
    if (ParseEmptyText(scanner, &v->last_name_)) {
      // There are three name fields; assume that the middle one is a
      // middle initial (it is, at least, on SmithsonianCheckout.html).
      v->middle_name_ = next;
      v->middle_initial_ = true;
    } else {  // only two name fields
      v->last_name_ = next;
    }

    return v.release();
  }

  scanner->Rewind();
  return NULL;
}

FirstLastNameField* FirstLastNameField::ParseComponentNames(
    AutofillScanner* scanner) {
  scoped_ptr<FirstLastNameField> v(new FirstLastNameField);
  scanner->SaveCursor();

  // A fair number of pages use the names "fname" and "lname" for naming
  // first and last name fields (examples from the test suite:
  // BESTBUY_COM - Sign In2.html; Crate and Barrel Check Out.html;
  // dell_checkout1.html).  At least one UK page (The China Shop2.html)
  // asks, in stuffy English style, for just initials and a surname,
  // so we match "initials" here (and just fill in a first name there,
  // American-style).
  // The ".*first$" matches fields ending in "first" (example in sample8.html).
  if (!ParseText(scanner, l10n_util::GetStringUTF16(IDS_AUTOFILL_FIRST_NAME_RE),
                 &v->first_name_))
    return NULL;

  // We check for a middle initial before checking for a middle name
  // because at least one page (PC Connection.html) has a field marked
  // as both (the label text is "MI" and the element name is
  // "txtmiddlename"); such a field probably actually represents a
  // middle initial.
  if (ParseText(scanner,
                l10n_util::GetStringUTF16(IDS_AUTOFILL_MIDDLE_INITIAL_RE),
                &v->middle_name_)) {
    v->middle_initial_ = true;
  } else {
    ParseText(scanner, l10n_util::GetStringUTF16(IDS_AUTOFILL_MIDDLE_NAME_RE),
              &v->middle_name_);
  }

  // The ".*last$" matches fields ending in "last" (example in sample8.html).
  if (ParseText(scanner, l10n_util::GetStringUTF16(IDS_AUTOFILL_LAST_NAME_RE),
                &v->last_name_)) {
    return v.release();
  }

  scanner->Rewind();
  return NULL;
}

FirstLastNameField* FirstLastNameField::ParseEcmlName(
    AutofillScanner* scanner) {
  scoped_ptr<FirstLastNameField> field(new FirstLastNameField);
  scanner->SaveCursor();

  string16 pattern = GetEcmlPattern(kEcmlShipToFirstName,
                                    kEcmlBillToFirstName, '|');
  if (!ParseText(scanner, pattern, &field->first_name_))
    return NULL;

  pattern = GetEcmlPattern(kEcmlShipToMiddleName, kEcmlBillToMiddleName, '|');
  ParseText(scanner, pattern, &field->middle_name_);

  pattern = GetEcmlPattern(kEcmlShipToLastName, kEcmlBillToLastName, '|');
  if (ParseText(scanner, pattern, &field->last_name_))
    return field.release();

  scanner->Rewind();
  return NULL;
}

FirstLastNameField* FirstLastNameField::Parse(AutofillScanner* scanner,
                                              bool is_ecml) {
  if (is_ecml)
    return ParseEcmlName(scanner);

  FirstLastNameField* field = ParseSpecificName(scanner);
  if (!field)
    field = ParseComponentNames(scanner);
  return field;
}

FirstLastNameField::FirstLastNameField()
    : first_name_(NULL),
      middle_name_(NULL),
      last_name_(NULL),
      middle_initial_(false) {
}
