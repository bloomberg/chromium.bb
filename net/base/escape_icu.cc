// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "net/base/escape.h"

#include "base/i18n/icu_string_conversions.h"
#include "base/utf_string_conversions.h"

// This file exists to avoid having escape.cc depend on ICU.

bool EscapeQueryParamValue(const string16& text, const char* codepage,
                           bool use_plus, string16* escaped) {
  // TODO(brettw) bug 1201094: this function should be removed, this "SKIP"
  // behavior is wrong when the character can't be encoded properly.
  std::string encoded;
  if (!base::UTF16ToCodepage(text, codepage,
                             base::OnStringConversionError::SKIP, &encoded))
    return false;

  escaped->assign(UTF8ToUTF16(EscapeQueryParamValue(encoded, use_plus)));
  return true;
}
