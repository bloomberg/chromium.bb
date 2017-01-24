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
#include "content/public/browser/web_contents.h"

namespace offline_pages {

BackgroundLoaderOffliner::BackgroundLoaderOffliner(
    content::BrowserContext* browser_context,
    const OfflinerPolicy* policy,
    OfflinePageModel* offline_page_model)
    : browser_context_(browser_context),
      offline_page_model_(offline_page_model),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()),
      save_state_(NONE),
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

void BackgroundLoaderOffliner::DidStopLoading() {
  if (!pending_request_.get()) {
    DVLOG(1) << "DidStopLoading called even though no pending request.";
    return;
  }

  save_state_ = SAVING;
  SavePageRequest request(*pending_request_.get());
  content::WebContents* web_contents(
      content::WebContentsObserver::web_contents());

  std::unique_ptr<OfflinePageArchiver> archiver(
      new OfflinePageMHTMLArchiver(web_contents));

  OfflinePageModel::SavePageParams params;
  params.url = web_contents->GetLastCommittedURL();
  params.client_id = request.client_id();
  params.proposed_offline_id = request.request_id();
  params.is_background = true;
  offline_page_model_->SavePage(
      params, std::move(archiver),
      base::Bind(&BackgroundLoaderOffliner::OnPageSaved,
                 weak_ptr_factory_.GetWeakPtr()));
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
    completion_callback_.Run(*pending_request_.get(),
                             Offliner::RequestStatus::LOADING_FAILED);
    ResetState();
  }
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
