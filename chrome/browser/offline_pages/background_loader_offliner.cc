// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/background_loader_offliner.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/offline_pages/offliner_helper.h"
#include "chrome/browser/offline_pages/offliner_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/offline_pages/content/renovations/render_frame_script_injector.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/renovations/page_renovation_loader.h"
#include "components/offline_pages/core/renovations/page_renovator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/mhtml_extra_parts.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/previews_state.h"
#include "net/http/http_response_headers.h"

namespace offline_pages {

namespace {
const char kContentType[] = "text/plain";
const char kContentTransferEncodingBinary[] =
    "Content-Transfer-Encoding: binary";
const char kXHeaderForSignals[] = "X-Chrome-Loading-Metrics-Data: 1";

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

void RecordErrorCauseUMA(const ClientId& client_id, int error_code) {
  base::UmaHistogramSparse(
      AddHistogramSuffix(client_id,
                         "OfflinePages.Background.LoadingErrorStatusCode"),
      error_code);
}

void RecordOffliningPreviewsUMA(const ClientId& client_id,
                                ChromeNavigationData* navigation_data) {
  content::PreviewsState previews_state = content::PreviewsTypes::PREVIEWS_OFF;
  if (navigation_data)
    previews_state = navigation_data->previews_state();

  int is_previews_enabled = 0;
  bool lite_page_received = false;
  data_reduction_proxy::DataReductionProxyData* data_reduction_proxy_data =
      nullptr;
  if (navigation_data)
    data_reduction_proxy_data = navigation_data->GetDataReductionProxyData();
  if (data_reduction_proxy_data)
    lite_page_received = data_reduction_proxy_data->lite_page_received();

  if ((previews_state != content::PreviewsTypes::PREVIEWS_OFF &&
       previews_state != content::PreviewsTypes::PREVIEWS_NO_TRANSFORM) ||
      lite_page_received)
    is_previews_enabled = 1;

  base::UmaHistogramBoolean(
      AddHistogramSuffix(client_id,
                         "OfflinePages.Background.OffliningPreviewStatus"),
      is_previews_enabled);
}

void HandleLoadTerminationCancel(
    const Offliner::CompletionCallback& completion_callback,
    const SavePageRequest& canceled_request) {
  completion_callback.Run(canceled_request,
                          Offliner::RequestStatus::FOREGROUND_CANCELED);
}

}  // namespace

BackgroundLoaderOffliner::BackgroundLoaderOffliner(
    content::BrowserContext* browser_context,
    const OfflinerPolicy* policy,
    OfflinePageModel* offline_page_model,
    std::unique_ptr<LoadTerminationListener> load_termination_listener)
    : browser_context_(browser_context),
      offline_page_model_(offline_page_model),
      policy_(policy),
      load_termination_listener_(std::move(load_termination_listener)),
      save_state_(NONE),
      page_load_state_(SUCCESS),
      network_bytes_(0LL),
      is_low_bar_met_(false),
      did_snapshot_on_last_retry_(false),
      weak_ptr_factory_(this) {
  DCHECK(offline_page_model_);
  DCHECK(browser_context_);
  // When the offliner is created for test harness runs, the
  // |load_termination_listener_| will be set to nullptr, in order to prevent
  // crashing, adding a check here.
  if (load_termination_listener_)
    load_termination_listener_->set_offliner(this);

  for (int i = 0; i < ResourceDataType::RESOURCE_DATA_TYPE_COUNT; ++i) {
    stats_[i].requested = 0;
    stats_[i].completed = 0;
  }
}

BackgroundLoaderOffliner::~BackgroundLoaderOffliner() {}

// static
BackgroundLoaderOffliner* BackgroundLoaderOffliner::FromWebContents(
    content::WebContents* contents) {
  Offliner* offliner = OfflinerUserData::OfflinerFromWebContents(contents);
  // Today we only have one kind of offliner that uses OfflinerUserData.  If we
  // add other types, revisit this cast.
  if (offliner)
    return static_cast<BackgroundLoaderOffliner*>(offliner);
  return nullptr;
}

bool BackgroundLoaderOffliner::LoadAndSave(
    const SavePageRequest& request,
    const CompletionCallback& completion_callback,
    const ProgressCallback& progress_callback) {
  DCHECK(completion_callback);
  DCHECK(progress_callback);
  DCHECK(offline_page_model_);

  if (pending_request_) {
    DVLOG(1) << "Already have pending request";
    return false;
  }

  ClientPolicyController* policy_controller =
      offline_page_model_->GetPolicyController();
  if (policy_controller->IsDisabledWhenPrefetchDisabled(
          request.client_id().name_space) &&
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

  ResetLoader();
  AttachObservers();

  MarkLoadStartTime();

  // Track copy of pending request.
  pending_request_.reset(new SavePageRequest(request));
  completion_callback_ = completion_callback;
  progress_callback_ = progress_callback;

  if (IsOfflinePagesRenovationsEnabled()) {
    // Lazily create PageRenovationLoader
    if (!page_renovation_loader_)
      page_renovation_loader_ = std::make_unique<PageRenovationLoader>();

    // Set up PageRenovator for this offlining instance.
    auto script_injector = std::make_unique<RenderFrameScriptInjector>(
        loader_->web_contents()->GetMainFrame(),
        ISOLATED_WORLD_ID_CHROME_INTERNAL);
    page_renovator_ = std::make_unique<PageRenovator>(
        page_renovation_loader_.get(), std::move(script_injector),
        request.url());
  }

  // Load page attempt.
  loader_.get()->LoadPage(request.url());

  snapshot_controller_ = SnapshotController::CreateForBackgroundOfflining(
      base::ThreadTaskRunnerHandle::Get(), this, (bool)page_renovator_);

  return true;
}

bool BackgroundLoaderOffliner::Cancel(const CancelCallback& callback) {
  DCHECK(pending_request_);
  // We ignore the case where pending_request_ is not set, but given the checks
  // in RequestCoordinator this should not happen.
  if (!pending_request_)
    return false;

  // TODO(chili): We are not able to cancel a pending
  // OfflinePageModel::SaveSnapshot() operation. We will notify caller that
  // cancel completed once the SavePage operation returns.
  if (save_state_ != NONE) {
    save_state_ = DELETE_AFTER_SAVE;
    cancel_callback_ = callback;
    return true;
  }

  // Post the cancel callback right after this call concludes.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, *pending_request_.get()));
  ResetState();
  return true;
}

void BackgroundLoaderOffliner::TerminateLoadIfInProgress() {
  if (!pending_request_)
    return;

  Cancel(base::Bind(HandleLoadTerminationCancel, completion_callback_));
}

bool BackgroundLoaderOffliner::HandleTimeout(int64_t request_id) {
  if (pending_request_) {
    DCHECK(request_id == pending_request_->request_id());
    if (is_low_bar_met_ && (pending_request_->started_attempt_count() + 1 >=
                                policy_->GetMaxStartedTries() ||
                            pending_request_->completed_attempt_count() + 1 >=
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

void BackgroundLoaderOffliner::CanDownload(
    const base::Callback<void(bool)>& callback) {
  if (!pending_request_.get()) {
    callback.Run(false);  // Shouldn't happen though...
  }

  bool should_allow_downloads = false;
  Offliner::RequestStatus final_status =
      Offliner::RequestStatus::LOADING_FAILED_DOWNLOAD;
  // Check whether we should allow file downloads for this save page request.
  // If we want to proceed with the file download, fail with
  // DOWNLOAD_THROTTLED. If we don't want to proceed with the file download,
  // fail with LOADING_FAILED_DOWNLOAD.
  if (offline_page_model_->GetPolicyController()->ShouldAllowDownloads(
          pending_request_.get()->client_id().name_space)) {
    should_allow_downloads = true;
    final_status = Offliner::RequestStatus::DOWNLOAD_THROTTLED;
  }

  callback.Run(should_allow_downloads);
  SavePageRequest request(*pending_request_.get());
  completion_callback_.Run(request, final_status);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&BackgroundLoaderOffliner::ResetState,
                            weak_ptr_factory_.GetWeakPtr()));
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
                        static_cast<int>(navigation_handle->GetNetErrorCode()));
    page_load_state_ = RETRIABLE;
  } else {
    int status_code = 200;  // Default to OK.
    // No response header can imply intermediate navigation state.
    if (navigation_handle->GetResponseHeaders())
      status_code = navigation_handle->GetResponseHeaders()->response_code();
    // 2XX and 3XX are ok because they indicate success or redirection.
    // We track 301 because it's MOVED_PERMANENTLY and usually accompanies an
    // error page with new address.
    // 400+ codes are client and server errors.
    // We skip 418 because it's a teapot.
    if (status_code == 301 || (status_code >= 400 && status_code != 418)) {
      RecordErrorCauseUMA(pending_request_->client_id(), status_code);
      page_load_state_ = RETRIABLE;
    }
  }

  // Record UMA if we are offlining a previvew instead of an unmodified page.
  // As documented in content/public/browser/navigation_handle.h, this
  // NavigationData is a clone of the NavigationData instance returned from
  // ResourceDispatcherHostDelegate::GetNavigationData during commit.
  // Because ChromeResourceDispatcherHostDelegate always returns a
  // ChromeNavigationData, it is safe to static_cast here.
  ChromeNavigationData* navigation_data = static_cast<ChromeNavigationData*>(
      navigation_handle->GetNavigationData());

  RecordOffliningPreviewsUMA(pending_request_->client_id(), navigation_data);
}

void BackgroundLoaderOffliner::SetSnapshotControllerForTest(
    std::unique_ptr<SnapshotController> controller) {
  snapshot_controller_ = std::move(controller);
}

void BackgroundLoaderOffliner::ObserveResourceLoading(
    ResourceLoadingObserver::ResourceDataType type,
    bool started) {
  // Add the signal to extra data, and use for tracking.

  RequestStats& found_stats = stats_[type];
  if (started)
    ++found_stats.requested;
  else
    ++found_stats.completed;
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
    // Write resource percentage signal data into extra data before emitting it
    // to the MHTML.
    RequestStats& image_stats = stats_[ResourceDataType::IMAGE];
    signal_data_.SetDouble("StartedImages", image_stats.requested);
    signal_data_.SetDouble("CompletedImages", image_stats.completed);
    RequestStats& css_stats = stats_[ResourceDataType::TEXT_CSS];
    signal_data_.SetDouble("StartedCSS", css_stats.requested);
    signal_data_.SetDouble("CompletedCSS", css_stats.completed);
    RequestStats& xhr_stats = stats_[ResourceDataType::XHR];
    signal_data_.SetDouble("StartedXHR", xhr_stats.requested);
    signal_data_.SetDouble("CompletedXHR", xhr_stats.completed);

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
  params.use_page_problem_detectors = true;
  params.request_origin = request.request_origin();

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

void BackgroundLoaderOffliner::RunRenovations() {
  if (page_renovator_) {
    page_renovator_->RunRenovations(
        base::Bind(&BackgroundLoaderOffliner::RenovationsCompleted,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void BackgroundLoaderOffliner::OnPageSaved(SavePageResult save_result,
                                           int64_t offline_id) {
  if (!pending_request_)
    return;

  SavePageRequest request(*pending_request_.get());
  bool did_snapshot_on_last_retry = did_snapshot_on_last_retry_;
  ResetState();

  if (save_state_ == DELETE_AFTER_SAVE) {
    // Delete the saved page off disk and from the OPM.
    std::vector<int64_t> offline_ids;
    offline_ids.push_back(offline_id);
    offline_page_model_->DeletePagesByOfflineId(
        offline_ids,
        base::Bind(&BackgroundLoaderOffliner::DeleteOfflinePageCallback,
                   weak_ptr_factory_.GetWeakPtr(), request));
    save_state_ = NONE;
    return;
  }

  save_state_ = NONE;

  Offliner::RequestStatus save_status;
  if (save_result == SavePageResult::ALREADY_EXISTS) {
    save_status = RequestStatus::SAVED;
  } else if (save_result == SavePageResult::SUCCESS) {
    if (did_snapshot_on_last_retry)
      save_status = RequestStatus::SAVED_ON_LAST_RETRY;
    else
      save_status = RequestStatus::SAVED;
  } else {
    save_status = RequestStatus::SAVE_FAILED;
  }

  completion_callback_.Run(request, save_status);
}

void BackgroundLoaderOffliner::DeleteOfflinePageCallback(
    const SavePageRequest& request,
    DeletePageResult result) {
  cancel_callback_.Run(request);
}

void BackgroundLoaderOffliner::ResetState() {
  pending_request_.reset();
  // Stop snapshot controller from triggering any more events.
  snapshot_controller_->Stop();
  // Delete the snapshot controller after stack unwinds, so we don't
  // corrupt stack in some edge cases. Deleting it soon should be safe because
  // we check against pending_request_ with every action, and snapshot
  // controller is configured to only call StartSnapshot once for BGL.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, snapshot_controller_.release());
  page_load_state_ = SUCCESS;
  network_bytes_ = 0LL;
  is_low_bar_met_ = false;
  did_snapshot_on_last_retry_ = false;
  content::WebContentsObserver::Observe(nullptr);
  loader_.reset();

  for (int i = 0; i < ResourceDataType::RESOURCE_DATA_TYPE_COUNT; ++i) {
    stats_[i].requested = 0;
    stats_[i].completed = 0;
  }
}

void BackgroundLoaderOffliner::ResetLoader() {
  loader_.reset(
      new background_loader::BackgroundLoaderContents(browser_context_));
  loader_->SetDelegate(this);
}

void BackgroundLoaderOffliner::AttachObservers() {
  content::WebContents* contents = loader_->web_contents();
  content::WebContentsObserver::Observe(contents);
  OfflinerUserData::AddToWebContents(contents, this);
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

void BackgroundLoaderOffliner::RenovationsCompleted() {
  snapshot_controller_->RenovationsCompleted();
}

}  // namespace offline_pages
