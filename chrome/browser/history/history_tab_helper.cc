// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_tab_helper.h"

#include <utility>

#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/frame_navigate_params.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#endif

using content::NavigationEntry;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(HistoryTabHelper);

HistoryTabHelper::HistoryTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      received_page_title_(false) {
}

HistoryTabHelper::~HistoryTabHelper() {
}

void HistoryTabHelper::UpdateHistoryForNavigation(
    const history::HistoryAddPageArgs& add_page_args) {
  HistoryService* hs = GetHistoryService();
  if (hs)
    GetHistoryService()->AddPage(add_page_args);
}

void HistoryTabHelper::UpdateHistoryPageTitle(const NavigationEntry& entry) {
  HistoryService* hs = GetHistoryService();
  if (hs)
    hs->SetPageTitle(entry.GetVirtualURL(),
                     entry.GetTitleForDisplay(std::string()));
}

history::HistoryAddPageArgs
HistoryTabHelper::CreateHistoryAddPageArgs(
    const GURL& virtual_url,
    base::Time timestamp,
    bool did_replace_entry,
    const content::FrameNavigateParams& params) {
  history::HistoryAddPageArgs add_page_args(
      params.url, timestamp, web_contents(), params.page_id,
      params.referrer.url, params.redirects, params.transition,
      history::SOURCE_BROWSED, did_replace_entry);
  if (content::PageTransitionIsMainFrame(params.transition) &&
      virtual_url != params.url) {
    // Hack on the "virtual" URL so that it will appear in history. For some
    // types of URLs, we will display a magic URL that is different from where
    // the page is actually navigated. We want the user to see in history what
    // they saw in the URL bar, so we add the virtual URL as a redirect.  This
    // only applies to the main frame, as the virtual URL doesn't apply to
    // sub-frames.
    add_page_args.url = virtual_url;
    if (!add_page_args.redirects.empty())
      add_page_args.redirects.back() = virtual_url;
  }
  return add_page_args;
}

void HistoryTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Allow the new page to set the title again.
  received_page_title_ = false;
}

void HistoryTabHelper::DidNavigateAnyFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Update history. Note that this needs to happen after the entry is complete,
  // which WillNavigate[Main,Sub]Frame will do before this function is called.
  if (!params.should_update_history)
    return;

  // Most of the time, the displayURL matches the loaded URL, but for about:
  // URLs, we use a data: URL as the real value.  We actually want to save the
  // about: URL to the history db and keep the data: URL hidden. This is what
  // the WebContents' URL getter does.
  const history::HistoryAddPageArgs& add_page_args =
      CreateHistoryAddPageArgs(
          web_contents()->GetURL(), details.entry->GetTimestamp(),
          details.did_replace_entry, params);

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (prerender_manager) {
    prerender::PrerenderContents* prerender_contents =
        prerender_manager->GetPrerenderContents(web_contents());
    if (prerender_contents) {
      prerender_contents->DidNavigate(add_page_args);
      return;
    }
  }

#if !defined(OS_ANDROID)
  // Don't update history if this web contents isn't associatd with a tab.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser || browser->is_app())
    return;
#endif

  UpdateHistoryForNavigation(add_page_args);
}

void HistoryTabHelper::TitleWasSet(NavigationEntry* entry, bool explicit_set) {
  if (received_page_title_)
    return;

  if (entry) {
    UpdateHistoryPageTitle(*entry);
    received_page_title_ = explicit_set;
  }
}

HistoryService* HistoryTabHelper::GetHistoryService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return NULL;

  return HistoryServiceFactory::GetForProfile(profile,
                                              Profile::IMPLICIT_ACCESS);
}

void HistoryTabHelper::WebContentsDestroyed() {
  // We update the history for this URL.
  WebContents* tab = web_contents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return;

  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile, Profile::IMPLICIT_ACCESS);
  if (hs) {
    NavigationEntry* entry = tab->GetController().GetLastCommittedEntry();
    if (entry) {
      hs->UpdateWithPageEndTime(tab, entry->GetPageID(), tab->GetURL(),
                                base::Time::Now());
    }
    hs->ClearCachedDataForContextID(tab);
  }
}
