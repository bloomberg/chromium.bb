// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_provider_util.h"

namespace history {

HistoryMatch::HistoryMatch()
    : url_info(),
      input_location(string16::npos),
      match_in_scheme(false),
      innermost_match(true) {
}

HistoryMatch::HistoryMatch(const URLRow& url_info,
                           size_t input_location,
                           bool match_in_scheme,
                           bool innermost_match)
    : url_info(url_info),
      input_location(input_location),
      match_in_scheme(match_in_scheme),
      innermost_match(innermost_match) {
}

bool HistoryMatch::operator==(const GURL& url) const {
  return url_info.url() == url;
}

}  // namespace history
