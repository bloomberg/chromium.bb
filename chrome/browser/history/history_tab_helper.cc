// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_tab_helper.h"

#include <utility>

#include "chrome/browser/history/history.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"

using content::NavigationEntry;
using content::WebContents;

HistoryTabHelper::HistoryTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      received_page_title_(false) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
                 content::Source<WebContents>(web_contents));
}

HistoryTabHelper::~HistoryTabHelper() {
}

void HistoryTabHelper::UpdateHistoryForNavigation(
    scoped_refptr<history::HistoryAddPageArgs> add_page_args) {
  HistoryService* hs = GetHistoryService();
  if (hs)
    GetHistoryService()->AddPage(*add_page_args);
}

void HistoryTabHelper::UpdateHistoryPageTitle(const NavigationEntry& entry) {
  HistoryService* hs = GetHistoryService();
  if (hs)
    hs->SetPageTitle(entry.GetVirtualURL(), entry.GetTitleForDisplay(""));
}

scoped_refptr<history::HistoryAddPageArgs>
HistoryTabHelper::CreateHistoryAddPageArgs(
    const GURL& virtual_url,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  scoped_refptr<history::HistoryAddPageArgs> add_page_args(
      new history::HistoryAddPageArgs(
          params.url, base::Time::Now(), web_contents(), params.page_id,
          params.referrer.url, params.redirects, params.transition,
          history::SOURCE_BROWSED, details.did_replace_entry));
  if (content::PageTransitionIsMainFrame(params.transition) &&
      virtual_url != params.url) {
    // Hack on the "virtual" URL so that it will appear in history. For some
    // types of URLs, we will display a magic URL that is different from where
    // the page is actually navigated. We want the user to see in history what
    // they saw in the URL bar, so we add the virtual URL as a redirect.  This
    // only applies to the main frame, as the virtual URL doesn't apply to
    // sub-frames.
    add_page_args->url = virtual_url;
    if (!add_page_args->redirects.empty())
      add_page_args->redirects.back() = virtual_url;
  }
  return add_page_args;
}

bool HistoryTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(HistoryTabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_PageContents, OnPageContents)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_Thumbnail, OnThumbnail)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
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
  scoped_refptr<history::HistoryAddPageArgs> add_page_args(
      CreateHistoryAddPageArgs(web_contents()->GetURL(), details, params));
  if (!web_contents()->GetDelegate() ||
      !web_contents()->GetDelegate()->ShouldAddNavigationToHistory(
          *add_page_args, details.type))
    return;

  UpdateHistoryForNavigation(add_page_args);
}

void HistoryTabHelper::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED);
  std::pair<content::NavigationEntry*, bool>* title =
      content::Details<std::pair<content::NavigationEntry*, bool> >(
          details).ptr();

  if (received_page_title_)
    return;

  if (title->first) {
    UpdateHistoryPageTitle(*title->first);
    received_page_title_ = title->second;
  }
}

void HistoryTabHelper::OnPageContents(const GURL& url,
                                      int32 page_id,
                                      const string16& contents) {
  // Don't index any https pages. People generally don't want their bank
  // accounts, etc. indexed on their computer, especially since some of these
  // things are not marked cachable.
  // TODO(brettw) we may want to consider more elaborate heuristics such as
  // the cachability of the page. We may also want to consider subframes (this
  // test will still index subframes if the subframe is SSL).
  // TODO(zelidrag) bug chromium-os:2808 - figure out if we want to reenable
  // content indexing for chromeos in some future releases.
#if !defined(OS_CHROMEOS)
  if (!url.SchemeIsSecure()) {
    HistoryService* hs = GetHistoryService();
    if (hs)
      hs->SetPageContents(url, contents);
  }
#endif
}

void HistoryTabHelper::OnThumbnail(const GURL& url,
                                   const ThumbnailScore& score,
                                   const SkBitmap& bitmap) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return;

  // Tell History about this thumbnail.
  history::TopSites* ts = profile->GetTopSites();
  if (ts) {
    gfx::Image thumbnail(bitmap);
    ts->SetPageThumbnail(url, &thumbnail, score);
  }
}

HistoryService* HistoryTabHelper::GetHistoryService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return NULL;

  return profile->GetHistoryService(Profile::IMPLICIT_ACCESS);
}

void HistoryTabHelper::WebContentsDestroyed(WebContents* tab) {
  // We update the history for this URL.
  // The content returned from web_contents() has been destroyed by now.
  // We need to use tab value directly.
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return;

  HistoryService* hs = profile->GetHistoryService(Profile::IMPLICIT_ACCESS);
  if (hs) {
    NavigationEntry* entry = tab->GetController().GetLastCommittedEntry();
    if (entry) {
      hs->UpdateWithPageEndTime(tab, entry->GetPageID(), tab->GetURL(),
                                base::Time::Now());
    }
  }
}
