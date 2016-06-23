// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_utils.h"
#include "components/offline_pages/offline_page_model.h"
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

  // Since this is a new navigation, we will reset the cached offline page,
  // unless we are currently looking at an offline page.
  GURL navigated_url = navigation_handle->GetURL();
  if (offline_page_ && navigated_url != offline_page_->GetOfflineURL())
    offline_page_ = nullptr;

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
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(context);
  if (!offline_page_model)
    return;

  if (net::NetworkChangeNotifier::IsOffline()) {
    offline_page_model->GetBestPageForOnlineURL(
        navigated_url,
        base::Bind(&OfflinePageTabHelper::TryRedirectToOffline,
                   weak_ptr_factory_.GetWeakPtr(),
                   RedirectReason::DISCONNECTED_NETWORK, navigated_url));
  } else {
    offline_page_model->GetPageByOfflineURL(
        navigated_url,
        base::Bind(&OfflinePageTabHelper::RedirectToOnline,
                   weak_ptr_factory_.GetWeakPtr(), navigated_url));
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

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context);
  if (!offline_page_model)
    return;

  // Otherwise, get the offline URL for this url, and attempt a redirect if
  // necessary.
  RedirectReason reason =
      ui::PageTransitionTypeIncludingQualifiersIs(
          navigation_handle->GetPageTransition(),
          ui::PAGE_TRANSITION_FORWARD_BACK)
          ? RedirectReason::FLAKY_NETWORK_FORWARD_BACK
          : RedirectReason::FLAKY_NETWORK;
  offline_page_model->GetBestPageForOnlineURL(
      navigated_url,
      base::Bind(&OfflinePageTabHelper::TryRedirectToOffline,
                 weak_ptr_factory_.GetWeakPtr(), reason, navigated_url));
}

void OfflinePageTabHelper::RedirectToOnline(
    const GURL& navigated_url,
    const OfflinePageItem* offline_page) {
  // Bails out if no redirection is needed.
  if (!offline_page)
    return;

  GURL redirect_url = offline_page->url;

  const content::NavigationController& controller =
      web_contents()->GetController();

  // Avoids looping between online and offline redirections.
  content::NavigationEntry* entry = controller.GetPendingEntry();
  if (entry && !entry->GetRedirectChain().empty() &&
      entry->GetRedirectChain().back() == redirect_url) {
    return;
  }

  Redirect(navigated_url, redirect_url);
  // Clear the offline page since we are redirecting to online.
  offline_page_ = nullptr;

  UMA_HISTOGRAM_COUNTS("OfflinePages.RedirectToOnlineCount", 1);
}

void OfflinePageTabHelper::TryRedirectToOffline(
    RedirectReason redirect_reason,
    const GURL& from_url,
    const OfflinePageItem* offline_page) {
  if (!offline_page)
    return;

  GURL redirect_url = offline_page->GetOfflineURL();

  if (!redirect_url.is_valid())
    return;

  if (redirect_reason == RedirectReason::FLAKY_NETWORK ||
      redirect_reason == RedirectReason::FLAKY_NETWORK_FORWARD_BACK) {
    UMA_HISTOGRAM_BOOLEAN("OfflinePages.ShowOfflinePageOnBadNetwork",
                          redirect_reason == RedirectReason::FLAKY_NETWORK);
    // Don't actually want to redirect on a forward/back nav.
    if (redirect_reason == RedirectReason::FLAKY_NETWORK_FORWARD_BACK)
      return;
  } else {
    const content::NavigationController& controller =
        web_contents()->GetController();

    // Avoids looping between online and offline redirections.
    content::NavigationEntry* entry = controller.GetPendingEntry();
    if (entry && !entry->GetRedirectChain().empty() &&
        entry->GetRedirectChain().back() == redirect_url) {
      return;
    }
  }

  Redirect(from_url, redirect_url);
  offline_page_ = base::MakeUnique<OfflinePageItem>(*offline_page);
  UMA_HISTOGRAM_COUNTS("OfflinePages.RedirectToOfflineCount", 1);
}

void OfflinePageTabHelper::Redirect(const GURL& from_url, const GURL& to_url) {
  content::NavigationController::LoadURLParams load_params(to_url);
  load_params.transition_type = ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  load_params.redirect_chain.push_back(from_url);
  web_contents()->GetController().LoadURLWithParams(load_params);
}

}  // namespace offline_pages
