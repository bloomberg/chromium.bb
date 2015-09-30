// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_helper.h"

#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SiteEngagementHelper);

SiteEngagementHelper::~SiteEngagementHelper() {
}

SiteEngagementHelper::SiteEngagementHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

void SiteEngagementHelper::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents());

  // Ignore pre-render loads.
  if (prerender_contents != NULL)
    return;

  // Ignore all schemes except HTTP and HTTPS.
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile);
  // Service is null in incognito.
  if (!service)
    return;

  service->HandleNavigation(url);
}
