// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/phone_field.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_regex_constants.h"
#include "chrome/browser/autofill/autofill_scanner.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// This string includes all area code separators, including NoText.
string16 GetAreaRegex() {
  string16 area_code = UTF8ToUTF16(autofill::kAreaCodeRe);
  area_code.append(ASCIIToUTF16("|"));  // Regexp separator.
  area_code.append(UTF8ToUTF16(autofill::kAreaCodeNotextRe));
  return area_code;
}

}  // namespace

// Phone field grammars - first matched grammar will be parsed. Grammars are
// separated by { REGEX_SEPARATOR, FIELD_NONE, 0 }. Suffix and extension are
// parsed separately unless they are necessary parts of the match.
// The following notation is used to describe the patterns:
// <cc> - country code field.
// <ac> - area code field.
// <phone> - phone or prefix.
// <suffix> - suffix.
// <ext> - extension.
// :N means field is limited to N characters, otherwise it is unlimited.
// (pattern <field>)? means pattern is optional and matched separately.
PhoneField::Parser PhoneField::phone_field_grammars_[] = {
  // Country code: <cc> Area Code: <ac> Phone: <phone> (- <suffix>
  // (Ext: <ext>)?)?
  { PhoneField::REGEX_COUNTRY, PhoneField::FIELD_COUNTRY_CODE, 0 },
  { PhoneField::REGEX_AREA, PhoneField::FIELD_AREA_CODE, 0 },
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_PHONE, 0 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // \( <ac> \) <phone>:3 <suffix>:4 (Ext: <ext>)?
  { PhoneField::REGEX_AREA_NOTEXT, PhoneField::FIELD_AREA_CODE, 3 },
  { PhoneField::REGEX_PREFIX_SEPARATOR, PhoneField::FIELD_PHONE, 3 },
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_SUFFIX, 4 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <cc> <ac>:3 - <phone>:3 - <suffix>:4 (Ext: <ext>)?
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_COUNTRY_CODE, 0 },
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_AREA_CODE, 3 },
  { PhoneField::REGEX_PREFIX_SEPARATOR, PhoneField::FIELD_PHONE, 3 },
  { PhoneField::REGEX_SUFFIX_SEPARATOR, PhoneField::FIELD_SUFFIX, 4 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <cc>:3 <ac>:3 <phone>:3 <suffix>:4 (Ext: <ext>)?
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_COUNTRY_CODE, 3 },
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_AREA_CODE, 3 },
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_PHONE, 3 },
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_SUFFIX, 4 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Area Code: <ac> Phone: <phone> (- <suffix> (Ext: <ext>)?)?
  { PhoneField::REGEX_AREA, PhoneField::FIELD_AREA_CODE, 0 },
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_PHONE, 0 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <ac> <phone>:3 <suffix>:4 (Ext: <ext>)?
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_AREA_CODE, 0 },
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_PHONE, 3 },
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_SUFFIX, 4 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <cc> \( <ac> \) <phone> (- <suffix> (Ext: <ext>)?)?
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_COUNTRY_CODE, 0 },
  { PhoneField::REGEX_AREA_NOTEXT, PhoneField::FIELD_AREA_CODE, 0 },
  { PhoneField::REGEX_PREFIX_SEPARATOR, PhoneField::FIELD_PHONE, 0 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: \( <ac> \) <phone> (- <suffix> (Ext: <ext>)?)?
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_COUNTRY_CODE, 0 },
  { PhoneField::REGEX_AREA_NOTEXT, PhoneField::FIELD_AREA_CODE, 0 },
  { PhoneField::REGEX_PREFIX_SEPARATOR, PhoneField::FIELD_PHONE, 0 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <cc> - <ac> - <phone> - <suffix> (Ext: <ext>)?
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_COUNTRY_CODE, 0 },
  { PhoneField::REGEX_PREFIX_SEPARATOR, PhoneField::FIELD_AREA_CODE, 0 },
  { PhoneField::REGEX_PREFIX_SEPARATOR, PhoneField::FIELD_PHONE, 0 },
  { PhoneField::REGEX_SUFFIX_SEPARATOR, PhoneField::FIELD_SUFFIX, 0 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <ac> Prefix: <phone> Suffix: <suffix> (Ext: <ext>)?
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_AREA_CODE, 0 },
  { PhoneField::REGEX_PREFIX, PhoneField::FIELD_PHONE, 0 },
  { PhoneField::REGEX_SUFFIX, PhoneField::FIELD_SUFFIX, 0 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <ac> - <phone>:3 - <suffix>:4 (Ext: <ext>)?
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_AREA_CODE, 0 },
  { PhoneField::REGEX_PREFIX_SEPARATOR, PhoneField::FIELD_PHONE, 3 },
  { PhoneField::REGEX_SUFFIX_SEPARATOR, PhoneField::FIELD_SUFFIX, 4 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <cc> - <ac> - <phone> (Ext: <ext>)?
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_COUNTRY_CODE, 0 },
  { PhoneField::REGEX_PREFIX_SEPARATOR, PhoneField::FIELD_AREA_CODE, 0 },
  { PhoneField::REGEX_SUFFIX_SEPARATOR, PhoneField::FIELD_PHONE, 0 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <ac> - <phone> (Ext: <ext>)?
  { PhoneField::REGEX_AREA, PhoneField::FIELD_AREA_CODE, 0 },
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_PHONE, 0 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <cc>:3 - <phone>:10 (Ext: <ext>)?
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_COUNTRY_CODE, 3 },
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_PHONE, 10 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <phone> (Ext: <ext>)?
  { PhoneField::REGEX_PHONE, PhoneField::FIELD_PHONE, 0 },
  { PhoneField::REGEX_SEPARATOR, FIELD_NONE, 0 },
};

