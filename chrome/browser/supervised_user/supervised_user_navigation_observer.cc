// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"

using base::Time;
using content::NavigationEntry;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SupervisedUserNavigationObserver);

SupervisedUserNavigationObserver::~SupervisedUserNavigationObserver() {
  supervised_user_service_->RemoveObserver(this);
}

SupervisedUserNavigationObserver::SupervisedUserNavigationObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), weak_ptr_factory_(this) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  supervised_user_service_ =
      SupervisedUserServiceFactory::GetForProfile(profile);
  url_filter_ = supervised_user_service_->GetURLFilter();
  supervised_user_service_->AddObserver(this);
}

// static
void SupervisedUserNavigationObserver::OnRequestBlocked(
    content::WebContents* web_contents,
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason,
    const base::Callback<void(bool)>& callback) {
  SupervisedUserNavigationObserver* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents);

  // Cancel the navigation if there is no navigation observer.
  if (!navigation_observer) {
    callback.Run(false);
    return;
  }

  navigation_observer->OnRequestBlockedInternal(url, reason, callback);
}

void SupervisedUserNavigationObserver::DidFinishNavigation(
      content::NavigationHandle* navigation_handle) {
  // Only filter same page navigations (eg. pushState/popState); others will
  // have been filtered by the NavigationThrottle.
  if (!navigation_handle->IsSameDocument())
    return;

  if (!navigation_handle->IsInMainFrame())
    return;

  url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
      web_contents()->GetLastCommittedURL(),
      base::Bind(&SupervisedUserNavigationObserver::URLFilterCheckCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 navigation_handle->GetURL()));
}

void SupervisedUserNavigationObserver::OnURLFilterChanged() {
  url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
      web_contents()->GetLastCommittedURL(),
      base::Bind(&SupervisedUserNavigationObserver::URLFilterCheckCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 web_contents()->GetLastCommittedURL()));
}

void SupervisedUserNavigationObserver::OnRequestBlockedInternal(
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason,
    const base::Callback<void(bool)>& callback) {
  Time timestamp = Time::Now();  // TODO(bauerb): Use SaneTime when available.
  // Create a history entry for the attempt and mark it as such.
  history::HistoryAddPageArgs add_page_args(
      url, timestamp, history::ContextIDForWebContents(web_contents()), 0, url,
      history::RedirectList(), ui::PAGE_TRANSITION_BLOCKED,
      history::SOURCE_BROWSED, false, true);

  // Add the entry to the history database.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);

  // |history_service| is null if saving history is disabled.
  if (history_service)
    history_service->AddPage(add_page_args);

  std::unique_ptr<NavigationEntry> entry = NavigationEntry::Create();
  entry->SetVirtualURL(url);
  entry->SetTimestamp(timestamp);
  auto serialized_entry = base::MakeUnique<sessions::SerializedNavigationEntry>(
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
          blocked_navigations_.size(), *entry));
  blocked_navigations_.push_back(std::move(serialized_entry));
  supervised_user_service_->DidBlockNavigation(web_contents());

  // Show the interstitial.
  const bool initial_page_load = true;
  MaybeShowInterstitial(url, reason, initial_page_load, callback);
}

void SupervisedUserNavigationObserver::URLFilterCheckCallback(
    const GURL& url,
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool uncertain) {
  // If the page has been changed in the meantime, we can exit.
  if (url != web_contents()->GetLastCommittedURL())
    return;

  if (behavior == SupervisedUserURLFilter::FilteringBehavior::BLOCK) {
    const bool initial_page_load = false;
    MaybeShowInterstitial(url, reason, initial_page_load,
                          base::Callback<void(bool)>());
  }
}

void SupervisedUserNavigationObserver::MaybeShowInterstitial(
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool initial_page_load,
    const base::Callback<void(bool)>& callback) {
  if (is_showing_interstitial_)
    return;

  is_showing_interstitial_ = true;
  base::Callback<void(bool)> wrapped_callback =
      base::Bind(&SupervisedUserNavigationObserver::OnInterstitialResult,
                 weak_ptr_factory_.GetWeakPtr(), callback);
  SupervisedUserInterstitial::Show(web_contents(), url, reason,
                                   initial_page_load, wrapped_callback);
}

void SupervisedUserNavigationObserver::OnInterstitialResult(
    const base::Callback<void(bool)>& callback,
    bool result) {
  is_showing_interstitial_ = false;
  if (callback)
    callback.Run(result);
}
