// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_extent.h"

#include "chrome/common/extensions/url_pattern.h"
#include "googleurl/src/gurl.h"

ExtensionExtent::ExtensionExtent() {
}

ExtensionExtent::ExtensionExtent(const ExtensionExtent& rhs)
    : patterns_(rhs.patterns_) {
}

ExtensionExtent::~ExtensionExtent() {
}

ExtensionExtent& ExtensionExtent::operator=(const ExtensionExtent& rhs) {
  patterns_ = rhs.patterns_;
  return *this;
}

bool ExtensionExtent::is_empty() const {
  return patterns_.empty();
}

void ExtensionExtent::AddPattern(const URLPattern& pattern) {
  patterns_.push_back(pattern);
}

void ExtensionExtent::ClearPaths() {
  patterns_.clear();
}

bool ExtensionExtent::ContainsURL(const GURL& url) const {
  for (PatternList::const_iterator pattern = patterns_.begin();
       pattern != patterns_.end(); ++pattern) {
    if (pattern->MatchesUrl(url))
      return true;
  }

  return false;
}

bool ExtensionExtent::OverlapsWith(const ExtensionExtent& other) const {
  // Two extension extents overlap if there is any one URL that would match at
  // least one pattern in each of the extents.
  for (PatternList::const_iterator i = patterns_.begin();
       i != patterns_.end(); ++i) {
    for (PatternList::const_iterator j = other.patterns().begin();
         j != other.patterns().end(); ++j) {
      if (i->OverlapsWith(*j))
        return true;
    }
  }

  return false;
}
