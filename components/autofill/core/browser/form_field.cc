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
#include "components/autofill/core/browser/autofill_scanner.h"
#include "components/autofill/core/browser/credit_card_field.h"
#include "components/autofill/core/browser/email_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/name_field.h"
#include "components/autofill/core/browser/phone_field.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {
namespace {

bool ShouldBeIgnored(const AutofillField* field) {
  // Ignore checkable fields as they interfere with parsers assuming context.
  // Eg., while parsing address, "Is PO box" checkbox after ADDRESS_LINE1
  // interferes with correctly understanding ADDRESS_LINE2.
  // Ignore fields marked as presentational. See
  // http://www.w3.org/TR/wai-aria/roles#presentation
  return field->is_checkable ||
         field->role == FormFieldData::ROLE_ATTRIBUTE_PRESENTATION;
}

}  // namespace

// static
void FormField::ParseFormFields(const std::vector<AutofillField*>& fields,
                                bool is_form_tag,
                                ServerFieldTypeMap* map) {
  DCHECK(map->empty());

  // Set up a working copy of the fields to be processed.
  std::vector<AutofillField*> remaining_fields(fields.size());
  std::copy(fields.begin(), fields.end(), remaining_fields.begin());

  remaining_fields.erase(
      std::remove_if(remaining_fields.begin(), remaining_fields.end(),
                     ShouldBeIgnored),
      remaining_fields.end());

  ServerFieldTypeMap saved_map = *map;

  // Email pass.
  ParseFormFieldsPass(EmailField::Parse, &remaining_fields, map);
  size_t email_count = map->size();

  // Phone pass.
  ParseFormFieldsPass(PhoneField::Parse, &remaining_fields, map);

  // Address pass.
  ParseFormFieldsPass(AddressField::Parse, &remaining_fields, map);

  // Credit card pass.
  ParseFormFieldsPass(CreditCardField::Parse, &remaining_fields, map);

  // Name pass.
  ParseFormFieldsPass(NameField::Parse, &remaining_fields, map);

  // Do not autofill a form if there are less than 3 recognized fields.
  // Otherwise it is very easy to have false positives. http://crbug.com/447332
  // For <form> tags, make an exception for email fields, which are commonly the
  // only recognized field on account registration sites.
  size_t kThreshold = 3;
  bool accept_parsing = (map->size() >= kThreshold ||
                         (is_form_tag && email_count > 0));
  if (!accept_parsing)
    *map = saved_map;
}

// static
bool FormField::ParseField(AutofillScanner* scanner,
                           const base::string16& pattern,
                           AutofillField** match) {
  return ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT, match);
}

// static
bool FormField::ParseFieldSpecifics(AutofillScanner* scanner,
                                    const base::string16& pattern,
                                    int match_type,
                                    AutofillField** match) {
  if (scanner->IsEnd())
    return false;

  const AutofillField* field = scanner->Cursor();

  if (!MatchesFormControlType(field->form_control_type, match_type))
    return false;

  return MatchAndAdvance(scanner, pattern, match_type, match);
}

// static
FormField::ParseNameLabelResult FormField::ParseNameAndLabelSeparately(
    AutofillScanner* scanner,
    const base::string16& pattern,
    int match_type,
    AutofillField** match) {
  if (scanner->IsEnd())
    return RESULT_MATCH_NONE;

  AutofillField* cur_match = nullptr;
  size_t saved_cursor = scanner->SaveCursor();
  bool parsed_name = ParseFieldSpecifics(scanner,
                                         pattern,
                                         match_type & ~MATCH_LABEL,
                                         &cur_match);
  scanner->RewindTo(saved_cursor);
  bool parsed_label = ParseFieldSpecifics(scanner,
                                          pattern,
                                          match_type & ~MATCH_NAME,
                                          &cur_match);
  if (parsed_name && parsed_label) {
    if (match)
      *match = cur_match;
    return RESULT_MATCH_NAME_LABEL;
  }

  scanner->RewindTo(saved_cursor);
  if (parsed_name)
    return RESULT_MATCH_NAME;
  if (parsed_label)
    return RESULT_MATCH_LABEL;
  return RESULT_MATCH_NONE;
}

// static
bool FormField::ParseEmptyLabel(AutofillScanner* scanner,
                                AutofillField** match) {
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
                                AutofillField** match) {
  AutofillField* field = scanner->Cursor();
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
      MatchesPattern(field->label, pattern)) {
    return true;
  }

  if ((match_type & FormField::MATCH_NAME) &&
      MatchesPattern(field->parseable_name(), pattern)) {
    return true;
  }

  return false;
}

// static
void FormField::ParseFormFieldsPass(ParseFunction parse,
                                    std::vector<AutofillField*>* fields,
                                    ServerFieldTypeMap* map) {
  // Store unmatched fields for further processing by the caller.
  std::vector<AutofillField*> remaining_fields;

  AutofillScanner scanner(*fields);
  while (!scanner.IsEnd()) {
    scoped_ptr<FormField> form_field(parse(&scanner));
    if (!form_field) {
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

  if ((match_type & MATCH_PASSWORD) && type == "password")
    return true;

  if ((match_type & MATCH_NUMBER) && type == "number")
    return true;

  return false;
}

}  // namespace autofill
