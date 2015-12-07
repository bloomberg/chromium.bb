// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

void RecordBookmarkLaunch(BookmarkLaunchLocation launchLocation) {
  DCHECK(launchLocation < BOOKMARK_LAUNCH_LOCATION_COUNT);
  UMA_HISTOGRAM_ENUMERATION("Stars.LaunchLocation", launchLocation,
                            BOOKMARK_LAUNCH_LOCATION_COUNT);
}