PhoneField::~PhoneField() {}

// static
FormField* PhoneField::Parse(AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return NULL;

  scoped_ptr<PhoneField> phone_field(new PhoneField);
  if (ParseInternal(phone_field.get(), scanner))
    return phone_field.release();

  return NULL;
}

PhoneField::PhoneField() {
  memset(parsed_phone_fields_, 0, sizeof(parsed_phone_fields_));
}

bool PhoneField::ClassifyField(FieldTypeMap* map) const {
  bool ok = true;

  DCHECK(parsed_phone_fields_[FIELD_PHONE]);  // Phone was correctly parsed.

  if ((parsed_phone_fields_[FIELD_COUNTRY_CODE] != NULL) ||
      (parsed_phone_fields_[FIELD_AREA_CODE] != NULL) ||
      (parsed_phone_fields_[FIELD_SUFFIX] != NULL)) {
    if (parsed_phone_fields_[FIELD_COUNTRY_CODE] != NULL) {
      ok = ok && AddClassification(parsed_phone_fields_[FIELD_COUNTRY_CODE],
                                   PHONE_HOME_COUNTRY_CODE,
                                   map);
    }

    AutofillFieldType field_number_type = PHONE_HOME_NUMBER;
    if (parsed_phone_fields_[FIELD_AREA_CODE] != NULL) {
      ok = ok && AddClassification(parsed_phone_fields_[FIELD_AREA_CODE],
                                   PHONE_HOME_CITY_CODE,
                                   map);
    } else if (parsed_phone_fields_[FIELD_COUNTRY_CODE] != NULL) {
      // Only if we can find country code without city code, it means the phone
      // number include city code.
      field_number_type = PHONE_HOME_CITY_AND_NUMBER;
    }
    // We tag the prefix as PHONE_HOME_NUMBER, then when filling the form
    // we fill only the prefix depending on the size of the input field.
    ok = ok && AddClassification(parsed_phone_fields_[FIELD_PHONE],
                                 field_number_type,
                                 map);
    // We tag the suffix as PHONE_HOME_NUMBER, then when filling the form
    // we fill only the suffix depending on the size of the input field.
    if (parsed_phone_fields_[FIELD_SUFFIX] != NULL) {
      ok = ok && AddClassification(parsed_phone_fields_[FIELD_SUFFIX],
                                   PHONE_HOME_NUMBER,
                                   map);
    }
  } else {
    ok = AddClassification(parsed_phone_fields_[FIELD_PHONE],
                           PHONE_HOME_WHOLE_NUMBER,
                           map);
  }

  return ok;
}

