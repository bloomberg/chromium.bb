// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/location_bar/location_bar_util.h"

#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/text_elider.h"

namespace location_bar_util {

base::string16 CalculateMinString(const base::string16& description) {
  // Chop at the first '.' or whitespace.
  const size_t chop_index = description.find_first_of(
      base::kWhitespaceUTF16 + base::ASCIIToUTF16("."));
  base::string16 min_string((chop_index == base::string16::npos) ?
      gfx::TruncateString(description, 3, gfx::WORD_BREAK) :
      description.substr(0, chop_index));
  base::i18n::AdjustStringForLocaleDirection(&min_string);
  return min_string;
}

}  // namespace location_bar_util
