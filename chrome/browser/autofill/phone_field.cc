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
#include "grit/autofill_resources.h"
#include "ui/base/l10n/l10n_util.h"

// static
PhoneField* PhoneField::Parse(std::vector<AutoFillField*>::const_iterator* iter,
                              bool is_ecml) {
  DCHECK(iter);
  if (!iter)
    return NULL;

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
  if (ParseText(&q, l10n_util::GetStringUTF16(IDS_AUTOFILL_PHONE_RE), &phone)) {
    area_code = false;
  } else {
    if (!ParseText(&q,
                   l10n_util::GetStringUTF16(IDS_AUTOFILL_AREA_CODE_RE),
                   &phone))
      return NULL;
    area_code = true;
    ParseText(&q, l10n_util::GetStringUTF16(IDS_AUTOFILL_PHONE_RE), &phone2);
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
    ParseText(&q,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_PHONE_PREFIX_RE),
              &phone2);

  // Look for a third text box.
  if (phone2)
    ParseText(&q,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_PHONE_SUFFIX_RE),
              &phone3);

  // Now we have one, two, or three phone number text fields.  Package them
  // up into a PhoneField object.

  scoped_ptr<PhoneField> phone_field(new PhoneField);
  if (phone2 == NULL) {  // only one field
    if (area_code)  // it's an area code
      return NULL;  // doesn't make sense
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
  ParseText(&q,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_PHONE_EXTENSION_RE),
            &phone_field->extension_);

  *iter = q;
  return phone_field.release();
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

    if (prefix_ != NULL) {
      // We tag the prefix as PHONE_HOME_NUMBER, then when filling the form
      // we fill only the prefix depending on the size of the input field.
      ok = ok && Add(field_type_map,
                     prefix_,
                     AutoFillType(PHONE_HOME_NUMBER));
      DCHECK(ok);
      // We tag the suffix as PHONE_HOME_NUMBER, then when filling the form
      // we fill only the suffix depending on the size of the input field.
      ok = ok && Add(field_type_map,
                     phone_,
                     AutoFillType(PHONE_HOME_NUMBER));
      DCHECK(ok);
    } else {
      ok = ok && Add(field_type_map, phone_, AutoFillType(PHONE_HOME_NUMBER));
      DCHECK(ok);
    }
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
