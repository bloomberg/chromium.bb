// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_field.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_field.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_regexes.h"
#include "components/autofill/core/browser/autofill_scanner.h"
#include "components/autofill/core/browser/credit_card_field.h"
#include "components/autofill/core/browser/email_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/name_field.h"
#include "components/autofill/core/browser/phone_field.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

bool IsCheckable(const AutofillField* field) {
  return field->is_checkable;
}

}  // namespace

// static
void FormField::ParseFormFields(const std::vector<AutofillField*>& fields,
                                ServerFieldTypeMap* map) {
  // Set up a working copy of the fields to be processed.
  std::vector<const AutofillField*> remaining_fields(fields.size());
  std::copy(fields.begin(), fields.end(), remaining_fields.begin());

  // Ignore checkable fields as they interfere with parsers assuming context.
  // Eg., while parsing address, "Is PO box" checkbox after ADDRESS_LINE1
  // interferes with correctly understanding ADDRESS_LINE2.
  remaining_fields.erase(
      std::remove_if(remaining_fields.begin(), remaining_fields.end(),
                     IsCheckable),
      remaining_fields.end());

  // Email pass.
  ParseFormFieldsPass(EmailField::Parse, &remaining_fields, map);

  // Phone pass.
  ParseFormFieldsPass(PhoneField::Parse, &remaining_fields, map);

  // Address pass.
  ParseFormFieldsPass(AddressField::Parse, &remaining_fields, map);

  // Credit card pass.
  ParseFormFieldsPass(CreditCardField::Parse, &remaining_fields, map);

  // Name pass.
  ParseFormFieldsPass(NameField::Parse, &remaining_fields, map);
}

// static
bool FormField::ParseField(AutofillScanner* scanner,
                           const base::string16& pattern,
                           const AutofillField** match) {
  return ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT, match);
}

// static
bool FormField::ParseFieldSpecifics(AutofillScanner* scanner,
                                    const base::string16& pattern,
                                    int match_type,
                                    const AutofillField** match) {
  if (scanner->IsEnd())
    return false;

  const AutofillField* field = scanner->Cursor();

  if (!MatchesFormControlType(field->form_control_type, match_type))
    return false;

  return MatchAndAdvance(scanner, pattern, match_type, match);
}

// static
bool FormField::ParseEmptyLabel(AutofillScanner* scanner,
                                const AutofillField** match) {
  return ParseFieldSpecifics(scanner,
                             base::ASCIIToUTF16("^$"),
                             MATCH_LABEL | MATCH_ALL_INPUTS,
                             match);
}

// static
bool FormField::AddClassification(const AutofillField* field,
                                  ServerFieldType type,
                                  ServerFieldTypeMap* map) {
  // Several fields are optional.
  if (!field)
    return true;

  return map->insert(make_pair(field->unique_name(), type)).second;
}

// static.
bool FormField::MatchAndAdvance(AutofillScanner* scanner,
                                const base::string16& pattern,
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
                      const base::string16& pattern,
                      int match_type) {
  if ((match_type & FormField::MATCH_LABEL) &&
      autofill::MatchesPattern(field->label, pattern)) {
    return true;
  }

  if ((match_type & FormField::MATCH_NAME) &&
      autofill::MatchesPattern(field->name, pattern)) {
    return true;
  }

  if ((match_type & FormField::MATCH_VALUE) &&
      autofill::MatchesPattern(field->value, pattern)) {
    return true;
  }

  return false;
}

// static
void FormField::ParseFormFieldsPass(ParseFunction parse,
                                    std::vector<const AutofillField*>* fields,
                                    ServerFieldTypeMap* map) {
  // Store unmatched fields for further processing by the caller.
  std::vector<const AutofillField*> remaining_fields;

  AutofillScanner scanner(*fields);
  while (!scanner.IsEnd()) {
    scoped_ptr<FormField> form_field(parse(&scanner));
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

bool FormField::MatchesFormControlType(const std::string& type,
                                       int match_type) {
  if ((match_type & MATCH_TEXT) && type == "text")
    return true;

  if ((match_type & MATCH_EMAIL) && type == "email")
    return true;

  if ((match_type & MATCH_TELEPHONE) && type == "tel")
    return true;

  if ((match_type & MATCH_SELECT) && type == "select-one")
    return true;

  if ((match_type & MATCH_TEXT_AREA) && type == "textarea")
    return true;

  return false;
}

}  // namespace autofill
