// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/location_bar_util.h"

#include "base/i18n/rtl.h"
#include "base/string_util.h"
#include "ui/base/text/text_elider.h"

namespace location_bar_util {

string16 CalculateMinString(const string16& description) {
  // Chop at the first '.' or whitespace.
  const size_t dot_index = description.find('.');
  const size_t ws_index = description.find_first_of(kWhitespaceUTF16);
  size_t chop_index = std::min(dot_index, ws_index);
  string16 min_string;
  if (chop_index == string16::npos) {
    // No dot or whitespace, truncate to at most 3 chars.
    min_string = ui::TruncateString(description, 3);
  } else {
    min_string = description.substr(0, chop_index);
  }
  base::i18n::AdjustStringForLocaleDirection(&min_string);
  return min_string;
}

}  // namespace location_bar_util
