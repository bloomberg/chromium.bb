// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class GURL;

// Per-WebContents class to handle updating the site engagement scores for
// origins as the user navigates.
class SiteEngagementHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SiteEngagementHelper> {
 public:
  ~SiteEngagementHelper() override;

 private:
  explicit SiteEngagementHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SiteEngagementHelper>;

  // content::WebContentsObserver overrides.
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) override;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementHelper);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_HELPER_H_
