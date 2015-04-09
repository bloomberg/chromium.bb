// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_tab_helper.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/favicon_url.h"

// static
void FaviconTabHelper::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (FromWebContents(web_contents))
    return;

  Profile* original_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetOriginalProfile();
  web_contents->SetUserData(
      UserDataKey(),
      new FaviconTabHelper(
          web_contents,
          FaviconServiceFactory::GetForProfile(
              original_profile, ServiceAccessType::IMPLICIT_ACCESS),
          HistoryServiceFactory::GetForProfile(
              original_profile, ServiceAccessType::IMPLICIT_ACCESS),
          BookmarkModelFactory::GetForProfileIfExists(original_profile)));
}

// static
FaviconTabHelper* FaviconTabHelper::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<FaviconTabHelper*>(
      favicon::ContentFaviconDriver::FromWebContents(web_contents));
}

// static
bool FaviconTabHelper::ShouldDisplayFavicon(
    content::WebContents* web_contents) {
  // Always display a throbber during pending loads.
  const content::NavigationController& controller =
      web_contents->GetController();
  if (controller.GetLastCommittedEntry() && controller.GetPendingEntry())
    return true;

  GURL url = web_contents->GetURL();
  if (url.SchemeIs(content::kChromeUIScheme) &&
      url.host() == chrome::kChromeUINewTabHost) {
    return false;
  }

  // No favicon on Instant New Tab Pages.
  if (chrome::IsInstantNTP(web_contents))
    return false;

  return true;
}

FaviconTabHelper::FaviconTabHelper(content::WebContents* web_contents,
                                   favicon::FaviconService* favicon_service,
                                   history::HistoryService* history_service,
                                   bookmarks::BookmarkModel* bookmark_model)
    : favicon::ContentFaviconDriver(web_contents,
                                    favicon_service,
                                    history_service,
                                    bookmark_model) {
}
