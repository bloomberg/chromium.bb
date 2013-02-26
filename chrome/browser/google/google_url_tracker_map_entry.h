// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_MAP_ENTRY_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_MAP_ENTRY_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GoogleURLTracker;
class GoogleURLTrackerInfoBarDelegate;
class InfoBarService;

namespace content {
class NavigationController;
}

class GoogleURLTrackerMapEntry : public content::NotificationObserver {
 public:
  GoogleURLTrackerMapEntry(
      GoogleURLTracker* google_url_tracker,
      InfoBarService* infobar_service,
      const content::NavigationController* navigation_controller);
  virtual ~GoogleURLTrackerMapEntry();

  bool has_infobar() const { return !!infobar_; }
  GoogleURLTrackerInfoBarDelegate* infobar() { return infobar_; }
  void SetInfoBar(GoogleURLTrackerInfoBarDelegate* infobar);

  const content::NavigationController* navigation_controller() const {
    return navigation_controller_;
  }

  void Close(bool redo_search);

 private:
  friend class GoogleURLTrackerTest;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;
  GoogleURLTracker* const google_url_tracker_;
  const InfoBarService* const infobar_service_;
  GoogleURLTrackerInfoBarDelegate* infobar_;
  const content::NavigationController* const navigation_controller_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerMapEntry);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_MAP_ENTRY_H_
