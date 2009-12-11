// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/phone_field.h"

#include "base/logging.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_field.h"

// static
PhoneField* PhoneField::Parse(std::vector<AutoFillField*>::const_iterator* iter,
                              bool is_ecml) {
  if (is_ecml)
    return ParseECML(iter);

  std::vector<AutoFillField*>::const_iterator q = *iter;
  AutoFillField* phone = NULL;
  AutoFillField* phone2 = NULL;
  AutoFillField* phone3 = NULL;
  bool area_code;  // true if we've parsed an area code field.

  // Some pages, such as BloomingdalesShipping.html, have a field labeled
  // "Area Code and Phone"; we want to parse this as a phone number field so
  // we look for "phone" before we look for "area code".
  if (ParseText(&q, ASCIIToUTF16("phone"), &phone)) {
    area_code = false;
  } else {
    if (!ParseText(&q, ASCIIToUTF16("area code"), &phone))
      return NULL;
    area_code = true;
    ParseText(&q, ASCIIToUTF16("phone"), &phone2);
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
    ParseText(&q, ASCIIToUTF16("^-|)|"), &phone2);

  // Look for a third text box.
  if (phone2)
    ParseText(&q, ASCIIToUTF16("^-|"), &phone3);

  // Now we have one, two, or three phone number text fields.  Package them
  // up into a PhoneField object.

  PhoneField phone_field;
  if (phone2 == NULL) {  // only one field
    if (area_code)  // it's an area code
      return NULL;  // doesn't make sense
    phone_field.phone_ = phone;
  } else {
    phone_field.area_code_ = phone;
    if (phone3 == NULL) {  // two fields
      phone_field.phone_ = phone2;
    } else {  // three boxes: area code, prefix and suffix
      phone_field.prefix_ = phone2;
      phone_field.phone_ = phone3;
    }
  }

  // Now look for an extension.
  ParseText(&q, ASCIIToUTF16("ext"), &phone_field.extension_);

  *iter = q;
  return new PhoneField(phone_field);
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
    ok = Add(field_type_map, area_code_, AutoFillType(PHONE_HOME_CITY_CODE));
    DCHECK(ok);

    // NOTE: we ignore the prefix/suffix thing here.
    ok = ok && Add(field_type_map, phone_, AutoFillType(PHONE_HOME_NUMBER));
    DCHECK(ok);
  } else {
    ok = Add(field_type_map, phone_, AutoFillType(PHONE_HOME_WHOLE_NUMBER));
    DCHECK(ok);
  }

  return ok;
}

PhoneField::PhoneField()
    : phone_(NULL),
      area_code_(NULL),
      prefix_(NULL),
      extension_(NULL) {
}
