// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_manager.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#endif

namespace content {

#if defined(OS_ANDROID)
namespace {

// Prefix for files stored in the Chromium-internal download directory to
// indicate files thta were fetched through Background Fetch.
const char kBackgroundFetchFilePrefix[] = "BGFetch-";

}  // namespace
#endif  // defined(OS_ANDROID)

// Internal functionality of the BackgroundFetchJobController that lives on the
// UI thread, where all interaction with the download manager must happen.
class BackgroundFetchJobController::Core : public DownloadItem::Observer {
 public:
  Core(const base::WeakPtr<BackgroundFetchJobController>& io_parent,
       const BackgroundFetchRegistrationId& registration_id,
       BrowserContext* browser_context,
       scoped_refptr<net::URLRequestContextGetter> request_context)
      : io_parent_(io_parent),
        registration_id_(registration_id),
        browser_context_(browser_context),
        request_context_(std::move(request_context)),
        weak_ptr_factory_(this) {}

  ~Core() final {
    for (const auto& pair : downloads_)
      pair.first->RemoveObserver(this);
  }

  // Returns a weak pointer that can be used to talk to |this|.
  base::WeakPtr<Core> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  // Starts fetching the |request| with the download manager.
  void StartRequest(
      scoped_refptr<BackgroundFetchRequestInfo> request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(request_context_);
    DCHECK(request);

    DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(browser_context_);
    DCHECK(download_manager);

    const ServiceWorkerFetchRequest& fetch_request = request->fetch_request();

    std::unique_ptr<DownloadUrlParameters> download_parameters(
        base::MakeUnique<DownloadUrlParameters>(
            fetch_request.url, request_context_.get(), traffic_annotation));

    // TODO(peter): The |download_parameters| should be populated with all the
    // properties set in the |fetch_request| structure.

    for (const auto& pair : fetch_request.headers)
      download_parameters->add_request_header(pair.first, pair.second);

    // Append the Origin header for requests whose CORS flag is set, or whose
    // request method is not GET or HEAD. See section 3.1 of the standard:
    // https://fetch.spec.whatwg.org/#origin-header
    if (fetch_request.mode == FETCH_REQUEST_MODE_CORS ||
        fetch_request.mode == FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT ||
        (fetch_request.method != "GET" && fetch_request.method != "POST")) {
      download_parameters->add_request_header(
          "Origin", registration_id_.origin().Serialize());
    }

    // TODO(peter): Background Fetch responses should not end up in the user's
    // download folder on any platform. Find an appropriate solution for desktop
    // too. The Android internal directory is not scoped to a profile.

    download_parameters->set_transient(true);

#if defined(OS_ANDROID)
    base::FilePath download_directory;
    if (base::android::GetDownloadInternalDirectory(&download_directory)) {
      download_parameters->set_file_path(download_directory.Append(
          std::string(kBackgroundFetchFilePrefix) + base::GenerateGUID()));
    }
#endif  // defined(OS_ANDROID)

    download_parameters->set_callback(base::Bind(&Core::DidStartRequest,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 std::move(request)));

    download_manager->DownloadUrl(std::move(download_parameters));
  }

  // DownloadItem::Observer overrides:
  void OnDownloadUpdated(DownloadItem* download_item) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    auto iter = downloads_.find(download_item);
    DCHECK(iter != downloads_.end());

    scoped_refptr<BackgroundFetchRequestInfo> request = iter->second;

    switch (download_item->GetState()) {
      case DownloadItem::DownloadState::COMPLETE:
        request->PopulateResponseFromDownloadItem(download_item);
        download_item->RemoveObserver(this);

        // Inform the host about |host| having completed.
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&BackgroundFetchJobController::DidCompleteRequest,
                       io_parent_, std::move(request)));

