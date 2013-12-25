// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/name_field.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "components/autofill/core/browser/autofill_scanner.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "ui/base/l10n/l10n_util.h"

using base::UTF8ToUTF16;

namespace autofill {
namespace {

// A form field that can parse a full name field.
class FullNameField : public NameField {
 public:
  static FullNameField* Parse(AutofillScanner* scanner);

 protected:
  // FormField:
  virtual bool ClassifyField(ServerFieldTypeMap* map) const OVERRIDE;

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
  static FirstLastNameField* Parse(AutofillScanner* scanner);

 protected:
  // FormField:
  virtual bool ClassifyField(ServerFieldTypeMap* map) const OVERRIDE;

 private:
  FirstLastNameField();

  const AutofillField* first_name_;
  const AutofillField* middle_name_;  // Optional.
  const AutofillField* last_name_;
  bool middle_initial_;  // True if middle_name_ is a middle initial.

  DISALLOW_COPY_AND_ASSIGN(FirstLastNameField);
};

}  // namespace

FormField* NameField::Parse(AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return NULL;

  // Try FirstLastNameField first since it's more specific.
  NameField* field = FirstLastNameField::Parse(scanner);
  if (!field)
    field = FullNameField::Parse(scanner);
  return field;
}

// This is overriden in concrete subclasses.
bool NameField::ClassifyField(ServerFieldTypeMap* map) const {
  return false;
}

FullNameField* FullNameField::Parse(AutofillScanner* scanner) {
  // Exclude e.g. "username" or "nickname" fields.
  scanner->SaveCursor();
  bool should_ignore = ParseField(scanner,
                                  UTF8ToUTF16(autofill::kNameIgnoredRe), NULL);
  scanner->Rewind();
  if (should_ignore)
    return NULL;

  // Searching for any label containing the word "name" is too general;
  // for example, Travelocity_Edit travel profile.html contains a field
  // "Travel Profile Name".
  const AutofillField* field = NULL;
  if (ParseField(scanner, UTF8ToUTF16(autofill::kNameRe), &field))
    return new FullNameField(field);

  return NULL;
}

bool FullNameField::ClassifyField(ServerFieldTypeMap* map) const {
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

  const AutofillField* next = NULL;
  if (ParseField(scanner,
                 UTF8ToUTF16(autofill::kNameSpecificRe), &v->first_name_) &&
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
  // The ".*last$" matches fields ending in "last" (example in sample8.html).

  // Allow name fields to appear in any order.
  while (!scanner->IsEnd()) {
    // Skip over any unrelated fields, e.g. "username" or "nickname".
    if (ParseFieldSpecifics(scanner, UTF8ToUTF16(autofill::kNameIgnoredRe),
                            MATCH_DEFAULT | MATCH_SELECT, NULL)) {
          continue;
    }

    if (!v->first_name_ &&
        ParseField(scanner, UTF8ToUTF16(autofill::kFirstNameRe),
                   &v->first_name_)) {
      continue;
    }

    // We check for a middle initial before checking for a middle name
    // because at least one page (PC Connection.html) has a field marked
    // as both (the label text is "MI" and the element name is
    // "txtmiddlename"); such a field probably actually represents a
    // middle initial.
    if (!v->middle_name_ &&
        ParseField(scanner, UTF8ToUTF16(autofill::kMiddleInitialRe),
                   &v->middle_name_)) {
      v->middle_initial_ = true;
      continue;
    }

    if (!v->middle_name_ &&
        ParseField(scanner, UTF8ToUTF16(autofill::kMiddleNameRe),
                   &v->middle_name_)) {
      continue;
    }

    if (!v->last_name_ &&
        ParseField(scanner, UTF8ToUTF16(autofill::kLastNameRe),
                   &v->last_name_)) {
      continue;
    }

    break;
  }

  // Consider the match to be successful if we detected both first and last name
  // fields.
  if (v->first_name_ && v->last_name_)
    return v.release();

  scanner->Rewind();
  return NULL;
}

FirstLastNameField* FirstLastNameField::Parse(AutofillScanner* scanner) {
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

bool FirstLastNameField::ClassifyField(ServerFieldTypeMap* map) const {
  bool ok = AddClassification(first_name_, NAME_FIRST, map);
  ok = ok && AddClassification(last_name_, NAME_LAST, map);
  ServerFieldType type = middle_initial_ ? NAME_MIDDLE_INITIAL : NAME_MIDDLE;
  ok = ok && AddClassification(middle_name_, type, map);
  return ok;
}

}  // namespace autofill
