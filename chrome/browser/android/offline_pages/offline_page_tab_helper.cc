// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/offline_page_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/page_transition_types.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(offline_pages::OfflinePageTabHelper);

namespace offline_pages {

OfflinePageTabHelper::OfflinePageTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      weak_ptr_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

OfflinePageTabHelper::~OfflinePageTabHelper() {}

void OfflinePageTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Skips non-main frame.
  if (!navigation_handle->IsInMainFrame())
    return;

  // Redirecting to online version will only take effect when there is network
  // connection.
  if (net::NetworkChangeNotifier::IsOffline())
    return;

  // Ignore navigations that are forward or back transitions in the nav stack
  // which are not at the head of the stack.
  const content::NavigationController& controller =
      web_contents()->GetController();
  if (controller.GetEntryCount() > 0 &&
      controller.GetCurrentEntryIndex() != -1 &&
      controller.GetCurrentEntryIndex() < controller.GetEntryCount() - 1) {
    return;
  }

  // Skips if not loading an offline copy of saved page.
  GURL online_url = offline_pages::OfflinePageUtils::GetOnlineURLForOfflineURL(
      web_contents()->GetBrowserContext(), navigation_handle->GetURL());
  if (!online_url.is_valid())
    return;

  // Avoids looping between online and offline redirections.
  content::NavigationEntry* entry = controller.GetPendingEntry();
  if (entry && !entry->GetRedirectChain().empty() &&
      entry->GetRedirectChain().back() == online_url) {
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&OfflinePageTabHelper::Redirect,
                 weak_ptr_factory_.GetWeakPtr(),
                 navigation_handle->GetURL(), online_url));
}

void OfflinePageTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Skips non-main frame.
  if (!navigation_handle->IsInMainFrame())
    return;

  // Skips load failure other than no network.
  net::Error error_code = navigation_handle->GetNetErrorCode();
  if (error_code != net::ERR_INTERNET_DISCONNECTED &&
      error_code != net::ERR_NAME_NOT_RESOLVED &&
      error_code != net::ERR_ADDRESS_UNREACHABLE &&
      error_code != net::ERR_PROXY_CONNECTION_FAILED) {
    return;
  }

  // On a forward or back transition, don't affect the order of the nav stack.
  if (navigation_handle->GetPageTransition() ==
      ui::PAGE_TRANSITION_FORWARD_BACK) {
    return;
  }

  // Skips if not loading an online version of saved page.
  GURL offline_url = offline_pages::OfflinePageUtils::GetOfflineURLForOnlineURL(
      web_contents()->GetBrowserContext(), navigation_handle->GetURL());
  if (!offline_url.is_valid())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&OfflinePageTabHelper::Redirect,
                 weak_ptr_factory_.GetWeakPtr(),
                 navigation_handle->GetURL(), offline_url));
}

void OfflinePageTabHelper::Redirect(
    const GURL& from_url, const GURL& to_url) {
  if (to_url.SchemeIsFile())
    UMA_HISTOGRAM_COUNTS("OfflinePages.RedirectToOfflineCount", 1);
  else
    UMA_HISTOGRAM_COUNTS("OfflinePages.RedirectToOnlineCount", 1);

  content::NavigationController::LoadURLParams load_params(to_url);
  load_params.transition_type = ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  load_params.redirect_chain.push_back(from_url);
  web_contents()->GetController().LoadURLWithParams(load_params);
}

}  // namespace offline_pages
