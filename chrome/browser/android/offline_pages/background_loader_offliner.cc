// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/background_loader_offliner.h"

#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offliner_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/mhtml_extra_parts.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace offline_pages {

namespace {
const long kOfflinePageDelayMs = 2000;
const long kOfflineDomContentLoadedMs = 25000;
const char kContentType[] = "text/plain";
const char kContentTransferEncodingBinary[] =
    "Content-Transfer-Encoding: binary";
const char kXHeaderForSignals[] = "X-Chrome-Loading-Metrics-Data: 1";

class OfflinerData : public content::WebContentsUserData<OfflinerData> {
 public:
  static void AddToWebContents(content::WebContents* webcontents,
                               BackgroundLoaderOffliner* offliner) {
    DCHECK(offliner);
    webcontents->SetUserData(UserDataKey(), std::unique_ptr<OfflinerData>(
                                                new OfflinerData(offliner)));
  }

  explicit OfflinerData(BackgroundLoaderOffliner* offliner) {
    offliner_ = offliner;
  }
  BackgroundLoaderOffliner* offliner() { return offliner_; }

 private:
  // The offliner that the WebContents is attached to. The offliner owns the
  // Delegate which owns the WebContents that this data is attached to.
  // Therefore, its lifetime should exceed that of the WebContents, so this
  // should always be non-null.
  BackgroundLoaderOffliner* offliner_;
};

std::string AddHistogramSuffix(const ClientId& client_id,
                               const char* histogram_name) {
  if (client_id.name_space.empty()) {
    NOTREACHED();
    return histogram_name;
  }
  std::string adjusted_histogram_name(histogram_name);
  adjusted_histogram_name += "." + client_id.name_space;
  return adjusted_histogram_name;
}

void RecordErrorCauseUMA(const ClientId& client_id, net::Error error_code) {
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      AddHistogramSuffix(client_id,
                         "OfflinePages.Background.BackgroundLoadingFailedCode"),
      std::abs(error_code));
}
}  // namespace

BackgroundLoaderOffliner::BackgroundLoaderOffliner(
    content::BrowserContext* browser_context,
    const OfflinerPolicy* policy,
    OfflinePageModel* offline_page_model)
    : browser_context_(browser_context),
      offline_page_model_(offline_page_model),
      policy_(policy),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()),
      save_state_(NONE),
      page_load_state_(SUCCESS),
      page_delay_ms_(kOfflinePageDelayMs),
      network_bytes_(0LL),
      is_low_bar_met_(false),
      did_snapshot_on_last_retry_(false),
      weak_ptr_factory_(this) {
  DCHECK(offline_page_model_);
  DCHECK(browser_context_);
}

BackgroundLoaderOffliner::~BackgroundLoaderOffliner() {}

// static
BackgroundLoaderOffliner* BackgroundLoaderOffliner::FromWebContents(
    content::WebContents* contents) {
  OfflinerData* data = OfflinerData::FromWebContents(contents);
  if (data)
    return data->offliner();
  return nullptr;
}

bool BackgroundLoaderOffliner::LoadAndSave(
    const SavePageRequest& request,
    const CompletionCallback& completion_callback,
    const ProgressCallback& progress_callback) {
  DCHECK(completion_callback);
  DCHECK(progress_callback);

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

  MarkLoadStartTime();

  // Track copy of pending request.
  pending_request_.reset(new SavePageRequest(request));
  completion_callback_ = completion_callback;
  progress_callback_ = progress_callback;

  // Listen for app foreground/background change.
  app_listener_.reset(new base::android::ApplicationStatusListener(
      base::Bind(&BackgroundLoaderOffliner::OnApplicationStateChange,
                 weak_ptr_factory_.GetWeakPtr())));

  // Load page attempt.
  loader_.get()->LoadPage(request.url());

  snapshot_controller_.reset(
      new SnapshotController(base::ThreadTaskRunnerHandle::Get(), this,
                             kOfflineDomContentLoadedMs, page_delay_ms_));

  return true;
}

void BackgroundLoaderOffliner::Cancel(const CancelCallback& callback) {
  // TODO(chili): We are not able to cancel a pending
  // OfflinePageModel::SaveSnapshot() operation. We will notify caller that
  // cancel completed once the SavePage operation returns.
  if (!pending_request_) {
    callback.Run(0LL);
    return;
  }

  if (save_state_ != NONE) {
    save_state_ = DELETE_AFTER_SAVE;
    cancel_callback_ = callback;
    return;
  }

  int64_t request_id = pending_request_->request_id();
  ResetState();
  callback.Run(request_id);
}

bool BackgroundLoaderOffliner::HandleTimeout(const SavePageRequest& request) {
  if (pending_request_) {
    DCHECK(request.request_id() == pending_request_->request_id());
    if (is_low_bar_met_ &&
        (request.started_attempt_count() + 1 >= policy_->GetMaxStartedTries() ||
         request.completed_attempt_count() + 1 >=
             policy_->GetMaxCompletedTries())) {
      // If we are already in the middle of a save operation, let it finish
      // but do not return SAVED_ON_LAST_RETRY
      if (save_state_ == NONE) {
        did_snapshot_on_last_retry_ = true;
        StartSnapshot();
      }
      return true;
    }
  }
  return false;
}

void BackgroundLoaderOffliner::MarkLoadStartTime() {
  load_start_time_ = base::TimeTicks::Now();
}

void BackgroundLoaderOffliner::DocumentAvailableInMainFrame() {
  snapshot_controller_->DocumentAvailableInMainFrame();
  is_low_bar_met_ = true;

  // Add this signal to signal_data_.
  AddLoadingSignal("DocumentAvailableInMainFrame");
}

