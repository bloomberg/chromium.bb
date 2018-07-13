// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/settings/matching_options.h"

namespace chrome_cleaner {

bool MatchingOptions::only_one_footprint() const {
  return only_one_footprint_;
}

void MatchingOptions::set_only_one_footprint(bool only_one_footprint) {
  only_one_footprint_ = only_one_footprint;
}

bool MatchingOptions::find_incomplete_matches() const {
  return find_incomplete_matches_;
}

void MatchingOptions::set_find_incomplete_matches(
    bool find_incomplete_matches) {
  find_incomplete_matches_ = find_incomplete_matches;
}

}  // namespace chrome_cleaner
