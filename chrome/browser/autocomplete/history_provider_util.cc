// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_provider_util.h"

namespace history {

const int kLowQualityMatchTypedLimit = 1;
const int kLowQualityMatchVisitLimit = 3;
const int kLowQualityMatchAgeLimitInDays = 3;

HistoryMatch::HistoryMatch()
    : url_info(),
      input_location(std::wstring::npos),
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

base::Time AutocompleteAgeThreshold() {
  return (base::Time::Now() -
          base::TimeDelta::FromDays(kLowQualityMatchAgeLimitInDays));
}

}
