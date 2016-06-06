// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread_task_runner_handle.h"
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
namespace {

void ReportAccessedOfflinePage(content::BrowserContext* browser_context,
                               const GURL& navigated_url,
                               const GURL& online_url) {
  // If there is a valid online URL for this navigated URL, then we are looking
  // at an offline page.
  if (online_url.is_valid())
    OfflinePageUtils::MarkPageAccessed(browser_context, navigated_url);
}

}  // namespace

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

  // This is a new navigation so we can invalidate any previously scheduled
  // operations.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Ignore navigations that are forward or back transitions in the nav stack
  // which are not at the head of the stack.
  const content::NavigationController& controller =
      web_contents()->GetController();
  if (controller.GetEntryCount() > 0 &&
      controller.GetCurrentEntryIndex() != -1 &&
      controller.GetCurrentEntryIndex() < controller.GetEntryCount() - 1) {
    return;
  }

  content::BrowserContext* context = web_contents()->GetBrowserContext();
  GURL navigated_url = navigation_handle->GetURL();
  auto redirect_url_callback =
      base::Bind(&OfflinePageTabHelper::GotRedirectURLForStartedNavigation,
                 weak_ptr_factory_.GetWeakPtr(), navigated_url);
  if (net::NetworkChangeNotifier::IsOffline()) {
    OfflinePageUtils::GetOfflineURLForOnlineURL(context, navigated_url,
                                                redirect_url_callback);
  } else {
    OfflinePageUtils::GetOnlineURLForOfflineURL(context, navigated_url,
                                                redirect_url_callback);
  }
}

void OfflinePageTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Skips non-main frame.
  if (!navigation_handle->IsInMainFrame())
    return;

  GURL navigated_url = navigation_handle->GetURL();
  net::Error error_code = navigation_handle->GetNetErrorCode();
  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();

  // If the offline page is being loaded successfully, set the access record but
  // no need to do anything else.
  if (error_code == net::OK) {
    OfflinePageUtils::GetOnlineURLForOfflineURL(
        browser_context, navigated_url,
        base::Bind(&ReportAccessedOfflinePage, browser_context, navigated_url));
    return;
  }

  // When the navigation starts, we redirect immediately from online page to
  // offline version on the case that there is no network connection. If there
  // is still network connection but with no or poor network connectivity, the
  // navigation will eventually fail and we want to redirect to offline copy
  // in this case. If error code doesn't match this list, then we still show
  // the error page and not an offline page, so do nothing.
  if (error_code != net::ERR_INTERNET_DISCONNECTED &&
      error_code != net::ERR_NAME_NOT_RESOLVED &&
      error_code != net::ERR_ADDRESS_UNREACHABLE &&
      error_code != net::ERR_PROXY_CONNECTION_FAILED) {
    return;
  }

  // Otherwise, get the offline URL for this url, and attempt a redirect if
  // necessary.
  OfflinePageUtils::GetOfflineURLForOnlineURL(
      browser_context, navigated_url,
      base::Bind(&OfflinePageTabHelper::GotRedirectURLForSupportedErrorCode,
                 weak_ptr_factory_.GetWeakPtr(),
                 navigation_handle->GetPageTransition(), navigated_url));
}

void OfflinePageTabHelper::GotRedirectURLForSupportedErrorCode(
    ui::PageTransition transition,
    const GURL& from_url,
    const GURL& redirect_url) {
  // If we didn't find an offline URL, or we are doing a forward/back
  // transition, don't redirect.
  bool do_redirect =
      redirect_url.is_valid() && transition != ui::PAGE_TRANSITION_FORWARD_BACK;
  UMA_HISTOGRAM_BOOLEAN("OfflinePages.ShowOfflinePageOnBadNetwork",
                        do_redirect);
  if (!do_redirect)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&OfflinePageTabHelper::Redirect,
                 weak_ptr_factory_.GetWeakPtr(), from_url, redirect_url));
}

void OfflinePageTabHelper::GotRedirectURLForStartedNavigation(
    const GURL& from_url,
    const GURL& redirect_url) {
  // Bails out if no redirection is needed.
  if (!redirect_url.is_valid())
    return;

  const content::NavigationController& controller =
      web_contents()->GetController();

  // Avoids looping between online and offline redirections.
  content::NavigationEntry* entry = controller.GetPendingEntry();
  if (entry && !entry->GetRedirectChain().empty() &&
      entry->GetRedirectChain().back() == redirect_url) {
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&OfflinePageTabHelper::Redirect,
                 weak_ptr_factory_.GetWeakPtr(), from_url, redirect_url));
}

void OfflinePageTabHelper::Redirect(
    const GURL& from_url, const GURL& to_url) {
  if (to_url.SchemeIsFile()) {
    UMA_HISTOGRAM_COUNTS("OfflinePages.RedirectToOfflineCount", 1);
  } else {
    UMA_HISTOGRAM_COUNTS("OfflinePages.RedirectToOnlineCount", 1);
  }

  content::NavigationController::LoadURLParams load_params(to_url);
  load_params.transition_type = ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  load_params.redirect_chain.push_back(from_url);
  web_contents()->GetController().LoadURLWithParams(load_params);
}

}  // namespace offline_pages
