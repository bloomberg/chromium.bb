// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/background_loader_offliner.h"

#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offliner_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace offline_pages {

namespace {
long kOfflinePageDelayMs = 2000;
}  // namespace

BackgroundLoaderOffliner::BackgroundLoaderOffliner(
    content::BrowserContext* browser_context,
    const OfflinerPolicy* policy,
    OfflinePageModel* offline_page_model)
    : browser_context_(browser_context),
      offline_page_model_(offline_page_model),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()),
      save_state_(NONE),
      page_load_state_(SUCCESS),
      page_delay_ms_(kOfflinePageDelayMs),
      weak_ptr_factory_(this) {
  DCHECK(offline_page_model_);
  DCHECK(browser_context_);
}

BackgroundLoaderOffliner::~BackgroundLoaderOffliner() {}

bool BackgroundLoaderOffliner::LoadAndSave(const SavePageRequest& request,
                                           const CompletionCallback& callback) {
  DCHECK(callback);

  if (pending_request_) {
    DVLOG(1) << "Already have pending request";
    return false;
  }

  // Do not allow loading for custom tabs clients if 3rd party cookies blocked.
  // TODO(dewittj): Revise api to specify policy rather than hard code to
  // name_space.
  if (request.client_id().name_space == kCCTNamespace &&
      (AreThirdPartyCookiesBlocked(browser_context_) ||
       IsNetworkPredictionDisabled(browser_context_))) {
    DVLOG(1) << "WARNING: Unable to load when 3rd party cookies blocked or "
             << "prediction disabled";
    // Record user metrics for third party cookies being disabled or network
    // prediction being disabled.
    if (AreThirdPartyCookiesBlocked(browser_context_)) {
      UMA_HISTOGRAM_ENUMERATION(
          "OfflinePages.Background.CctApiDisableStatus",
          static_cast<int>(OfflinePagesCctApiPrerenderAllowedStatus::
                               THIRD_PARTY_COOKIES_DISABLED),
          static_cast<int>(OfflinePagesCctApiPrerenderAllowedStatus::
                               NETWORK_PREDICTION_DISABLED) +
              1);
    }
    if (IsNetworkPredictionDisabled(browser_context_)) {
      UMA_HISTOGRAM_ENUMERATION(
          "OfflinePages.Background.CctApiDisableStatus",
          static_cast<int>(OfflinePagesCctApiPrerenderAllowedStatus::
                               NETWORK_PREDICTION_DISABLED),
          static_cast<int>(OfflinePagesCctApiPrerenderAllowedStatus::
                               NETWORK_PREDICTION_DISABLED) +
              1);
    }

    return false;
  }

  // Record UMA that the load was allowed to proceed.
  if (request.client_id().name_space == kCCTNamespace) {
    UMA_HISTOGRAM_ENUMERATION(
        "OfflinePages.Background.CctApiDisableStatus",
        static_cast<int>(
            OfflinePagesCctApiPrerenderAllowedStatus::PRERENDER_ALLOWED),
        static_cast<int>(OfflinePagesCctApiPrerenderAllowedStatus::
                             NETWORK_PREDICTION_DISABLED) +
            1);
  }

  if (!OfflinePageModel::CanSaveURL(request.url())) {
    DVLOG(1) << "Not able to save page for requested url: " << request.url();
    return false;
  }

  if (!loader_)
    ResetState();

  // Invalidate ptrs for all delayed/saving tasks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Track copy of pending request.
  pending_request_.reset(new SavePageRequest(request));
  completion_callback_ = callback;

  // Listen for app foreground/background change.
  app_listener_.reset(new base::android::ApplicationStatusListener(
      base::Bind(&BackgroundLoaderOffliner::OnApplicationStateChange,
                 weak_ptr_factory_.GetWeakPtr())));

  // Load page attempt.
  loader_.get()->LoadPage(request.url());

  return true;
}

