// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/url_pattern_set.h"

#include "chrome/common/extensions/url_pattern.h"
#include "googleurl/src/gurl.h"

URLPatternSet::URLPatternSet() {
}

URLPatternSet::URLPatternSet(const URLPatternSet& rhs)
    : patterns_(rhs.patterns_) {
}

URLPatternSet::~URLPatternSet() {
}

URLPatternSet& URLPatternSet::operator=(const URLPatternSet& rhs) {
  patterns_ = rhs.patterns_;
  return *this;
}

bool URLPatternSet::is_empty() const {
  return patterns_.empty();
}

void URLPatternSet::AddPattern(const URLPattern& pattern) {
  patterns_.push_back(pattern);
}

void URLPatternSet::ClearPatterns() {
  patterns_.clear();
}

bool URLPatternSet::MatchesURL(const GURL& url) const {
  for (URLPatternList::const_iterator pattern = patterns_.begin();
       pattern != patterns_.end(); ++pattern) {
    if (pattern->MatchesURL(url))
      return true;
  }

  return false;
}

bool URLPatternSet::OverlapsWith(const URLPatternSet& other) const {
  // Two extension extents overlap if there is any one URL that would match at
  // least one pattern in each of the extents.
  for (URLPatternList::const_iterator i = patterns_.begin();
       i != patterns_.end(); ++i) {
    for (URLPatternList::const_iterator j = other.patterns().begin();
         j != other.patterns().end(); ++j) {
      if (i->OverlapsWith(*j))
        return true;
    }
  }

  return false;
}
