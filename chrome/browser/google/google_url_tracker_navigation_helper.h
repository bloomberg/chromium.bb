// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_NAVIGATION_HELPER_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_NAVIGATION_HELPER_H_

class GoogleURLTracker;
class InfoBarService;
class Profile;

namespace content {
class NavigationController;
}

// A helper class for GoogleURLTracker that abstracts the details of listening
// for various navigation events.
class GoogleURLTrackerNavigationHelper {
 public:
  virtual ~GoogleURLTrackerNavigationHelper() {}

  // Sets the GoogleURLTracker that should receive callbacks from this observer.
  virtual void SetGoogleURLTracker(GoogleURLTracker* tracker) = 0;

  // Enables or disables listening for navigation starts. OnNavigationPending
  // will be called for each navigation start if listening is enabled.
  virtual void SetListeningForNavigationStart(bool listen) = 0;

  // Returns whether or not the observer is currently listening for navigation
  // starts.
  virtual bool IsListeningForNavigationStart() = 0;

  // Enables or disables listening for navigation commits for the given
  // NavigationController. OnNavigationCommitted will be called for each
  // navigation commit if listening is enabled.
  virtual void SetListeningForNavigationCommit(
      const content::NavigationController* nav_controller,
      bool listen) = 0;

  // Returns whether or not the observer is currently listening for navigation
  // commits for the given NavigationController.
  virtual bool IsListeningForNavigationCommit(
      const content::NavigationController* nav_controller) = 0;

  // Enables or disables listening for tab destruction for the given
  // NavigationController. OnTabClosed will be called on tab destruction if
  // listening is enabled.
  virtual void SetListeningForTabDestruction(
      const content::NavigationController* nav_controller,
      bool listen) = 0;

  // Returns whether or not the observer is currently listening for tab
  // destruction for the given NavigationController.
  virtual bool IsListeningForTabDestruction(
      const content::NavigationController* nav_controller) = 0;
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_NAVIGATION_HELPER_H_
