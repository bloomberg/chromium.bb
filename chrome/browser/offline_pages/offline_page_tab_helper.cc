// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_tab_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_request_job.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/base/page_transition_types.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(offline_pages::OfflinePageTabHelper);

namespace offline_pages {

namespace {
bool SchemeIsForUntrustedOfflinePages(const GURL& url) {
#if defined(OS_ANDROID)
  if (url.SchemeIs(url::kContentScheme))
    return true;
#endif
  return url.SchemeIsFile();
}
}  // namespace

OfflinePageTabHelper::LoadedOfflinePageInfo::LoadedOfflinePageInfo()
    : trusted_state(OfflinePageTrustedState::UNTRUSTED),
      is_showing_offline_preview(false) {}

OfflinePageTabHelper::LoadedOfflinePageInfo::LoadedOfflinePageInfo(
    OfflinePageTabHelper::LoadedOfflinePageInfo&& other) = default;

OfflinePageTabHelper::LoadedOfflinePageInfo::~LoadedOfflinePageInfo() = default;

OfflinePageTabHelper::LoadedOfflinePageInfo&
OfflinePageTabHelper::LoadedOfflinePageInfo::operator=(
    OfflinePageTabHelper::LoadedOfflinePageInfo&& other) = default;

// static
OfflinePageTabHelper::LoadedOfflinePageInfo
OfflinePageTabHelper::LoadedOfflinePageInfo::MakeUntrusted() {
  LoadedOfflinePageInfo untrusted_info;
  untrusted_info.offline_page = std::make_unique<OfflinePageItem>();
  untrusted_info.offline_page->offline_id = store_utils::GenerateOfflineId();

  return untrusted_info;
}

void OfflinePageTabHelper::LoadedOfflinePageInfo::Clear() {
  offline_page.reset();
  offline_header.Clear();
  trusted_state = OfflinePageTrustedState::UNTRUSTED;
  is_showing_offline_preview = false;
}

bool OfflinePageTabHelper::LoadedOfflinePageInfo::IsValid() const {
  return offline_page != nullptr;
}

OfflinePageTabHelper::OfflinePageTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      mhtml_page_notifier_bindings_(web_contents, this),
      weak_ptr_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  prefetch_service_ = PrefetchServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
}

OfflinePageTabHelper::~OfflinePageTabHelper() {}

void OfflinePageTabHelper::NotifyIsMhtmlPage(const GURL& main_frame_url,
                                             base::Time creation_time) {
  if (mhtml_page_notifier_bindings_.GetCurrentTargetFrame() !=
      web_contents()->GetMainFrame()) {
    return;
  }

  // Sanity checking the input URL.
  if (!main_frame_url.is_valid() || !main_frame_url.SchemeIsHTTPOrHTTPS())
    return;

  // The renderer's info should only be used if the offline page is not trusted,
  // so ignore this information if we have a trusted page already.
  if (provisional_offline_info_.IsValid() &&
      provisional_offline_info_.trusted_state !=
          OfflinePageTrustedState::UNTRUSTED) {
    return;
  }

  if (!provisional_offline_info_.IsValid())
    provisional_offline_info_ = LoadedOfflinePageInfo::MakeUntrusted();
  provisional_offline_info_.offline_page->url = main_frame_url;

  if (!creation_time.is_null())
    provisional_offline_info_.offline_page->creation_time = creation_time;
}

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
  if (offline_info_.offline_page.get() &&
      !navigation_handle->IsSameDocument()) {
    offline_info_.Clear();
  }

  // Report any attempted navigation as indication that browser is in use.
  // This doesn't have to be a successful navigation.
  if (prefetch_service_) {
    prefetch_service_->GetOfflineMetricsCollector()->OnAppStartupOrResume();
  }
}

void OfflinePageTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Skips non-main frame.
  if (!navigation_handle->IsInMainFrame())
    return;

  if (!navigation_handle->HasCommitted())
    return;

  if (navigation_handle->IsSameDocument())
    return;

  FinalizeOfflineInfo(navigation_handle);
  provisional_offline_info_.Clear();

  ReportOfflinePageMetrics();
  ReportPrefetchMetrics(navigation_handle);

  TryLoadingOfflinePageOnNetError(navigation_handle);
}

