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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRegularExpression.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCaseSensitivity.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using autofill::GetEcmlPattern;
using autofill::HasECMLField;

bool MatchName(const AutofillField* field, const string16& pattern) {
  // TODO(jhawkins): Remove StringToLowerASCII.  WebRegularExpression needs to
  // be fixed to take WebTextCaseInsensitive into account.
  WebKit::WebRegularExpression re(WebKit::WebString(pattern),
                                  WebKit::WebTextCaseInsensitive);
  bool match = re.match(
      WebKit::WebString(StringToLowerASCII(field->name))) != -1;
  return match;
}

bool MatchLabel(const AutofillField* field, const string16& pattern) {
  // TODO(jhawkins): Remove StringToLowerASCII.  WebRegularExpression needs to
  // be fixed to take WebTextCaseInsensitive into account.
  WebKit::WebRegularExpression re(WebKit::WebString(pattern),
                                  WebKit::WebTextCaseInsensitive);
  bool match = re.match(
      WebKit::WebString(StringToLowerASCII(field->label))) != -1;
  return match;
}

// Parses a field using the different field views we know about.  |is_ecml|
// should be true when the field conforms to the ECML specification.
FormField* ParseFormField(AutofillScanner* scanner, bool is_ecml) {
  FormField* field;

  field = EmailField::Parse(scanner, is_ecml);
  if (field)
    return field;

  // Parses both phone and fax.
  field = PhoneField::Parse(scanner, is_ecml);
  if (field)
    return field;

  field = AddressField::Parse(scanner, is_ecml);
  if (field)
    return field;

  field = CreditCardField::Parse(scanner, is_ecml);
  if (field)
    return field;

  // We search for a |NameField| last since it matches the word "name", which is
  // relatively general.
  return NameField::Parse(scanner, is_ecml);
}

}  // namespace

// static
void FormField::ParseFormFields(const std::vector<AutofillField*>& fields,
                                FieldTypeMap* map) {
  // First, check if there is at least one form field with an ECML name.
  // If there is, then we will match an element only if it is in the standard.
  bool is_ecml = HasECMLField(fields);

  // Parse fields.
  AutofillScanner scanner(fields);
  while (!scanner.IsEnd()) {
    scoped_ptr<FormField> form_field(ParseFormField(&scanner, is_ecml));
    if (!form_field.get()) {
      scanner.Advance();
      continue;
    }

    // Add entries into the map for each field type found in |form_field|.
    bool ok = form_field->ClassifyField(map);
    DCHECK(ok);
  }
}

// static
bool FormField::ParseField(AutofillScanner* scanner,
                           const string16& pattern,
                           const AutofillField** match) {
  return ParseFieldSpecifics(scanner, pattern, MATCH_ALL, match);
}

// static
bool FormField::ParseFieldSpecifics(AutofillScanner* scanner,
                                    const string16& pattern,
                                    int match_type,
                                    const AutofillField** match) {
  if (scanner->IsEnd())
    return false;

  const AutofillField* field = scanner->Cursor();
  if (Match(field, pattern, match_type)) {
    if (match)
      *match = field;
    scanner->Advance();
    return true;
  }

  return false;
}

// static
bool FormField::ParseEmptyLabel(AutofillScanner* scanner,
                                const AutofillField** match) {
  return ParseFieldSpecifics(scanner, ASCIIToUTF16("^$"), MATCH_LABEL, match);
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

// static
bool FormField::Match(const AutofillField* field,
                      const string16& pattern,
                      int match_type) {
  if ((match_type & FormField::MATCH_LABEL) && MatchLabel(field, pattern))
      return true;

  if ((match_type & FormField::MATCH_NAME) && MatchName(field, pattern))
      return true;

  return false;
}
