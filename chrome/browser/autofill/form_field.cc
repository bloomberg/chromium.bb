// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/form_field.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_ecml.h"
#include "chrome/browser/autofill/address_field.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_scanner.h"
#include "chrome/browser/autofill/credit_card_field.h"
#include "chrome/browser/autofill/email_field.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/name_field.h"
#include "chrome/browser/autofill/phone_field.h"
#include "grit/autofill_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using autofill::GetEcmlPattern;
using autofill::HasECMLField;

bool IsTextField(const string16& type) {
  return type == ASCIIToUTF16("text");
}

bool IsEmailField(const string16& type) {
  return type == ASCIIToUTF16("email");
}

bool IsTelephoneField(const string16& type) {
  return type == ASCIIToUTF16("tel");
}

bool IsSelectField(const string16& type) {
  return type == ASCIIToUTF16("select-one");
}

}  // namespace

// static
void FormField::ParseFormFields(const std::vector<AutofillField*>& fields,
                                FieldTypeMap* map) {
  // First, check if there is at least one form field with an ECML name.
  // If there is, then we will match an element only if it is in the standard.
  bool is_ecml = HasECMLField(fields);

  // Set up a working copy of the fields to be processed.
  std::vector<const AutofillField*> remaining_fields(fields.size());
  std::copy(fields.begin(), fields.end(), remaining_fields.begin());

  // Email pass.
  ParseFormFieldsPass(EmailField::Parse, is_ecml, &remaining_fields, map);

  // Phone/fax pass.
  ParseFormFieldsPass(PhoneField::Parse, is_ecml, &remaining_fields, map);

  // Address pass.
  ParseFormFieldsPass(AddressField::Parse, is_ecml, &remaining_fields, map);

  // Credit card pass.
  ParseFormFieldsPass(CreditCardField::Parse, is_ecml, &remaining_fields, map);

  // Name pass.
  ParseFormFieldsPass(NameField::Parse, is_ecml, &remaining_fields, map);
}

// static
bool FormField::ParseField(AutofillScanner* scanner,
                           const string16& pattern,
                           const AutofillField** match) {
  return ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT, match);
}

// static
bool FormField::ParseFieldSpecifics(AutofillScanner* scanner,
                                    const string16& pattern,
                                    int match_type,
                                    const AutofillField** match) {
  if (scanner->IsEnd())
    return false;

  const AutofillField* field = scanner->Cursor();

  if ((match_type & MATCH_TEXT) && IsTextField(field->form_control_type))
    return MatchAndAdvance(scanner, pattern, match_type, match);

  if ((match_type & MATCH_EMAIL) && IsEmailField(field->form_control_type))
    return MatchAndAdvance(scanner, pattern, match_type, match);

  if ((match_type & MATCH_TELEPHONE) &&
      IsTelephoneField(field->form_control_type)) {
    return MatchAndAdvance(scanner, pattern, match_type, match);
  }

  if ((match_type & MATCH_SELECT) && IsSelectField(field->form_control_type))
    return MatchAndAdvance(scanner, pattern, match_type, match);

  return false;
}

// static
bool FormField::ParseEmptyLabel(AutofillScanner* scanner,
                                const AutofillField** match) {
  return ParseFieldSpecifics(scanner,
                             ASCIIToUTF16("^$"),
                             MATCH_LABEL | MATCH_ALL_INPUTS,
                             match);
}

// static
bool FormField::AddClassification(const AutofillField* field,
                                  AutofillFieldType type,
                                  FieldTypeMap* map) {
  // Several fields are optional.
  if (!field)
    return true;

  return map->insert(make_pair(field->unique_name(), type)).second;
}

// static.
bool FormField::MatchAndAdvance(AutofillScanner* scanner,
                                const string16& pattern,
                                int match_type,
                                const AutofillField** match) {
  const AutofillField* field = scanner->Cursor();
  if (FormField::Match(field, pattern, match_type)) {
    if (match)
      *match = field;
    scanner->Advance();
    return true;
  }

  return false;
}

// static
bool FormField::Match(const AutofillField* field,
                      const string16& pattern,
                      int match_type) {
  if ((match_type & FormField::MATCH_LABEL) &&
      autofill::MatchString(field->label, pattern)) {
    return true;
  }

  if ((match_type & FormField::MATCH_NAME) &&
      autofill::MatchString(field->name, pattern)) {
    return true;
  }

  return false;
}

// static
void FormField::ParseFormFieldsPass(ParseFunction parse,
                                    bool is_ecml,
                                    std::vector<const AutofillField*>* fields,
                                    FieldTypeMap* map) {
  // Store unmatched fields for further processing by the caller.
  std::vector<const AutofillField*> remaining_fields;

  AutofillScanner scanner(*fields);
  while (!scanner.IsEnd()) {
    scoped_ptr<FormField> form_field(parse(&scanner, is_ecml));
    if (!form_field.get()) {
      remaining_fields.push_back(scanner.Cursor());
      scanner.Advance();
      continue;
    }

    // Add entries into the map for each field type found in |form_field|.
    bool ok = form_field->ClassifyField(map);
    DCHECK(ok);
  }

  std::swap(*fields, remaining_fields);
}
