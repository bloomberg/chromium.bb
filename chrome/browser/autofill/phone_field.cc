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
#include "chrome/browser/autofill/fax_number.h"
#include "chrome/browser/autofill/home_phone_number.h"
#include "ui/base/l10n/l10n_util.h"

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

  // Go through the phones in order HOME, FAX, attempting to match. HOME should
  // be the last as it is a catch all case ("fax" and "faxarea" parsed as FAX,
  // but "area" and "someotherarea" parsed as HOME, for example).
  for (int i = PHONE_TYPE_MAX - 1; i >= PHONE_TYPE_FIRST; --i) {
    phone_field->SetPhoneType(static_cast<PhoneField::PhoneType>(i));
    if (ParseInternal(phone_field.get(), scanner, i == HOME_PHONE))
      return phone_field.release();
  }

  return NULL;
}

PhoneField::PhoneField() {
  memset(parsed_phone_fields_, 0, sizeof(parsed_phone_fields_));
  SetPhoneType(HOME_PHONE);
}

bool PhoneField::ClassifyField(FieldTypeMap* map) const {
  bool ok = true;

  DCHECK(parsed_phone_fields_[FIELD_PHONE]);  // Phone was correctly parsed.

  if ((parsed_phone_fields_[FIELD_COUNTRY_CODE] != NULL) ||
      (parsed_phone_fields_[FIELD_AREA_CODE] != NULL) ||
      (parsed_phone_fields_[FIELD_SUFFIX] != NULL)) {
    if (parsed_phone_fields_[FIELD_COUNTRY_CODE] != NULL) {
      ok = ok && AddClassification(parsed_phone_fields_[FIELD_COUNTRY_CODE],
                                   number_->GetCountryCodeType(),
                                   map);
    }

    AutofillFieldType field_number_type = number_->GetNumberType();
    if (parsed_phone_fields_[FIELD_AREA_CODE] != NULL) {
      ok = ok && AddClassification(parsed_phone_fields_[FIELD_AREA_CODE],
                                   number_->GetCityCodeType(),
                                   map);
    } else if (parsed_phone_fields_[FIELD_COUNTRY_CODE] != NULL) {
      // Only if we can find country code without city code, it means the phone
      // number include city code.
      field_number_type = number_->GetCityAndNumberType();
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
                                   number_->GetNumberType(),
                                   map);
    }
  } else {
    ok = AddClassification(parsed_phone_fields_[FIELD_PHONE],
                           number_->GetWholeNumberType(),
                           map);
  }

  return ok;
}

string16  PhoneField::GetCountryRegex() const {
  // This one is the same for Home and Fax numbers.
  return UTF8ToUTF16(autofill::kCountryCodeRe);
}

string16 PhoneField::GetAreaRegex() const {
  // This one is the same for Home and Fax numbers.
  string16 area_code = UTF8ToUTF16(autofill::kAreaCodeRe);
  area_code.append(ASCIIToUTF16("|"));  // Regexp separator.
  area_code.append(GetAreaNoTextRegex());
  return area_code;
}

string16 PhoneField::GetAreaNoTextRegex() const {
  // This one is the same for Home and Fax numbers.
  return UTF8ToUTF16(autofill::kAreaCodeNotextRe);
}

string16 PhoneField::GetPhoneRegex() const {
  if (phone_type_ == HOME_PHONE)
    return UTF8ToUTF16(autofill::kPhoneRe);
  else if (phone_type_ == FAX_PHONE)
    return UTF8ToUTF16(autofill::kFaxRe);
  else
    NOTREACHED();
  return string16();
}

string16 PhoneField::GetPrefixSeparatorRegex() const {
  // This one is the same for Home and Fax numbers.
  return UTF8ToUTF16(autofill::kPhonePrefixSeparatorRe);
}

string16 PhoneField::GetPrefixRegex() const {
  // This one is the same for Home and Fax numbers.
  return UTF8ToUTF16(autofill::kPhonePrefixRe);
}

string16 PhoneField::GetSuffixSeparatorRegex() const {
  // This one is the same for Home and Fax numbers.
  return UTF8ToUTF16(autofill::kPhoneSuffixSeparatorRe);
}

string16 PhoneField::GetSuffixRegex() const {
  // This one is the same for Home and Fax numbers.
  return UTF8ToUTF16(autofill::kPhoneSuffixRe);
}

string16 PhoneField::GetExtensionRegex() const {
  // This one is the same for Home and Fax numbers.
  return UTF8ToUTF16(autofill::kPhoneExtensionRe);
}

string16 PhoneField::GetRegExp(RegexType regex_id) const {
  switch (regex_id) {
    case REGEX_COUNTRY: return GetCountryRegex();
    case REGEX_AREA: return GetAreaRegex();
    case REGEX_AREA_NOTEXT: return GetAreaNoTextRegex();
    case REGEX_PHONE: return GetPhoneRegex();
    case REGEX_PREFIX_SEPARATOR: return GetPrefixSeparatorRegex();
    case REGEX_PREFIX: return GetPrefixRegex();
    case REGEX_SUFFIX_SEPARATOR: return GetSuffixSeparatorRegex();
    case REGEX_SUFFIX: return GetSuffixRegex();
    case REGEX_EXTENSION: return GetExtensionRegex();
    default:
      NOTREACHED();
      break;
  }
  return string16();
}

// static
bool PhoneField::ParseInternal(PhoneField *phone_field,
                               AutofillScanner* scanner,
                               bool regular_phone) {
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
    if (!ParseField(scanner, phone_field->GetSuffixRegex(),
                    &phone_field->parsed_phone_fields_[FIELD_SUFFIX])) {
      ParseField(scanner, phone_field->GetSuffixSeparatorRegex(),
                 &phone_field->parsed_phone_fields_[FIELD_SUFFIX]);
    }
  }

  // Now look for an extension.
  ParseField(scanner, phone_field->GetExtensionRegex(),
             &phone_field->parsed_phone_fields_[FIELD_EXTENSION]);

  return true;
}

void PhoneField::SetPhoneType(PhoneType phone_type) {
  // Field types are different as well, so we create a temporary phone number,
  // to get relevant field types.
  if (phone_type == HOME_PHONE)
    number_.reset(new PhoneNumber(AutofillType::PHONE_HOME, NULL));
  else
    number_.reset(new PhoneNumber(AutofillType::PHONE_FAX, NULL));
  phone_type_ = phone_type;
}

