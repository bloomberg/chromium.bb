// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_field.h"

#include "base/logging.h"
#include "base/sha1.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

namespace {

static std::string Hash32Bit(const std::string& str) {
  std::string hash_bin = base::SHA1HashString(str);
  DCHECK_EQ(20U, hash_bin.length());

  uint32 hash32 = ((hash_bin[0] & 0xFF) << 24) |
                  ((hash_bin[1] & 0xFF) << 16) |
                  ((hash_bin[2] & 0xFF) << 8) |
                   (hash_bin[3] & 0xFF);

  return UintToString(hash32);
}

}  // namespace

AutoFillField::AutoFillField()
    : server_type_(NO_SERVER_DATA),
      heuristic_type_(UNKNOWN_TYPE) {
}

AutoFillField::AutoFillField(const webkit_glue::FormField& field,
                             const string16& unique_name)
    : webkit_glue::FormField(field),
      unique_name_(unique_name),
      server_type_(NO_SERVER_DATA),
      heuristic_type_(UNKNOWN_TYPE) {
}

AutoFillFieldType AutoFillField::type() const {
  if (server_type_ != NO_SERVER_DATA)
    return server_type_;

  return heuristic_type_;
}

bool AutoFillField::IsEmpty() const {
  return value().empty();
}

std::string AutoFillField::FieldSignature() const {
  std::string field_name = UTF16ToUTF8(name());
  std::string type = UTF16ToUTF8(html_input_type());
  std::string field_string = field_name + "&" + type;
  return Hash32Bit(field_string);
}

void AutoFillField::set_heuristic_type(const AutoFillFieldType& type) {
  DCHECK(type >= 0 && type < MAX_VALID_FIELD_TYPE);
  if (type >= 0 && type < MAX_VALID_FIELD_TYPE)
    heuristic_type_ = type;
  else
    heuristic_type_ = UNKNOWN_TYPE;
}

bool AutoFillField::IsFieldFillable() const {
  return type() != UNKNOWN_TYPE;
}
