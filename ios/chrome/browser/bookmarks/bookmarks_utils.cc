// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"

void RecordBookmarkLaunch(BookmarkLaunchLocation launch_location) {
  DCHECK(launch_location < BOOKMARK_LAUNCH_LOCATION_COUNT);
  UMA_HISTOGRAM_ENUMERATION("Stars.LaunchLocation", launch_location,
                            BOOKMARK_LAUNCH_LOCATION_COUNT);
}

bool RemoveAllUserBookmarksIOS(ios::ChromeBrowserState* browser_state) {
  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(browser_state);

  if (!bookmark_model->loaded())
    return false;

  bookmark_model->RemoveAllUserBookmarks();

  for (int i = 0; i < bookmark_model->root_node()->child_count(); ++i) {
    if (!bookmark_model->client()->CanBeEditedByUser(
            bookmark_model->root_node()->GetChild(i)))
      continue;
    CHECK(bookmark_model->root_node()->GetChild(i)->empty())
        << "Failed to remove all user bookmarks.";
  }

  // The default save folder is reset to the generic one.
  browser_state->GetPrefs()->SetInt64(prefs::kIosBookmarkFolderDefault, -1);

  return true;
}
