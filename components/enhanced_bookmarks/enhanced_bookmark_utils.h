// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_UTILS_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_UTILS_H_

namespace enhanced_bookmarks {

// Possible locations where a bookmark can be opened from.
// Please sync with the corresponding histograms.xml.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.enhancedbookmarks
enum LaunchLocation {
  ALL_ITEMS = 0,
  UNCATEGORIZED = 1,  // Deprecated.
  FOLDER = 2,
  FILTER = 3,
  SEARCH = 4,
  BOOKMARK_EDITOR = 5,
  COUNT = 6,
};

}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_UTILS_H_

