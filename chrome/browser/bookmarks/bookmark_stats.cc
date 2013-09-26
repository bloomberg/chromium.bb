// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_stats.h"

#include "base/metrics/histogram.h"
#include "content/public/browser/user_metrics.h"

void RecordBookmarkLaunch(BookmarkLaunchLocation location) {
  if (location == BOOKMARK_LAUNCH_LOCATION_DETACHED_BAR ||
      location == BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR) {
    content::RecordAction(
        content::UserMetricsAction("ClickedBookmarkBarURLButton"));
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Bookmarks.LaunchLocation", location, BOOKMARK_LAUNCH_LOCATION_LIMIT);
}

void RecordBookmarkFolderOpen(BookmarkLaunchLocation location) {
  if (location == BOOKMARK_LAUNCH_LOCATION_DETACHED_BAR ||
      location == BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR) {
    content::RecordAction(
        content::UserMetricsAction("ClickedBookmarkBarFolder"));
  }
}

void RecordBookmarkAppsPageOpen(BookmarkLaunchLocation location) {
  if (location == BOOKMARK_LAUNCH_LOCATION_DETACHED_BAR ||
      location == BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR) {
    content::RecordAction(
        content::UserMetricsAction("ClickedBookmarkBarAppsShortcutButton"));
  }
}