void BackgroundLoaderOffliner::Cancel() {
  // TODO(chili): We are not able to cancel a pending
  // OfflinePageModel::SavePage() operation. We just ignore the callback.
  if (!pending_request_)
    return;

  if (save_state_ != NONE) {
    save_state_ = DELETE_AFTER_SAVE;
    return;
  }

  ResetState();
}

bool BackgroundLoaderOffliner::HandleTimeout(const SavePageRequest& request) {
  // TODO(romax) Decide if we want to also take a snapshot on the last timeout
  // for the background loader offliner.
  return false;
}

void BackgroundLoaderOffliner::DidStopLoading() {
  if (!pending_request_.get()) {
    DVLOG(1) << "DidStopLoading called even though no pending request.";
    return;
  }

  // Invalidate ptrs for any ongoing save operation.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Post SavePage task with 2 second delay.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BackgroundLoaderOffliner::SavePage,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(page_delay_ms_));
}

void BackgroundLoaderOffliner::RenderProcessGone(
    base::TerminationStatus status) {
  if (pending_request_) {
    SavePageRequest request(*pending_request_.get());
    switch (status) {
      case base::TERMINATION_STATUS_OOM:
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
      case base::TERMINATION_STATUS_STILL_RUNNING:
        completion_callback_.Run(
            request, Offliner::RequestStatus::LOADING_FAILED_NO_NEXT);
        break;
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      default:
        completion_callback_.Run(request,
                                 Offliner::RequestStatus::LOADING_FAILED);
    }
    ResetState();
  }
}

void BackgroundLoaderOffliner::WebContentsDestroyed() {
  if (pending_request_) {
    SavePageRequest request(*pending_request_.get());
    completion_callback_.Run(request, Offliner::RequestStatus::LOADING_FAILED);
    ResetState();
  }
}

void BackgroundLoaderOffliner::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // If there was an error of any kind (certificate, client, DNS, etc),
  // Mark as error page. Resetting here causes RecordNavigationMetrics to crash.
  if (navigation_handle->IsErrorPage()) {
    // TODO(chili): we need to UMA this.
    switch (navigation_handle->GetNetErrorCode()) {
      case net::ERR_ACCESS_DENIED:
      case net::ERR_ADDRESS_INVALID:
      case net::ERR_ADDRESS_UNREACHABLE:
      case net::ERR_CERT_COMMON_NAME_INVALID:
      case net::ERR_CERT_AUTHORITY_INVALID:
      case net::ERR_CERT_CONTAINS_ERRORS:
      case net::ERR_CERT_INVALID:
      case net::ERR_CONNECTION_FAILED:
      case net::ERR_DISALLOWED_URL_SCHEME:
      case net::ERR_DNS_SERVER_FAILED:
      case net::ERR_FILE_NOT_FOUND:
      case net::ERR_FILE_PATH_TOO_LONG:
      case net::ERR_FILE_TOO_BIG:
      case net::ERR_FILE_VIRUS_INFECTED:
      case net::ERR_INVALID_HANDLE:
      case net::ERR_INVALID_RESPONSE:
      case net::ERR_INVALID_URL:
      case net::ERR_MSG_TOO_BIG:
      case net::ERR_NAME_NOT_RESOLVED:
      case net::ERR_NAME_RESOLUTION_FAILED:
      case net::ERR_SSL_PROTOCOL_ERROR:
      case net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED:
      case net::ERR_SSL_SERVER_CERT_BAD_FORMAT:
      case net::ERR_UNKNOWN_URL_SCHEME:
        page_load_state_ = NONRETRIABLE;
        break;
      case net::ERR_INTERNET_DISCONNECTED:
        page_load_state_ = DELAY_RETRY;
        break;
      default:
        page_load_state_ = RETRIABLE;
    }
  }

  // If the page is not the same, invalidate any pending save tasks.
  //
  // Downloads or 204/205 response codes do not commit (no new navigation)
  // Same-Page (committed) navigations are:
  // - reference fragment navigations
  // - pushState/replaceState
  // - same page history navigation
  if (navigation_handle->HasCommitted() && !navigation_handle->IsSamePage())
    weak_ptr_factory_.InvalidateWeakPtrs();
}

