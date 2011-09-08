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
#include "ui/base/l10n/l10n_util.h"

namespace {

// The UTF-8 version of these regular expressions are in
// regular_expressions.txt.
const char kNameIgnoredRe[] =
    "user.?name|user.?id|nickname|maiden name|title|prefix|suffix"
    // de-DE
    "|vollst\xc3\xa4ndiger.?name"
    // zh-CN
    "|\xe7\x94\xa8\xe6\x88\xb7\xe5\x90\x8d"
    // ko-KR
    "|(\xec\x82\xac\xec\x9a\xa9\xec\x9e\x90.?)?\xec\x95\x84\xec\x9d\xb4\xeb"
        "\x94\x94|\xec\x82\xac\xec\x9a\xa9\xec\x9e\x90.?ID";
const char kNameRe[] =
    "^name|full.?name|your.?name|customer.?name|firstandlastname|bill.?name"
        "|ship.?name"
    // es
    "|nombre.*y.*apellidos"
    // fr-FR
    "|^nom"
    // ja-JP
    "|\xe3\x81\x8a\xe5\x90\x8d\xe5\x89\x8d|\xe6\xb0\x8f\xe5\x90\x8d"
    // pt-BR, pt-PT
    "|^nome"
    // zh-CN
    "|\xe5\xa7\x93\xe5\x90\x8d"
    // ko-KR
    "|\xec\x84\xb1\xeb\xaa\x85";
const char kNameSpecificRe[] =
    "^name"
    // fr-FR
    "|^nom"
    // pt-BR, pt-PT
    "|^nome";
const char kFirstNameRe[] =
    "first.*name|initials|fname|first$"
    // de-DE
    "|vorname"
    // es
    "|nombre"
    // fr-FR
    "|forename|pr\xc3\xa9nom|prenom"
    // ja-JP
    "|\xe5\x90\x8d"
    // pt-BR, pt-PT
    "|nome"
    // ru
    "|\xd0\x98\xd0\xbc\xd1\x8f"
    // ko-KR
    "|\xec\x9d\xb4\xeb\xa6\x84";
const char kMiddleInitialRe[] = "middle.*initial|m\\.i\\.|mi$|\\bmi\\b";
const char kMiddleNameRe[] =
    "middle.*name|mname|middle$"
    // es
    "|apellido.?materno|lastlastname";
const char kLastNameRe[] =
    "last.*name|lname|surname|last$|secondname"
    // de-DE
    "|nachname"
    // es
    "|apellido"
    // fr-FR
    "|famille|^nom"
    // it-IT
    "|cognome"
    // ja-JP
    "|\xe5\xa7\x93"
    // pt-BR, pt-PT
    "|morada|apelidos|surename|sobrenome"
    // ru
    "|\xd0\xa4\xd0\xb0\xd0\xbc\xd0\xb8\xd0\xbb\xd0\xb8\xd1\x8f"
    // ko-KR
    "|\xec\x84\xb1[^\xeb\xaa\x85]?";

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
  static FirstLastNameField* Parse(AutofillScanner* scanner);

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
bool NameField::ClassifyField(FieldTypeMap* map) const {
  return false;
}

FullNameField* FullNameField::Parse(AutofillScanner* scanner) {
  // Exclude e.g. "username" or "nickname" fields.
  scanner->SaveCursor();
  bool should_ignore = ParseField(scanner, UTF8ToUTF16(kNameIgnoredRe), NULL);
  scanner->Rewind();
  if (should_ignore)
    return NULL;

  // Searching for any label containing the word "name" is too general;
  // for example, Travelocity_Edit travel profile.html contains a field
  // "Travel Profile Name".
  const AutofillField* field = NULL;
  if (ParseField(scanner, UTF8ToUTF16(kNameRe), &field))
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
                 UTF8ToUTF16(kNameSpecificRe), &v->first_name_) &&
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
    if (ParseFieldSpecifics(scanner, UTF8ToUTF16(kNameIgnoredRe),
                            MATCH_DEFAULT | MATCH_SELECT, NULL)) {
          continue;
    }

    if (!v->first_name_ &&
        ParseField(scanner, UTF8ToUTF16(kFirstNameRe), &v->first_name_)) {
      continue;
    }

    // We check for a middle initial before checking for a middle name
    // because at least one page (PC Connection.html) has a field marked
    // as both (the label text is "MI" and the element name is
    // "txtmiddlename"); such a field probably actually represents a
    // middle initial.
    if (!v->middle_name_ &&
        ParseField(scanner, UTF8ToUTF16(kMiddleInitialRe), &v->middle_name_)) {
      v->middle_initial_ = true;
      continue;
    }

    if (!v->middle_name_ &&
        ParseField(scanner, UTF8ToUTF16(kMiddleNameRe), &v->middle_name_)) {
      continue;
    }

    if (!v->last_name_ &&
        ParseField(scanner, UTF8ToUTF16(kLastNameRe), &v->last_name_)) {
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

bool FirstLastNameField::ClassifyField(FieldTypeMap* map) const {
  bool ok = AddClassification(first_name_, NAME_FIRST, map);
  ok = ok && AddClassification(last_name_, NAME_LAST, map);
  AutofillFieldType type = middle_initial_ ? NAME_MIDDLE_INITIAL : NAME_MIDDLE;
  ok = ok && AddClassification(middle_name_, type, map);
  return ok;
}
