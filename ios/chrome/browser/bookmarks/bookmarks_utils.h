// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARKS_UTILS_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARKS_UTILS_H_

// Possible locations where a bookmark can be opened from.
enum BookmarkLaunchLocation {
  BOOKMARK_LAUNCH_LOCATION_ALL_ITEMS,
  BOOKMARK_LAUNCH_LOCATION_UNCATEGORIZED_DEPRECATED,
  BOOKMARK_LAUNCH_LOCATION_FOLDER,
  BOOKMARK_LAUNCH_LOCATION_FILTER_DEPRECATED,
  BOOKMARK_LAUNCH_LOCATION_SEARCH_DEPRECATED,
  BOOKMARK_LAUNCH_LOCATION_BOOKMARK_EDITOR_DEPRECATED,
  BOOKMARK_LAUNCH_LOCATION_OMNIBOX,
  BOOKMARK_LAUNCH_LOCATION_COUNT,
};

// Records the proper metric based on the launch location.
void RecordBookmarkLaunch(BookmarkLaunchLocation launchLocation);

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARKS_UTILS_H_