        // Clear the local state for the |request|, it no longer is our concern.
        downloads_.erase(iter);
        break;
      case DownloadItem::DownloadState::CANCELLED:
        // TODO(harkness): Consider how we want to handle cancelled downloads.
        break;
      case DownloadItem::DownloadState::INTERRUPTED:
        // TODO(harkness): Just update the notification that it is paused.
        break;
      case DownloadItem::DownloadState::IN_PROGRESS:
        // TODO(harkness): If the download was previously paused, this should
        // now unpause the notification.
        break;
      case DownloadItem::DownloadState::MAX_DOWNLOAD_STATE:
        NOTREACHED();
        break;
    }
  }

  void OnDownloadDestroyed(DownloadItem* download_item) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(downloads_.count(download_item), 1u);
    downloads_.erase(download_item);

    download_item->RemoveObserver(this);
  }

 private:
  // Called when the download manager has started the given |request|. The
  // |download_item| continues to be owned by the download system. The
  // |interrupt_reason| will indicate when a request could not be started.
  void DidStartRequest(scoped_refptr<BackgroundFetchRequestInfo> request,
                       DownloadItem* download_item,
                       DownloadInterruptReason interrupt_reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(interrupt_reason, DOWNLOAD_INTERRUPT_REASON_NONE);
    DCHECK(download_item);

    request->PopulateDownloadState(download_item, interrupt_reason);

    // TODO(peter): The above two DCHECKs are assumptions our implementation
    // currently makes, but are not fit for production. We need to handle such
    // failures gracefully.

    // Register for updates on the download's progress.
    download_item->AddObserver(this);

    // Inform the host about the |request| having started.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BackgroundFetchJobController::DidStartRequest, io_parent_,
                   request, download_item->GetGuid()));

    // Associate the |download_item| with the |request| so that we can retrieve
    // it's information when further updates happen.
    downloads_.insert(std::make_pair(download_item, std::move(request)));
  }

  // Weak reference to the BackgroundFetchJobController instance that owns us.
  base::WeakPtr<BackgroundFetchJobController> io_parent_;

  // The Background Fetch registration Id for which this request is being made.
  BackgroundFetchRegistrationId registration_id_;

  // The BrowserContext that owns the JobController, and thereby us.
  BrowserContext* browser_context_;

  // The URL request context to use when issuing the requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // Map from DownloadItem* to the request info for the in-progress downloads.
  std::unordered_map<DownloadItem*, scoped_refptr<BackgroundFetchRequestInfo>>
      downloads_;

  base::WeakPtrFactory<Core> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

BackgroundFetchJobController::BackgroundFetchJobController(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options,
    BackgroundFetchDataManager* data_manager,
    BrowserContext* browser_context,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    CompletedCallback completed_callback)
    : registration_id_(registration_id),
      options_(options),
      data_manager_(data_manager),
      completed_callback_(std::move(completed_callback)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Create the core, containing the internal functionality that will have to
  // be run on the UI thread. It will respond to this class with a weak pointer.
  ui_core_.reset(new Core(weak_ptr_factory_.GetWeakPtr(), registration_id,
                          browser_context, std::move(request_context)));

  // Get a WeakPtr over which we can talk to the |ui_core_|.
  ui_core_ptr_ = ui_core_->GetWeakPtr();
}

BackgroundFetchJobController::~BackgroundFetchJobController() = default;

void BackgroundFetchJobController::Start(
    std::vector<scoped_refptr<BackgroundFetchRequestInfo>> initial_requests,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_LE(initial_requests.size(), kMaximumBackgroundFetchParallelRequests);
  DCHECK_EQ(state_, State::INITIALIZED);

  state_ = State::FETCHING;

  for (const auto& request : initial_requests)
    StartRequest(request, traffic_annotation);
}

void BackgroundFetchJobController::StartRequest(
    scoped_refptr<BackgroundFetchRequestInfo> request,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(state_, State::FETCHING);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&Core::StartRequest, ui_core_ptr_,
                                     std::move(request), traffic_annotation));
}

void BackgroundFetchJobController::DidStartRequest(
    scoped_refptr<BackgroundFetchRequestInfo> request,
    const std::string& download_guid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_manager_->MarkRequestAsStarted(registration_id_, request.get(),
                                      download_guid);
}

void BackgroundFetchJobController::DidCompleteRequest(
    scoped_refptr<BackgroundFetchRequestInfo> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The DataManager must acknowledge that it stored the data and that there are
  // no more pending requests to avoid marking this job as completed too early.
  pending_completed_file_acknowledgements_++;

  data_manager_->MarkRequestAsCompleteAndGetNextRequest(
      registration_id_, request.get(),
      base::BindOnce(&BackgroundFetchJobController::DidGetNextRequest,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundFetchJobController::DidGetNextRequest(
    scoped_refptr<BackgroundFetchRequestInfo> request) {
  DCHECK_LE(pending_completed_file_acknowledgements_, 1);
  pending_completed_file_acknowledgements_--;

  // If a |request| has been given, start downloading the file and bail.
  if (request) {
    StartRequest(std::move(request), NO_TRAFFIC_ANNOTATION_YET);
    return;
  }

  // If there are outstanding completed file acknowlegements, bail as well.
  if (pending_completed_file_acknowledgements_ > 0)
    return;

  state_ = State::COMPLETED;

  // Otherwise the job this controller is responsible for has completed.
  std::move(completed_callback_).Run(this);
}

void BackgroundFetchJobController::UpdateUI(const std::string& title) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(harkness): Update the user interface with |title|.
}

void BackgroundFetchJobController::Abort() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(harkness): Abort all in-progress downloads.

  state_ = State::ABORTED;

  // Inform the owner of the controller about the job having completed.
  std::move(completed_callback_).Run(this);
}

}  // namespace content
