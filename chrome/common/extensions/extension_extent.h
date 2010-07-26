// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_EXTENT_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_EXTENT_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/common/extensions/url_pattern.h"
#include "googleurl/src/gurl.h"

// Represents the set of URLs an extension uses for web content.
class ExtensionExtent {
 public:
  typedef std::vector<URLPattern> PatternList;

  bool is_empty() const { return patterns_.empty(); }

  const PatternList& patterns() const { return patterns_; }
  void AddPattern(const URLPattern& pattern) { patterns_.push_back(pattern); }
  void ClearPaths() { patterns_.clear(); }

  // Test if the extent contains a URL.
  bool ContainsURL(const GURL& url) const;

  // Returns true if there is a single URL that would be in two extents.
  bool OverlapsWith(const ExtensionExtent& other) const;

 private:
  // The list of URL patterns that comprise the extent.
  PatternList patterns_;
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_EXTENT_H_
