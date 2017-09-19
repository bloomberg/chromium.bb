// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_ENGAGEMENT_INCOGNITO_WINDOW_INCOGNITO_WINDOW_TRACKER_H_
#define CHROME_BROWSER_FEATURE_ENGAGEMENT_INCOGNITO_WINDOW_INCOGNITO_WINDOW_TRACKER_H_

#include "chrome/browser/feature_engagement/feature_tracker.h"

#include "chrome/browser/feature_engagement/session_duration_updater.h"
#include "chrome/browser/feature_engagement/session_duration_updater_factory.h"

namespace feature_engagement {

// The IncognitoWindowTracker provides a backend for displaying in-product help
// for the incognito window. IncognitoWindowTracker is the interface through
// which the event constants for the IncognitoWindow feature can be be altered.
// Once all of the event constants are met, IncognitoWindowTracker calls for the
// IncognitoWindowPromo to be shown, along with recording when the
// IncognitoWindowPromo is dismissed. The requirements to show the
// IncognitoWindowPromo are as follows:
//
// - At least two hours of observed session time have elapsed.
// - The user has never opened incognito window through any means.
// - The user has cleared browsing data.
class IncognitoWindowTracker : public FeatureTracker {
 public:
  IncognitoWindowTracker(Profile* profile,
                         SessionDurationUpdater* session_duration_updater);

  // Alerts the incognito window tracker that an incognito window was opened.
  void OnIncognitoWindowOpened();
  // Alerts the incognito window tracker that browsing history was deleted.
  void OnBrowsingDataCleared();
  // Clears the flag for whether there is any in-product help being displayed.
  void OnPromoClosed();

 protected:
  // Alternate constructor to support unit testing.
  explicit IncognitoWindowTracker(
      SessionDurationUpdater* session_duration_updater);
  ~IncognitoWindowTracker() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(IncognitoWindowTrackerEventTest,
                           TestOnSessionTimeMet);
  FRIEND_TEST_ALL_PREFIXES(IncognitoWindowTrackerTest, TestShouldNotShowPromo);
  FRIEND_TEST_ALL_PREFIXES(IncognitoWindowTrackerTest, TestShouldShowPromo);

  // FeatureTracker:
  void OnSessionTimeMet() override;

  // Shows the Incognito Window in-product help promo bubble.
  void ShowPromo();

  DISALLOW_COPY_AND_ASSIGN(IncognitoWindowTracker);
};

}  // namespace feature_engagement

#endif  // CHROME_BROWSER_FEATURE_ENGAGEMENT_INCOGNITO_WINDOW_INCOGNITO_WINDOW_TRACKER_H_
