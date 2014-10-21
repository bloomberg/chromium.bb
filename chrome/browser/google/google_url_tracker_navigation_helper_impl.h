// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_NAVIGATION_HELPER_IMPL_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_NAVIGATION_HELPER_IMPL_H_

#include "components/google/core/browser/google_url_tracker_navigation_helper.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class GoogleURLTrackerNavigationHelperImpl
    : public GoogleURLTrackerNavigationHelper,
      public content::NotificationObserver {
 public:
  GoogleURLTrackerNavigationHelperImpl(content::WebContents* web_contents,
                                       GoogleURLTracker* tracker);
  ~GoogleURLTrackerNavigationHelperImpl() override;

  // GoogleURLTrackerNavigationHelper:
  void SetListeningForNavigationCommit(bool listen) override;
  bool IsListeningForNavigationCommit() override;
  void SetListeningForTabDestruction(bool listen) override;
  bool IsListeningForTabDestruction() override;
  void OpenURL(GURL url,
               WindowOpenDisposition disposition,
               bool user_clicked_on_link) override;

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  content::WebContents* web_contents_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerNavigationHelperImpl);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_NAVIGATION_HELPER_IMPL_H_
