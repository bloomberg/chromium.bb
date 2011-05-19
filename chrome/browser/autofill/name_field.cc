// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/name_field.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_ecml.h"
#include "chrome/browser/autofill/autofill_scanner.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "grit/autofill_resources.h"
#include "ui/base/l10n/l10n_util.h"

using autofill::GetEcmlPattern;

namespace {

// A form field that can parse a full name field.
class FullNameField : public NameField {
 public:
  static FullNameField* Parse(AutofillScanner* scanner);

 protected:
  // FormField:
  virtual bool ClassifyField(FieldTypeMap* map) const OVERRIDE;

 private:
  explicit FullNameField(const AutofillField* field);

  const AutofillField* field_;

  DISALLOW_COPY_AND_ASSIGN(FullNameField);
};

// A form field that can parse a first and last name field.
class FirstLastNameField : public NameField {
 public:
  static FirstLastNameField* ParseSpecificName(AutofillScanner* scanner);
  static FirstLastNameField* ParseComponentNames(AutofillScanner* scanner);
  static FirstLastNameField* ParseEcmlName(AutofillScanner* scanner);
  static FirstLastNameField* Parse(AutofillScanner* scanner, bool is_ecml);

 protected:
  // FormField:
  virtual bool ClassifyField(FieldTypeMap* map) const OVERRIDE;

 private:
  FirstLastNameField();

  const AutofillField* first_name_;
  const AutofillField* middle_name_;  // Optional.
  const AutofillField* last_name_;
  bool middle_initial_;  // True if middle_name_ is a middle initial.

  DISALLOW_COPY_AND_ASSIGN(FirstLastNameField);
};

}  // namespace

NameField* NameField::Parse(AutofillScanner* scanner, bool is_ecml) {
  if (scanner->IsEnd())
    return NULL;

  // Try FirstLastNameField first since it's more specific.
  NameField* field = FirstLastNameField::Parse(scanner, is_ecml);
  if (!field && !is_ecml)
    field = FullNameField::Parse(scanner);
  return field;
}

// This is overriden in concrete subclasses.
bool NameField::ClassifyField(FieldTypeMap* map) const {
  return false;
}

FullNameField* FullNameField::Parse(AutofillScanner* scanner) {
  // Exclude labels containing the string "username", which typically
  // denotes a login ID rather than the user's actual name.
  scanner->SaveCursor();
  bool is_username = ParseField(
      scanner, l10n_util::GetStringUTF16(IDS_AUTOFILL_USERNAME_RE), NULL);
  scanner->Rewind();
  if (is_username)
    return NULL;

  // Searching for any label containing the word "name" is too general;
  // for example, Travelocity_Edit travel profile.html contains a field
  // "Travel Profile Name".
  const AutofillField* field = NULL;
  if (ParseField(scanner, l10n_util::GetStringUTF16(IDS_AUTOFILL_NAME_RE),
                 &field))
    return new FullNameField(field);

  return NULL;
}

bool FullNameField::ClassifyField(FieldTypeMap* map) const {
  return AddClassification(field_, NAME_FULL, map);
}

FullNameField::FullNameField(const AutofillField* field)
    : field_(field) {
}

FirstLastNameField* FirstLastNameField::ParseSpecificName(
    AutofillScanner* scanner) {
  // Some pages (e.g. Overstock_comBilling.html, SmithsonianCheckout.html)
  // have the label "Name" followed by two or three text fields.
  scoped_ptr<FirstLastNameField> v(new FirstLastNameField);
  scanner->SaveCursor();

  const AutofillField* next;
  if (ParseField(scanner,
                 l10n_util::GetStringUTF16(IDS_AUTOFILL_NAME_SPECIFIC_RE),
                 &v->first_name_) &&
      ParseEmptyLabel(scanner, &next)) {
    if (ParseEmptyLabel(scanner, &v->last_name_)) {
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
  if (!ParseField(scanner,
                  l10n_util::GetStringUTF16(IDS_AUTOFILL_FIRST_NAME_RE),
                  &v->first_name_)) {
    return NULL;
  }

  // We check for a middle initial before checking for a middle name
  // because at least one page (PC Connection.html) has a field marked
  // as both (the label text is "MI" and the element name is
  // "txtmiddlename"); such a field probably actually represents a
  // middle initial.
  if (ParseField(scanner,
                l10n_util::GetStringUTF16(IDS_AUTOFILL_MIDDLE_INITIAL_RE),
                &v->middle_name_)) {
    v->middle_initial_ = true;
  } else {
    ParseField(scanner, l10n_util::GetStringUTF16(IDS_AUTOFILL_MIDDLE_NAME_RE),
              &v->middle_name_);
  }

  // The ".*last$" matches fields ending in "last" (example in sample8.html).
  if (ParseField(scanner, l10n_util::GetStringUTF16(IDS_AUTOFILL_LAST_NAME_RE),
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
  if (!ParseField(scanner, pattern, &field->first_name_))
    return NULL;

  pattern = GetEcmlPattern(kEcmlShipToMiddleName, kEcmlBillToMiddleName, '|');
  ParseField(scanner, pattern, &field->middle_name_);

  pattern = GetEcmlPattern(kEcmlShipToLastName, kEcmlBillToLastName, '|');
  if (ParseField(scanner, pattern, &field->last_name_))
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

bool FirstLastNameField::ClassifyField(FieldTypeMap* map) const {
  bool ok = AddClassification(first_name_, NAME_FIRST, map);
  ok = ok && AddClassification(last_name_, NAME_LAST, map);
  AutofillFieldType type = middle_initial_ ? NAME_MIDDLE_INITIAL : NAME_MIDDLE;
  ok = ok && AddClassification(middle_name_, type, map);
  return ok;
}
