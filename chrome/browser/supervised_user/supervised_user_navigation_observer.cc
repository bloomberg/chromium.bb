// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"

using base::Time;
using content::NavigationEntry;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SupervisedUserNavigationObserver);

SupervisedUserNavigationObserver::~SupervisedUserNavigationObserver() {
}

SupervisedUserNavigationObserver::SupervisedUserNavigationObserver(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  url_filter_ = supervised_user_service->GetURLFilterForUIThread();
}

// static
void SupervisedUserNavigationObserver::OnRequestBlocked(
    int render_process_host_id,
    int render_view_id,
    const GURL& url,
    SupervisedUserURLFilter::FilteringBehaviorReason reason,
    const base::Callback<void(bool)>& callback) {
  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, render_view_id);
  if (!web_contents) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE, base::Bind(callback, false));
    return;
  }

  SupervisedUserNavigationObserver* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents);
  if (navigation_observer)
    navigation_observer->OnRequestBlockedInternal(url);

  // Show the interstitial.
  SupervisedUserInterstitial::Show(web_contents, url, reason, callback);
}

void SupervisedUserNavigationObserver::OnRequestBlockedInternal(
    const GURL& url) {
  Time timestamp = Time::Now();  // TODO(bauerb): Use SaneTime when available.
  // Create a history entry for the attempt and mark it as such.
  history::HistoryAddPageArgs add_page_args(
      url, timestamp, history::ContextIDForWebContents(web_contents_), 0, url,
      history::RedirectList(), ui::PAGE_TRANSITION_BLOCKED,
      history::SOURCE_BROWSED, false);

  // Add the entry to the history database.
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);

  // |history_service| is null if saving history is disabled.
  if (history_service)
    history_service->AddPage(add_page_args);

  scoped_ptr<NavigationEntry> entry(NavigationEntry::Create());
  entry->SetVirtualURL(url);
  entry->SetTimestamp(timestamp);
  blocked_navigations_.push_back(entry.release());
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  supervised_user_service->DidBlockNavigation(web_contents_);
}
