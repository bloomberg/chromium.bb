// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_NAVIGATION_HELPER_IMPL_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_NAVIGATION_HELPER_IMPL_H_

#include "chrome/browser/google/google_url_tracker_navigation_helper.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

class GoogleURLTrackerNavigationHelperImpl
    : public GoogleURLTrackerNavigationHelper,
      public content::NotificationObserver {
 public:
  explicit GoogleURLTrackerNavigationHelperImpl();
  virtual ~GoogleURLTrackerNavigationHelperImpl();

  // GoogleURLTrackerNavigationHelper.
  virtual void SetGoogleURLTracker(GoogleURLTracker* tracker) OVERRIDE;
  virtual void SetListeningForNavigationStart(bool listen) OVERRIDE;
  virtual bool IsListeningForNavigationStart() OVERRIDE;
  virtual void SetListeningForNavigationCommit(
      const content::NavigationController* nav_controller,
      bool listen) OVERRIDE;
  virtual bool IsListeningForNavigationCommit(
      const content::NavigationController* nav_controller) OVERRIDE;
  virtual void SetListeningForTabDestruction(
      const content::NavigationController* nav_controller,
      bool listen) OVERRIDE;
  virtual bool IsListeningForTabDestruction(
      const content::NavigationController* nav_controller) OVERRIDE;

 private:
  friend class GoogleURLTrackerNavigationHelperTest;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Handles instant commit notifications by simulating the relevant navigation
  // callbacks.
  void OnInstantCommitted(content::NavigationController* nav_controller,
                          InfoBarService* infobar_service,
                          const GURL& search_url);

  // Returns a WebContents NavigationSource for the WebContents corresponding to
  // the given NavigationController NotificationSource.
  virtual content::NotificationSource GetWebContentsSource(
      const content::NotificationSource& nav_controller_source);

  GoogleURLTracker* tracker_;
  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_NAVIGATION_HELPER_IMPL_H_