void BackgroundLoaderOffliner::DocumentOnLoadCompletedInMainFrame() {
  if (!pending_request_.get()) {
    DVLOG(1) << "DidStopLoading called even though no pending request.";
    return;
  }

  // Add this signal to signal_data_.
  AddLoadingSignal("DocumentOnLoadCompletedInMainFrame");

  snapshot_controller_->DocumentOnLoadCompletedInMainFrame();
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
  if (!navigation_handle->IsInMainFrame())
    return;
  // If there was an error of any kind (certificate, client, DNS, etc),
  // Mark as error page. Resetting here causes RecordNavigationMetrics to crash.
  if (navigation_handle->IsErrorPage()) {
    RecordErrorCauseUMA(pending_request_->client_id(),
                        navigation_handle->GetNetErrorCode());
    switch (navigation_handle->GetNetErrorCode()) {
      case net::ERR_INTERNET_DISCONNECTED:
        page_load_state_ = DELAY_RETRY;
        break;
      default:
        page_load_state_ = RETRIABLE;
    }
  }
}

void BackgroundLoaderOffliner::SetPageDelayForTest(long delay_ms) {
  page_delay_ms_ = delay_ms;
}

void BackgroundLoaderOffliner::OnNetworkBytesChanged(int64_t bytes) {
  if (pending_request_ && save_state_ != SAVING) {
    network_bytes_ += bytes;
    progress_callback_.Run(*pending_request_, network_bytes_);
  }
}

void BackgroundLoaderOffliner::StartSnapshot() {
  if (!pending_request_.get()) {
    DVLOG(1) << "Pending request was cleared during delay.";
    return;
  }

  // Add this signal to signal_data_.
  AddLoadingSignal("Snapshotting");

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

  // Add loading signal into the MHTML that will be generated if the command
  // line flag is set for it.
  if (IsOfflinePagesLoadSignalCollectingEnabled()) {
    // Stash loading signals for writing when we write out the MHTML.
    std::string headers = base::StringPrintf(
        "%s\r\n%s\r\n\r\n", kContentTransferEncodingBinary, kXHeaderForSignals);
    std::string body;
    base::JSONWriter::Write(signal_data_, &body);
    std::string content_type = kContentType;
    std::string content_location = base::StringPrintf(
        "cid:signal-data-%" PRId64 "@mhtml.blink", request.request_id());

    content::MHTMLExtraParts* extra_parts =
        content::MHTMLExtraParts::FromWebContents(web_contents);
    DCHECK(extra_parts);
    if (extra_parts != nullptr) {
      extra_parts->AddExtraMHTMLPart(content_type, content_location, headers,
                                     body);
    }
  }

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
  bool did_snapshot_on_last_retry = did_snapshot_on_last_retry_;
  ResetState();

  if (save_state_ == DELETE_AFTER_SAVE) {
    save_state_ = NONE;
    cancel_callback_.Run(request.request_id());
    return;
  }

  save_state_ = NONE;

  Offliner::RequestStatus save_status;
  if (save_result == SavePageResult::SUCCESS) {
    if (did_snapshot_on_last_retry)
      save_status = RequestStatus::SAVED_ON_LAST_RETRY;
    else
      save_status = RequestStatus::SAVED;
  } else {
    save_status = RequestStatus::SAVE_FAILED;
  }

  completion_callback_.Run(request, save_status);
}

void BackgroundLoaderOffliner::ResetState() {
  pending_request_.reset();
  snapshot_controller_.reset();
  page_load_state_ = SUCCESS;
  network_bytes_ = 0LL;
  is_low_bar_met_ = false;
  did_snapshot_on_last_retry_ = false;
  // TODO(chili): Remove after RequestCoordinator can handle multiple offliners.
  // We reset the loader and observer after completion so loaders
  // will not be re-used across different requests/tries. This is a temporary
  // solution while there exists assumptions about the number of offliners
  // there are.
  loader_.reset(
      new background_loader::BackgroundLoaderContents(browser_context_));
  content::WebContents* contents = loader_->web_contents();
  content::WebContentsObserver::Observe(contents);
  OfflinerData::AddToWebContents(contents, this);
}

void BackgroundLoaderOffliner::OnApplicationStateChange(
    base::android::ApplicationState application_state) {
  if (pending_request_ && is_low_end_device_ &&
      application_state ==
          base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
    DVLOG(1) << "App became active, canceling current offlining request";
    SavePageRequest* request = pending_request_.get();
    // This works because Bind will make a copy of request, and we
    // should not have to worry about reset being called before cancel callback.
    Cancel(base::Bind(
        &BackgroundLoaderOffliner::HandleApplicationStateChangeCancel,
        weak_ptr_factory_.GetWeakPtr(), *request));
  }
}

void BackgroundLoaderOffliner::HandleApplicationStateChangeCancel(
    const SavePageRequest& request,
    int64_t offline_id) {
  // If for some reason the request was reset during while waiting for callback
  // ignore the completion callback.
  if (pending_request_ && pending_request_->request_id() != offline_id)
    return;
  completion_callback_.Run(request, RequestStatus::FOREGROUND_CANCELED);
}

void BackgroundLoaderOffliner::AddLoadingSignal(const char* signal_name) {
  base::TimeTicks current_time = base::TimeTicks::Now();
  base::TimeDelta delay_so_far = current_time - load_start_time_;
  // We would prefer to use int64_t here, but JSON does not support that type.
  // Given the choice between int and double, we choose to implicitly convert to
  // a double since it maintains more precision (we can get a longer time in
  // milliseconds than we can with a 2 bit int, 53 bits vs 32).
  double delay = delay_so_far.InMilliseconds();
  signal_data_.SetDouble(signal_name, delay);
}

}  // namespace offline_pages

DEFINE_WEB_CONTENTS_USER_DATA_KEY(offline_pages::OfflinerData);
