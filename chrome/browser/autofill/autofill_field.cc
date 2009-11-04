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
  DCHECK(hash_bin.length() == 20);

  uint32 hash32 = ((hash_bin[0] & 0xFF) << 24) |
                  ((hash_bin[1] & 0xFF) << 16) |
                  ((hash_bin[2] & 0xFF) << 8) |
                   (hash_bin[3] & 0xFF);

  return UintToString(hash32);
}

}  // namespace

AutoFillField::AutoFillField(const webkit_glue::FormField& field)
    : webkit_glue::FormField(field) {
}

std::string AutoFillField::FieldSignature() const {
  std::string field_name = UTF16ToUTF8(name());
  std::string type = UTF16ToUTF8(html_input_type());
  std::string field_string = field_name + "&" + type;
  return Hash32Bit(field_string);
}
