// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_field.h"

#include "base/logging.h"
#include "base/sha1.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"

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

AutofillField::AutofillField()
    : server_type_(NO_SERVER_DATA),
      heuristic_type_(UNKNOWN_TYPE),
      phone_part_(IGNORED) {
}

AutofillField::AutofillField(const FormFieldData& field,
                             const string16& unique_name)
    : FormFieldData(field),
      unique_name_(unique_name),
      server_type_(NO_SERVER_DATA),
      heuristic_type_(UNKNOWN_TYPE),
      phone_part_(IGNORED) {
}

AutofillField::~AutofillField() {}

void AutofillField::set_heuristic_type(AutofillFieldType type) {
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

void AutofillField::set_server_type(AutofillFieldType type) {
  // Chrome no longer supports fax numbers, but the server still does.
  if (type >= PHONE_FAX_NUMBER && type <= PHONE_FAX_WHOLE_NUMBER)
    return;

  server_type_ = type;
}

AutofillFieldType AutofillField::type() const {
  if (server_type_ != NO_SERVER_DATA)
    return server_type_;

  return heuristic_type_;
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
  return type() != UNKNOWN_TYPE;
}