void BackgroundLoaderOffliner::SetPageDelayForTest(long delay_ms) {
  page_delay_ms_ = delay_ms;
}

void BackgroundLoaderOffliner::SavePage() {
  if (!pending_request_.get()) {
    DVLOG(1) << "Pending request was cleared during delay.";
    return;
  }

  SavePageRequest request(*pending_request_.get());
  // If there was an error navigating to page, return loading failed.
  if (page_load_state_ != SUCCESS) {
    Offliner::RequestStatus status;
    switch (page_load_state_) {
      case RETRIABLE:
        status = Offliner::RequestStatus::LOADING_FAILED;
        break;
      case NONRETRIABLE:
        status = Offliner::RequestStatus::LOADING_FAILED_NO_RETRY;
        break;
      case DELAY_RETRY:
        status = Offliner::RequestStatus::LOADING_FAILED_NO_NEXT;
        break;
      default:
        // We should've already checked for Success before entering here.
        NOTREACHED();
        status = Offliner::RequestStatus::LOADING_FAILED;
    }
    completion_callback_.Run(request, status);
    ResetState();
    return;
  }

  save_state_ = SAVING;
  content::WebContents* web_contents(
      content::WebContentsObserver::web_contents());

  std::unique_ptr<OfflinePageArchiver> archiver(
      new OfflinePageMHTMLArchiver(web_contents));

  OfflinePageModel::SavePageParams params;
  params.url = web_contents->GetLastCommittedURL();
  params.client_id = request.client_id();
  params.proposed_offline_id = request.request_id();
  params.is_background = true;

  // Pass in the original URL if it's different from last committed
  // when redirects occur.
  if (!request.original_url().is_empty())
    params.original_url = request.original_url();
  else if (params.url != request.url())
    params.original_url = request.url();

  offline_page_model_->SavePage(
      params, std::move(archiver),
      base::Bind(&BackgroundLoaderOffliner::OnPageSaved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundLoaderOffliner::OnPageSaved(SavePageResult save_result,
                                           int64_t offline_id) {
  if (!pending_request_)
    return;

  SavePageRequest request(*pending_request_.get());
  ResetState();

  if (save_state_ == DELETE_AFTER_SAVE) {
    save_state_ = NONE;
    return;
  }

  save_state_ = NONE;

  Offliner::RequestStatus save_status;
  if (save_result == SavePageResult::SUCCESS)
    save_status = RequestStatus::SAVED;
  else
    save_status = RequestStatus::SAVE_FAILED;

  completion_callback_.Run(request, save_status);
}

void BackgroundLoaderOffliner::ResetState() {
  pending_request_.reset();
  page_load_state_ = SUCCESS;
  // TODO(chili): Remove after RequestCoordinator can handle multiple offliners.
  // We reset the loader and observer after completion so loaders
  // will not be re-used across different requests/tries. This is a temporary
  // solution while there exists assumptions about the number of offliners
  // there are.
  loader_.reset(
      new background_loader::BackgroundLoaderContents(browser_context_));
  content::WebContentsObserver::Observe(loader_.get()->web_contents());
}

void BackgroundLoaderOffliner::OnApplicationStateChange(
    base::android::ApplicationState application_state) {
  if (pending_request_ && is_low_end_device_ &&
      application_state ==
          base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
    DVLOG(1) << "App became active, canceling current offlining request";
    SavePageRequest* request = pending_request_.get();
    Cancel();
    completion_callback_.Run(*request, RequestStatus::FOREGROUND_CANCELED);
  }
}

}  // namespace offline_pages
