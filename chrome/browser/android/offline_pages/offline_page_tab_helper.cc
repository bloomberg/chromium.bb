// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/offline_pages/offline_page_request_job.h"
#include "chrome/browser/android/offline_pages/offline_page_utils.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(offline_pages::OfflinePageTabHelper);

namespace offline_pages {

OfflinePageTabHelper::LoadedOfflinePageInfo::LoadedOfflinePageInfo()
    : is_showing_offline_preview(false) {}

OfflinePageTabHelper::LoadedOfflinePageInfo::~LoadedOfflinePageInfo() {}

void OfflinePageTabHelper::LoadedOfflinePageInfo::Clear() {
  offline_page.reset();
  offline_header.Clear();
  is_showing_offline_preview = false;
}

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
  reloading_url_on_net_error_ = false;

  // The provisional offline info can be cleared no matter how.
  provisional_offline_info_.Clear();

  // If not a fragment navigation, clear the cached offline info.
  if (offline_info_.offline_page.get() && !navigation_handle->IsSamePage()) {
    offline_info_.Clear();
  }
}

void OfflinePageTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Skips non-main frame.
  if (!navigation_handle->IsInMainFrame())
    return;

  if (!navigation_handle->HasCommitted())
    return;

  if (navigation_handle->IsSamePage())
    return;

  GURL navigated_url = navigation_handle->GetURL();
  if (navigation_handle->IsErrorPage()) {
    offline_info_.Clear();
  } else {
    // The provisional offline info can now be committed if the navigation is
    // done without error.
    DCHECK(!provisional_offline_info_.offline_page ||
           OfflinePageUtils::EqualsIgnoringFragment(
               navigated_url,
               provisional_offline_info_.offline_page->url));
    offline_info_.offline_page =
        std::move(provisional_offline_info_.offline_page);
    offline_info_.offline_header = provisional_offline_info_.offline_header;
    offline_info_.is_showing_offline_preview =
        provisional_offline_info_.is_showing_offline_preview;
  }
  provisional_offline_info_.Clear();

  // If the offline page has been loaded successfully, nothing more to do.
  net::Error error_code = navigation_handle->GetNetErrorCode();
  if (error_code == net::OK)
    return;

  // We might be reloading the URL in order to fetch the offline page.
  // * If successful, nothing to do.
  // * Otherwise, we're hitting error again. Bail out to avoid loop.
  if (reloading_url_on_net_error_)
    return;

  // When the navigation starts, the request might be intercepted to serve the
  // offline content if the network is detected to be in disconnected or poor
  // conditions. This detection might not work for some cases, i.e., connected
  // to a hotspot or proxy that does not have network, and the navigation will
  // eventually fail. To handle this, we will reload the page to force the
  // offline interception if the error code matches the following list.
  // Otherwise, the error page will be shown.
  if (error_code != net::ERR_INTERNET_DISCONNECTED &&
      error_code != net::ERR_NAME_NOT_RESOLVED &&
      error_code != net::ERR_ADDRESS_UNREACHABLE &&
      error_code != net::ERR_PROXY_CONNECTION_FAILED) {
    // Do not report aborted error since the error page is not shown on this
    // error.
    if (error_code != net::ERR_ABORTED) {
      OfflinePageRequestJob::ReportAggregatedRequestResult(
          OfflinePageRequestJob::AggregatedRequestResult::SHOW_NET_ERROR_PAGE);
    }
    return;
  }

  // When there is no valid tab android there is nowhere to show the offline
  // page, so we can leave.
  int tab_id;
  if (!OfflinePageUtils::GetTabId(web_contents(), &tab_id)) {
    // No need to report NO_TAB_ID since it should have already been detected
    // and reported in offline page request handler.
    return;
  }

  OfflinePageUtils::SelectPageForURL(
      web_contents()->GetBrowserContext(),
      navigated_url,
      OfflinePageModel::URLSearchMode::SEARCH_BY_ALL_URLS,
      tab_id,
      base::Bind(&OfflinePageTabHelper::SelectPageForURLDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageTabHelper::SelectPageForURLDone(
    const OfflinePageItem* offline_page) {
  // Bails out if no offline page is found.
  if (!offline_page) {
    OfflinePageRequestJob::ReportAggregatedRequestResult(
        OfflinePageRequestJob::AggregatedRequestResult::
            PAGE_NOT_FOUND_ON_FLAKY_NETWORK);
    return;
  }

  reloading_url_on_net_error_ = true;

  // Reloads the page with extra header set to force loading the offline page.
  content::NavigationController::LoadURLParams load_params(offline_page->url);
  load_params.transition_type = ui::PAGE_TRANSITION_RELOAD;
  OfflinePageHeader offline_header;
  offline_header.reason = OfflinePageHeader::Reason::NET_ERROR;
  load_params.extra_headers = offline_header.GetCompleteHeaderString();
  web_contents()->GetController().LoadURLWithParams(load_params);
}

// This is a callback from network request interceptor. It happens between
// DidStartNavigation and DidFinishNavigation calls on this tab helper.
void OfflinePageTabHelper::SetOfflinePage(
    const OfflinePageItem& offline_page,
    const OfflinePageHeader& offline_header,
    bool is_offline_preview) {
  provisional_offline_info_.offline_page =
      base::MakeUnique<OfflinePageItem>(offline_page);
  provisional_offline_info_.offline_header = offline_header;
  provisional_offline_info_.is_showing_offline_preview = is_offline_preview;
}

const OfflinePageItem* OfflinePageTabHelper::GetOfflinePageForTest() const {
  return provisional_offline_info_.offline_page.get();
}

bool OfflinePageTabHelper::IsShowingOfflinePreview() const {
  // TODO(ryansturm): Change this once offline pages infrastructure uses
  // NavigationHandle instead of a back channel. crbug.com/658899
  return provisional_offline_info_.is_showing_offline_preview ||
         offline_info_.is_showing_offline_preview;
}

}  // namespace offline_pages