void OfflinePageTabHelper::FinalizeOfflineInfo(
    content::NavigationHandle* navigation_handle) {
  offline_info_.Clear();

  if (navigation_handle->IsErrorPage())
    return;

  GURL navigated_url = navigation_handle->GetURL();

  content::WebContents* web_contents = navigation_handle->GetWebContents();
  if (web_contents->GetContentsMimeType() != "multipart/related" &&
      web_contents->GetContentsMimeType() != "message/rfc822") {
    return;
  }

  if (SchemeIsForUntrustedOfflinePages(navigated_url)) {
    // If a MHTML archive is being loaded for file: or content: URL, and we did
    // get a message from the renderer describing the contents, the results of
    // that message will be stored in |provisional_offline_info_|.
    if (provisional_offline_info_.IsValid()) {
      offline_info_ = std::move(provisional_offline_info_);
      provisional_offline_info_.Clear();
    } else {
      // Otherwise, just use an empty untrusted page.
      offline_info_ = LoadedOfflinePageInfo::MakeUntrusted();
      offline_info_.offline_page->url = navigated_url;
    }
  } else if (navigated_url.SchemeIsHTTPOrHTTPS()) {
    // For http/https URL, commit the provisional offline info if any.
    if (provisional_offline_info_.IsValid()) {
      DCHECK(OfflinePageUtils::EqualsIgnoringFragment(
          navigated_url, provisional_offline_info_.offline_page->url));
      offline_info_ = std::move(provisional_offline_info_);
      provisional_offline_info_.Clear();
    }
  }
}

void OfflinePageTabHelper::ReportOfflinePageMetrics() {
  if (!offline_page())
    return;
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.TrustStateOnOpen",
                            offline_info_.trusted_state,
                            OfflinePageTrustedState::TRUSTED_STATE_MAX);
}

void OfflinePageTabHelper::ReportPrefetchMetrics(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsErrorPage())
    return;

  if (!prefetch_service_)
    return;

  // Report the kind of navigation (online/offline) to metrics collector.
  // It accumulates this info to mark a day as 'offline' or 'online'.
  OfflineMetricsCollector* metrics_collector =
      prefetch_service_->GetOfflineMetricsCollector();
  DCHECK(metrics_collector);

  if (offline_page()) {
    // Report prefetch usage.
    if (policy_controller_.IsSuggested(offline_page()->client_id.name_space))
      metrics_collector->OnPrefetchedPageOpened();
    // Note that navigation to offline page may happen even if network is
    // connected. For the purposes of collecting offline usage statistics,
    // we still count this as offline navigation.
    metrics_collector->OnSuccessfulNavigationOffline();
  } else {
    metrics_collector->OnSuccessfulNavigationOnline();
    // The device is apparently online, attempt to report stats to UMA.
    metrics_collector->ReportAccumulatedStats();
  }
}

