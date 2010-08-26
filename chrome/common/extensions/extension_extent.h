// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_EXTENT_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_EXTENT_H_
#pragma once

#include <vector>

class GURL;
class URLPattern;

// Represents the set of URLs an extension uses for web content.
class ExtensionExtent {
 public:
  typedef std::vector<URLPattern> PatternList;

  ExtensionExtent();
  ExtensionExtent(const ExtensionExtent& rhs);
  ~ExtensionExtent();
  ExtensionExtent& operator=(const ExtensionExtent& rhs);

  bool is_empty() const;

  const PatternList& patterns() const { return patterns_; }
  void AddPattern(const URLPattern& pattern);
  void ClearPaths();

  // Test if the extent contains a URL.
  bool ContainsURL(const GURL& url) const;

  // Returns true if there is a single URL that would be in two extents.
  bool OverlapsWith(const ExtensionExtent& other) const;

 private:
  // The list of URL patterns that comprise the extent.
  PatternList patterns_;
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_EXTENT_H_
