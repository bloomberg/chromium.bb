// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/phone_field.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/fax_number.h"
#include "chrome/browser/autofill/home_phone_number.h"
#include "grit/autofill_resources.h"
#include "ui/base/l10n/l10n_util.h"

PhoneField::~PhoneField() {}

// static
PhoneField* PhoneField::Parse(std::vector<AutoFillField*>::const_iterator* iter,
                              bool is_ecml) {
  DCHECK(iter);
  if (!iter)
    return NULL;

  if (is_ecml)
    return ParseECML(iter);

  scoped_ptr<PhoneField> phone_field(new PhoneField);

  // Go through the phones in order HOME, FAX, attempting to match. HOME should
  // be the last as it is a catch all case ("fax" and "faxarea" parsed as FAX,
  // but "area" and "someotherarea" parsed as HOME, for example).
  for (int i = PHONE_TYPE_MAX - 1; i >= PHONE_TYPE_FIRST; --i) {
    phone_field->SetPhoneType(static_cast<PhoneField::PHONE_TYPE>(i));
    if (ParseInternal(phone_field.get(), iter, i == HOME_PHONE))
      return phone_field.release();
  }

  return NULL;
}

// static
PhoneField* PhoneField::ParseECML(
    std::vector<AutoFillField*>::const_iterator* iter) {
  string16 pattern(GetEcmlPattern(kEcmlShipToPhone, kEcmlBillToPhone, '|'));

  AutoFillField* field;
  if (ParseText(iter, pattern, &field)) {
    PhoneField* phone_field = new PhoneField();
    phone_field->phone_ = field;
    return phone_field;
  }

  return NULL;
}

bool PhoneField::GetFieldInfo(FieldTypeMap* field_type_map) const {
  bool ok;

  if (area_code_ != NULL) {
    ok = Add(field_type_map, area_code_,
             AutoFillType(number_->GetCityCodeType()));
    DCHECK(ok);

    if (prefix_ != NULL) {
      // We tag the prefix as PHONE_HOME_NUMBER, then when filling the form
      // we fill only the prefix depending on the size of the input field.
      ok = ok && Add(field_type_map,
                     prefix_,
                     AutoFillType(number_->GetNumberType()));
      DCHECK(ok);
      // We tag the suffix as PHONE_HOME_NUMBER, then when filling the form
      // we fill only the suffix depending on the size of the input field.
      ok = ok && Add(field_type_map,
                     phone_,
                     AutoFillType(number_->GetNumberType()));
      DCHECK(ok);
    } else {
      ok = ok && Add(field_type_map,
                     phone_,
                     AutoFillType(number_->GetNumberType()));
      DCHECK(ok);
    }
  } else {
    ok = Add(field_type_map,
             phone_,
             AutoFillType(number_->GetWholeNumberType()));
    DCHECK(ok);
  }

  return ok;
}

PhoneField::PhoneField()
    : phone_(NULL),
      area_code_(NULL),
      prefix_(NULL),
      extension_(NULL) {
  SetPhoneType(HOME_PHONE);
}

string16 PhoneField::GetAreaRegex() const {
  // This one is the same for Home and Fax numbers.
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_AREA_CODE_RE);
}

string16 PhoneField::GetPhoneRegex() const {
  if (phone_type_ == HOME_PHONE)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_PHONE_RE);
  else if (phone_type_ == FAX_PHONE)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_FAX_RE);
  else
    NOTREACHED();
  return string16();
}

string16 PhoneField::GetPrefixRegex() const {
  // This one is the same for Home and Fax numbers.
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_PHONE_PREFIX_RE);
}

string16 PhoneField::GetSuffixRegex() const {
  // This one is the same for Home and Fax numbers.
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_PHONE_SUFFIX_RE);
}

string16 PhoneField::GetExtensionRegex() const {
  // This one is the same for Home and Fax numbers.
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_PHONE_EXTENSION_RE);
}

// static
bool PhoneField::ParseInternal(
    PhoneField *phone_field,
    std::vector<AutoFillField*>::const_iterator* iter,
    bool regular_phone) {
  DCHECK(iter);

  DCHECK(phone_field);
  if (!phone_field)
    return false;

  std::vector<AutoFillField*>::const_iterator q = *iter;
  // The form owns the following variables, so they should not be deleted.
  AutoFillField* phone = NULL;
  AutoFillField* phone2 = NULL;
  AutoFillField* phone3 = NULL;
  bool area_code = false;  // true if we've parsed an area code field.

  // Some pages, such as BloomingdalesShipping.html, have a field labeled
  // "Area Code and Phone"; we want to parse this as a phone number field so
  // we look for "phone" before we look for "area code".
  if (ParseText(&q, phone_field->GetPhoneRegex(), &phone)) {
    area_code = false;
    // Check the case when the match is for non-home phone and area code, e.g.
    // first field is a "Fax area code" and the subsequent is "Fax phone".
    if (!regular_phone) {
      // Attempt parsing of the same field as an area code and then phone:
      std::vector<AutoFillField*>::const_iterator temp_it = *iter;
      AutoFillField* tmp_phone1 = NULL;
      AutoFillField* tmp_phone2 = NULL;
      if (ParseText(&temp_it, phone_field->GetAreaRegex(), &tmp_phone1) &&
          ParseText(&temp_it, phone_field->GetPhoneRegex(), &tmp_phone2)) {
        phone = tmp_phone1;
        phone2 = tmp_phone2;
        q = temp_it;
        area_code = true;
      }
    }
  } else {
    if (!ParseText(&q, phone_field->GetAreaRegex(), &phone))
      return false;
    area_code = true;
    // If this is not a home phone and there was no specification before
    // the phone number actually starts (e.g. field 1 "Area code:", field 2
    // "Fax:"), we skip searching for preffix and suffix and bail out.
    if (!ParseText(&q, phone_field->GetPhoneRegex(), &phone2) && !regular_phone)
      return false;
  }

  // Sometimes phone number fields are separated by "-" (e.g. test page
  // Crate and Barrel Check Out.html).  Also, area codes are sometimes
  // surrounded by parentheses, so a ")" may appear after the area code field.
  //
  // We used to match "tel" here, which we've seen in field names (e.g. on
  // Newegg2.html), but that's too general: some pages (e.g.
  // uk/Furniture123-1.html) have several phone numbers in succession and we
  // don't want those to be parsed as components of a single phone number.
  if (phone2 == NULL)
    ParseText(&q, phone_field->GetPrefixRegex(), &phone2);

  // Look for a third text box.
  if (phone2)
    ParseText(&q, phone_field->GetSuffixRegex(), &phone3);

  // Now we have one, two, or three phone number text fields.  Package them
  // up into a PhoneField object.

  if (phone2 == NULL) {  // only one field
    if (area_code) {
      // It's an area code - it doesn't make sense.
      return false;
    }
    phone_field->phone_ = phone;
  } else {
    phone_field->area_code_ = phone;
    if (phone3 == NULL) {  // two fields
      phone_field->phone_ = phone2;
    } else {  // three boxes: area code, prefix and suffix
      phone_field->prefix_ = phone2;
      phone_field->phone_ = phone3;
    }
  }

  // Now look for an extension.
  ParseText(&q, phone_field->GetExtensionRegex(), &phone_field->extension_);

  *iter = q;
  return true;
}

void PhoneField::SetPhoneType(PHONE_TYPE phone_type) {
  // Field types are different as well, so we create a temporary phone number,
  // to get relevant field types.
  if (phone_type == HOME_PHONE)
    number_.reset(new HomePhoneNumber);
  else
    number_.reset(new FaxNumber);
  phone_type_ = phone_type;
}

