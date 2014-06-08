// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_NAVIGATION_HELPER_H_
#define COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_NAVIGATION_HELPER_H_

#include "base/macros.h"
#include "ui/base/window_open_disposition.h"

class GoogleURLTracker;
class GURL;

// Interface via which GoogleURLTracker communicates with its driver.
// TODO(blundell): Rename this class to GoogleURLTrackerDriver.
// crbug.com/373221
class GoogleURLTrackerNavigationHelper {
 public:
  explicit GoogleURLTrackerNavigationHelper(
      GoogleURLTracker* google_url_tracker);
  virtual ~GoogleURLTrackerNavigationHelper();

  // Enables or disables listening for navigation commits.
  // OnNavigationCommitted will be called for each navigation commit if
  // listening is enabled.
  virtual void SetListeningForNavigationCommit(bool listen) = 0;

  // Returns whether or not this object is currently listening for navigation
  // commits.
  virtual bool IsListeningForNavigationCommit() = 0;

  // Enables or disables listening for tab destruction. OnTabClosed will be
  // called on tab destruction if listening is enabled.
  virtual void SetListeningForTabDestruction(bool listen) = 0;

  // Returns whether or not this object is currently listening for tab
  // destruction.
  virtual bool IsListeningForTabDestruction() = 0;

  // Opens |url| with the given window disposition.
  virtual void OpenURL(GURL url,
                       WindowOpenDisposition disposition,
                       bool user_clicked_on_link) = 0;

 protected:
  GoogleURLTracker* google_url_tracker() { return google_url_tracker_; }

 private:
  GoogleURLTracker* google_url_tracker_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerNavigationHelper);
};

#endif  // COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_URL_TRACKER_NAVIGATION_HELPER_H_