string16 PhoneField::GetRegExp(RegexType regex_id) const {
  switch (regex_id) {
    case REGEX_COUNTRY:
      return UTF8ToUTF16(autofill::kCountryCodeRe);
    case REGEX_AREA:
      return GetAreaRegex();
    case REGEX_AREA_NOTEXT:
      return UTF8ToUTF16(autofill::kAreaCodeNotextRe);
    case REGEX_PHONE:
      return UTF8ToUTF16(autofill::kPhoneRe);
    case REGEX_PREFIX_SEPARATOR:
      return UTF8ToUTF16(autofill::kPhonePrefixSeparatorRe);
    case REGEX_PREFIX:
      return UTF8ToUTF16(autofill::kPhonePrefixRe);
    case REGEX_SUFFIX_SEPARATOR:
      return UTF8ToUTF16(autofill::kPhoneSuffixSeparatorRe);
    case REGEX_SUFFIX:
      return UTF8ToUTF16(autofill::kPhoneSuffixRe);
    case REGEX_EXTENSION:
      return UTF8ToUTF16(autofill::kPhoneExtensionRe);
    default:
      NOTREACHED();
      break;
  }
  return string16();
}

// static
bool PhoneField::ParseInternal(PhoneField *phone_field,
                               AutofillScanner* scanner) {
  DCHECK(phone_field);
  scanner->SaveCursor();

  // The form owns the following variables, so they should not be deleted.
  const AutofillField* parsed_fields[FIELD_MAX];

  for (size_t i = 0; i < arraysize(phone_field_grammars_); ++i) {
    memset(parsed_fields, 0, sizeof(parsed_fields));
    scanner->SaveCursor();

    // Attempt to parse according to the next grammar.
    for (; i < arraysize(phone_field_grammars_) &&
         phone_field_grammars_[i].regex != REGEX_SEPARATOR; ++i) {
      if (!ParseFieldSpecifics(
              scanner,
              phone_field->GetRegExp(phone_field_grammars_[i].regex),
              MATCH_DEFAULT | MATCH_TELEPHONE,
              &parsed_fields[phone_field_grammars_[i].phone_part]))
        break;
      if (phone_field_grammars_[i].max_size &&
          (!parsed_fields[phone_field_grammars_[i].phone_part]->max_length ||
            phone_field_grammars_[i].max_size <
            parsed_fields[phone_field_grammars_[i].phone_part]->max_length)) {
        break;
      }
    }

    if (i >= arraysize(phone_field_grammars_)) {
      scanner->Rewind();
      return false;  // Parsing failed.
    }
    if (phone_field_grammars_[i].regex == REGEX_SEPARATOR)
      break;  // Parsing succeeded.

    // Proceed to the next grammar.
    do {
      ++i;
    } while (i < arraysize(phone_field_grammars_) &&
             phone_field_grammars_[i].regex != REGEX_SEPARATOR);

    if (i + 1 == arraysize(phone_field_grammars_)) {
      scanner->Rewind();
      return false;  // Tried through all the possibilities - did not match.
    }

    scanner->Rewind();
  }

  if (!parsed_fields[FIELD_PHONE]) {
    scanner->Rewind();
    return false;
  }

  for (int i = 0; i < FIELD_MAX; ++i)
    phone_field->parsed_phone_fields_[i] = parsed_fields[i];

  // Look for optional fields.

  // Look for a third text box.
  if (!phone_field->parsed_phone_fields_[FIELD_SUFFIX]) {
    if (!ParseField(scanner, UTF8ToUTF16(autofill::kPhoneSuffixRe),
                    &phone_field->parsed_phone_fields_[FIELD_SUFFIX])) {
      ParseField(scanner, UTF8ToUTF16(autofill::kPhoneSuffixSeparatorRe),
                 &phone_field->parsed_phone_fields_[FIELD_SUFFIX]);
    }
  }

  // Now look for an extension.
  ParseField(scanner, UTF8ToUTF16(autofill::kPhoneExtensionRe),
             &phone_field->parsed_phone_fields_[FIELD_EXTENSION]);

  return true;
}
