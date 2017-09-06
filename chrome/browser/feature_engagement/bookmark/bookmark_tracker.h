// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_ENGAGEMENT_BOOKMARK_BOOKMARK_TRACKER_H_
#define CHROME_BROWSER_FEATURE_ENGAGEMENT_BOOKMARK_BOOKMARK_TRACKER_H_

#include "base/macros.h"
#include "chrome/browser/feature_engagement/feature_tracker.h"

class Profile;
class SessionDurationUpdater;

namespace feature_engagement {

// The BookmarkTracker provides a backend for displaying in-product help for the
// bookmarks. BookmarkTracker is the interface through which the event
// constants for the Bookmark feature can be be altered. Once all of the event
// constants are met, BookmarkTracker calls for the BookmarkPromo to be shown,
// along with recording when the BookmarkPromo is dismissed. The requirements to
// show the BookmarkPromo are as follows:
//
// - At least five hours of observed session time have elapsed.
// - The user has never added another bookmark through any means.
// - The user has navigated to a URL that they have visited at least 3 times.
// - This URL cannot be the home page or new tab page.
class BookmarkTracker : public FeatureTracker {
 public:
  BookmarkTracker(Profile* profile,
                  SessionDurationUpdater* session_duration_updater);

  // Alerts the bookmark tracker that a bookmark was added.
  void OnBookmarkAdded();
  // Checks if the promo should be displayed since a known URL has been visited.
  void OnVisitedKnownURL();
  // Clears the flag for whether there is any in-product help being displayed.
  void OnPromoClosed();

 protected:
  // Alternate constructor to support unit testing.
  explicit BookmarkTracker(SessionDurationUpdater* session_duration_updater);
  ~BookmarkTracker() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BookmarkTrackerEventTest, TestOnSessionTimeMet);
  FRIEND_TEST_ALL_PREFIXES(BookmarkTrackerTest, TestShouldNotShowPromo);
  FRIEND_TEST_ALL_PREFIXES(BookmarkTrackerTest, TestShouldShowPromo);

  // FeatureTracker:
  void OnSessionTimeMet() override;

  // Sets the BookmarkInProductHelp pref to true and calls the Bookmark Promo.
  void ShowPromo();

  DISALLOW_COPY_AND_ASSIGN(BookmarkTracker);
};

}  // namespace feature_engagement

#endif  // CHROME_BROWSER_FEATURE_ENGAGEMENT_BOOKMARK_BOOKMARK_TRACKER_H_
