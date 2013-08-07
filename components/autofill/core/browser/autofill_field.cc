// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field.h"

#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"

namespace {

static std::string Hash32Bit(const std::string& str) {
  std::string hash_bin = base::SHA1HashString(str);
  DCHECK_EQ(20U, hash_bin.length());

  uint32 hash32 = ((hash_bin[0] & 0xFF) << 24) |
                  ((hash_bin[1] & 0xFF) << 16) |
                  ((hash_bin[2] & 0xFF) << 8) |
                   (hash_bin[3] & 0xFF);

  return base::UintToString(hash32);
}

}  // namespace

namespace autofill {

AutofillField::AutofillField()
    : server_type_(NO_SERVER_DATA),
      heuristic_type_(UNKNOWN_TYPE),
      html_type_(HTML_TYPE_UNKNOWN),
      html_mode_(HTML_MODE_NONE),
      phone_part_(IGNORED) {
}

AutofillField::AutofillField(const FormFieldData& field,
                             const base::string16& unique_name)
    : FormFieldData(field),
      unique_name_(unique_name),
      server_type_(NO_SERVER_DATA),
      heuristic_type_(UNKNOWN_TYPE),
      html_type_(HTML_TYPE_UNKNOWN),
      html_mode_(HTML_MODE_NONE),
      phone_part_(IGNORED) {
}

AutofillField::~AutofillField() {}

void AutofillField::set_heuristic_type(ServerFieldType type) {
  if (type >= 0 && type < MAX_VALID_FIELD_TYPE &&
      type != FIELD_WITH_DEFAULT_VALUE) {
    heuristic_type_ = type;
  } else {
    NOTREACHED();
    // This case should not be reachable; but since this has potential
    // implications on data uploaded to the server, better safe than sorry.
    heuristic_type_ = UNKNOWN_TYPE;
  }
}

void AutofillField::set_server_type(ServerFieldType type) {
  // Chrome no longer supports fax numbers, but the server still does.
  if (type >= PHONE_FAX_NUMBER && type <= PHONE_FAX_WHOLE_NUMBER)
    return;

  server_type_ = type;
}

void AutofillField::SetHtmlType(HtmlFieldType type, HtmlFieldMode mode) {
  html_type_ = type;
  html_mode_ = mode;

  if (type == HTML_TYPE_TEL_LOCAL_PREFIX)
    phone_part_ = AutofillField::PHONE_PREFIX;
  else if (type == HTML_TYPE_TEL_LOCAL_SUFFIX)
    phone_part_ = AutofillField::PHONE_SUFFIX;
}

AutofillType AutofillField::Type() const {
  if (html_type_ != HTML_TYPE_UNKNOWN)
    return AutofillType(html_type_, html_mode_);

  if (server_type_ != NO_SERVER_DATA)
    return AutofillType(server_type_);

  return AutofillType(heuristic_type_);
}

bool AutofillField::IsEmpty() const {
  return value.empty();
}

std::string AutofillField::FieldSignature() const {
  std::string field_name = UTF16ToUTF8(name);
  std::string field_string = field_name + "&" + form_control_type;
  return Hash32Bit(field_string);
}

bool AutofillField::IsFieldFillable() const {
  return !Type().IsUnknown();
}

}  // namespace autofill
