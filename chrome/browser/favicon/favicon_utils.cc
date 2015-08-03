// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_utils.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/favicon_url.h"

namespace favicon {

void CreateContentFaviconDriverForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (ContentFaviconDriver::FromWebContents(web_contents))
    return;

  Profile* original_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetOriginalProfile();
  return ContentFaviconDriver::CreateForWebContents(
      web_contents, FaviconServiceFactory::GetForProfile(
                        original_profile, ServiceAccessType::IMPLICIT_ACCESS),
      HistoryServiceFactory::GetForProfile(original_profile,
                                           ServiceAccessType::IMPLICIT_ACCESS),
      BookmarkModelFactory::GetForProfileIfExists(original_profile));
}

bool ShouldDisplayFavicon(content::WebContents* web_contents) {
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
  if (search::IsInstantNTP(web_contents))
    return false;

  return true;
}

}  // namespace favicon
