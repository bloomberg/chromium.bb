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

PhoneField::~PhoneField() {}

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
const PhoneField::Parser PhoneField::kPhoneFieldGrammars[] = {
  // Country code: <cc> Area Code: <ac> Phone: <phone> (- <suffix>
  // (Ext: <ext>)?)?
  { REGEX_COUNTRY, FIELD_COUNTRY_CODE, 0 },
  { REGEX_AREA, FIELD_AREA_CODE, 0 },
  { REGEX_PHONE, FIELD_PHONE, 0 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // \( <ac> \) <phone>:3 <suffix>:4 (Ext: <ext>)?
  { REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 3 },
  { REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3 },
  { REGEX_PHONE, FIELD_SUFFIX, 4 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <cc> <ac>:3 - <phone>:3 - <suffix>:4 (Ext: <ext>)?
  { REGEX_PHONE, FIELD_COUNTRY_CODE, 0 },
  { REGEX_PHONE, FIELD_AREA_CODE, 3 },
  { REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3 },
  { REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 4 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <cc>:3 <ac>:3 <phone>:3 <suffix>:4 (Ext: <ext>)?
  { REGEX_PHONE, FIELD_COUNTRY_CODE, 3 },
  { REGEX_PHONE, FIELD_AREA_CODE, 3 },
  { REGEX_PHONE, FIELD_PHONE, 3 },
  { REGEX_PHONE, FIELD_SUFFIX, 4 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Area Code: <ac> Phone: <phone> (- <suffix> (Ext: <ext>)?)?
  { REGEX_AREA, FIELD_AREA_CODE, 0 },
  { REGEX_PHONE, FIELD_PHONE, 0 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <ac> <phone>:3 <suffix>:4 (Ext: <ext>)?
  { REGEX_PHONE, FIELD_AREA_CODE, 0 },
  { REGEX_PHONE, FIELD_PHONE, 3 },
  { REGEX_PHONE, FIELD_SUFFIX, 4 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <cc> \( <ac> \) <phone> (- <suffix> (Ext: <ext>)?)?
  { REGEX_PHONE, FIELD_COUNTRY_CODE, 0 },
  { REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 0 },
  { REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 0 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: \( <ac> \) <phone> (- <suffix> (Ext: <ext>)?)?
  { REGEX_PHONE, FIELD_COUNTRY_CODE, 0 },
  { REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 0 },
  { REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 0 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <cc> - <ac> - <phone> - <suffix> (Ext: <ext>)?
  { REGEX_PHONE, FIELD_COUNTRY_CODE, 0 },
  { REGEX_PREFIX_SEPARATOR, FIELD_AREA_CODE, 0 },
  { REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 0 },
  { REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 0 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <ac> Prefix: <phone> Suffix: <suffix> (Ext: <ext>)?
  { REGEX_PHONE, FIELD_AREA_CODE, 0 },
  { REGEX_PREFIX, FIELD_PHONE, 0 },
  { REGEX_SUFFIX, FIELD_SUFFIX, 0 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <ac> - <phone>:3 - <suffix>:4 (Ext: <ext>)?
  { REGEX_PHONE, FIELD_AREA_CODE, 0 },
  { REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3 },
  { REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 4 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <cc> - <ac> - <phone> (Ext: <ext>)?
  { REGEX_PHONE, FIELD_COUNTRY_CODE, 0 },
  { REGEX_PREFIX_SEPARATOR, FIELD_AREA_CODE, 0 },
  { REGEX_SUFFIX_SEPARATOR, FIELD_PHONE, 0 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <ac> - <phone> (Ext: <ext>)?
  { REGEX_AREA, FIELD_AREA_CODE, 0 },
  { REGEX_PHONE, FIELD_PHONE, 0 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <cc>:3 - <phone>:10 (Ext: <ext>)?
  { REGEX_PHONE, FIELD_COUNTRY_CODE, 3 },
  { REGEX_PHONE, FIELD_PHONE, 10 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
  // Phone: <phone> (Ext: <ext>)?
  { REGEX_PHONE, FIELD_PHONE, 0 },
  { REGEX_SEPARATOR, FIELD_NONE, 0 },
};

// static
FormField* PhoneField::Parse(AutofillScanner* scanner,
                             bool parse_new_field_types) {
  if (scanner->IsEnd())
    return NULL;

  scanner->SaveCursor();

  // The form owns the following variables, so they should not be deleted.
  const AutofillField* parsed_fields[FIELD_MAX];

  for (size_t i = 0; i < arraysize(kPhoneFieldGrammars); ++i) {
    memset(parsed_fields, 0, sizeof(parsed_fields));
    scanner->SaveCursor();

    // Attempt to parse according to the next grammar.
    for (; i < arraysize(kPhoneFieldGrammars) &&
         kPhoneFieldGrammars[i].regex != REGEX_SEPARATOR; ++i) {
      if (!ParseFieldSpecifics(
              scanner,
              GetRegExp(kPhoneFieldGrammars[i].regex),
              MATCH_DEFAULT | MATCH_TELEPHONE,
              &parsed_fields[kPhoneFieldGrammars[i].phone_part]))
        break;
      if (kPhoneFieldGrammars[i].max_size &&
          (!parsed_fields[kPhoneFieldGrammars[i].phone_part]->max_length ||
            kPhoneFieldGrammars[i].max_size <
            parsed_fields[kPhoneFieldGrammars[i].phone_part]->max_length)) {
        break;
      }
    }

    if (i >= arraysize(kPhoneFieldGrammars)) {
      scanner->Rewind();
      return NULL;  // Parsing failed.
    }
    if (kPhoneFieldGrammars[i].regex == REGEX_SEPARATOR)
      break;  // Parsing succeeded.

    // Proceed to the next grammar.
    do {
      ++i;
    } while (i < arraysize(kPhoneFieldGrammars) &&
             kPhoneFieldGrammars[i].regex != REGEX_SEPARATOR);

    if (i + 1 == arraysize(kPhoneFieldGrammars)) {
      scanner->Rewind();
      return NULL;  // Tried through all the possibilities - did not match.
    }

    scanner->Rewind();
  }

  if (!parsed_fields[FIELD_PHONE]) {
    scanner->Rewind();
    return NULL;
  }

  scoped_ptr<PhoneField> phone_field(new PhoneField);
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

  return phone_field.release();
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

PhoneField::PhoneField() {
  memset(parsed_phone_fields_, 0, sizeof(parsed_phone_fields_));
}

// static
string16 PhoneField::GetRegExp(RegexType regex_id) {
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