void OfflinePageTabHelper::TryLoadingOfflinePageOnNetError(
    content::NavigationHandle* navigation_handle) {
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

  OfflinePageUtils::SelectPagesForURL(
      web_contents()->GetBrowserContext(), navigation_handle->GetURL(),
      URLSearchMode::SEARCH_BY_ALL_URLS, tab_id,
      base::Bind(&OfflinePageTabHelper::SelectPagesForURLDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageTabHelper::SelectPagesForURLDone(
    const std::vector<OfflinePageItem>& offline_pages) {
  // Bails out if no offline page is found.
  if (offline_pages.empty()) {
    OfflinePageRequestJob::ReportAggregatedRequestResult(
        OfflinePageRequestJob::AggregatedRequestResult::
            PAGE_NOT_FOUND_ON_FLAKY_NETWORK);
    return;
  }

  reloading_url_on_net_error_ = true;

  // Reloads the page with extra header set to force loading the offline page.
  content::NavigationController::LoadURLParams load_params(
      offline_pages.front().url);
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
    OfflinePageTrustedState trusted_state,
    bool is_offline_preview) {
  provisional_offline_info_.offline_page =
      std::make_unique<OfflinePageItem>(offline_page);
  provisional_offline_info_.offline_header = offline_header;
  provisional_offline_info_.trusted_state = trusted_state;
  provisional_offline_info_.is_showing_offline_preview = is_offline_preview;
}

void OfflinePageTabHelper::ClearOfflinePage() {
  provisional_offline_info_.Clear();
  offline_info_.Clear();
}

bool OfflinePageTabHelper::IsShowingTrustedOfflinePage() const {
  return offline_info_.offline_page &&
         (offline_info_.trusted_state != OfflinePageTrustedState::UNTRUSTED);
}

const OfflinePageItem* OfflinePageTabHelper::GetOfflinePageForTest() const {
  return provisional_offline_info_.offline_page.get();
}

OfflinePageTrustedState OfflinePageTabHelper::GetTrustedStateForTest() const {
  return provisional_offline_info_.trusted_state;
}

void OfflinePageTabHelper::SetCurrentTargetFrameForTest(
    content::RenderFrameHost* render_frame_host) {
  mhtml_page_notifier_bindings_.SetCurrentTargetFrameForTesting(
      render_frame_host);
}

const OfflinePageItem* OfflinePageTabHelper::GetOfflinePreviewItem() const {
  if (provisional_offline_info_.is_showing_offline_preview)
    return provisional_offline_info_.offline_page.get();
  if (offline_info_.is_showing_offline_preview)
    return offline_info_.offline_page.get();
  return nullptr;
}

void OfflinePageTabHelper::ScheduleDownloadHelper(
    content::WebContents* web_contents,
    const std::string& name_space,
    const GURL& url,
    OfflinePageUtils::DownloadUIActionFlags ui_action,
    const std::string& request_origin) {
  OfflinePageUtils::CheckDuplicateDownloads(
      web_contents->GetBrowserContext(), url,
      base::Bind(&OfflinePageTabHelper::DuplicateCheckDoneForScheduleDownload,
                 weak_ptr_factory_.GetWeakPtr(), web_contents, name_space, url,
                 ui_action, request_origin));
}

void OfflinePageTabHelper::DuplicateCheckDoneForScheduleDownload(
    content::WebContents* web_contents,
    const std::string& name_space,
    const GURL& url,
    OfflinePageUtils::DownloadUIActionFlags ui_action,
    const std::string& request_origin,
    OfflinePageUtils::DuplicateCheckResult result) {
  if (result != OfflinePageUtils::DuplicateCheckResult::NOT_FOUND) {
    if (static_cast<int>(ui_action) &
        static_cast<int>(
            OfflinePageUtils::DownloadUIActionFlags::PROMPT_DUPLICATE)) {
      OfflinePageUtils::ShowDuplicatePrompt(
          base::Bind(&OfflinePageTabHelper::DoDownloadPageLater,
                     weak_ptr_factory_.GetWeakPtr(), web_contents, name_space,
                     url, ui_action, request_origin),
          url,
          result ==
              OfflinePageUtils::DuplicateCheckResult::DUPLICATE_REQUEST_FOUND,
          web_contents);
      return;
    }
  }

  DoDownloadPageLater(web_contents, name_space, url, ui_action, request_origin);
}

void OfflinePageTabHelper::DoDownloadPageLater(
    content::WebContents* web_contents,
    const std::string& name_space,
    const GURL& url,
    OfflinePageUtils::DownloadUIActionFlags ui_action,
    const std::string& request_origin) {
  offline_pages::RequestCoordinator* request_coordinator =
      offline_pages::RequestCoordinatorFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (!request_coordinator)
    return;

  offline_pages::RequestCoordinator::SavePageLaterParams params;
  params.url = url;
  params.client_id = offline_pages::ClientId(name_space, base::GenerateGUID());
  params.request_origin = request_origin;
  request_coordinator->SavePageLater(params, base::DoNothing());

  if (static_cast<int>(ui_action) &
      static_cast<int>(OfflinePageUtils::DownloadUIActionFlags::
                           SHOW_TOAST_ON_NEW_DOWNLOAD)) {
    OfflinePageUtils::ShowDownloadingToast();
  }
}

}  // namespace offline_pages
