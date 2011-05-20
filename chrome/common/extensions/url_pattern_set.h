// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_URL_PATTERN_SET_H_
#define CHROME_COMMON_EXTENSIONS_URL_PATTERN_SET_H_
#pragma once

#include <vector>

#include "chrome/common/extensions/url_pattern.h"

class GURL;

// Represents the set of URLs an extension uses for web content.
class URLPatternSet {
 public:
  URLPatternSet();
  URLPatternSet(const URLPatternSet& rhs);
  ~URLPatternSet();
  URLPatternSet& operator=(const URLPatternSet& rhs);

  bool is_empty() const;

  const URLPatternList& patterns() const { return patterns_; }
  void AddPattern(const URLPattern& pattern);
  void ClearPatterns();

  // Test if the extent contains a URL.
  bool MatchesURL(const GURL& url) const;

  // Returns true if there is a single URL that would be in two extents.
  bool OverlapsWith(const URLPatternSet& other) const;

 private:
  // The list of URL patterns that comprise the extent.
  URLPatternList patterns_;
};

#endif  // CHROME_COMMON_EXTENSIONS_URL_PATTERN_SET_H_
