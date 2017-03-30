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
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_manager.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

// Internal functionality of the BackgroundFetchJobController that lives on the
// UI thread, where all interaction with the download manager must happen.
class BackgroundFetchJobController::Core : public DownloadItem::Observer {
 public:
  Core(const base::WeakPtr<BackgroundFetchJobController>& io_parent,
       BrowserContext* browser_context,
       scoped_refptr<net::URLRequestContextGetter> request_context)
      : io_parent_(io_parent),
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
  void StartRequest(const BackgroundFetchRequestInfo& request) {
    DCHECK(request_context_);

    DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(browser_context_);
    DCHECK(download_manager);

    std::unique_ptr<DownloadUrlParameters> download_parameters(
        base::MakeUnique<DownloadUrlParameters>(request.GetURL(),
                                                request_context_.get()));

    // TODO(peter): The |download_parameters| should be populated with all the
    // properties set in the |request|'s ServiceWorkerFetchRequest member.

    download_parameters->set_callback(base::Bind(
        &Core::DidStartRequest, weak_ptr_factory_.GetWeakPtr(), request));

    download_manager->DownloadUrl(std::move(download_parameters));
  }

  // DownloadItem::Observer overrides:
  void OnDownloadUpdated(DownloadItem* item) override {
    auto iter = downloads_.find(item);
    DCHECK(iter != downloads_.end());

    const BackgroundFetchRequestInfo& request = iter->second;

    switch (item->GetState()) {
      case DownloadItem::DownloadState::COMPLETE:
        // TODO(peter): Populate the responses' information in the |request|.

        item->RemoveObserver(this);

        // Inform the host about |host| having completed.
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&BackgroundFetchJobController::DidCompleteRequest,
                       io_parent_, request));

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

  void OnDownloadDestroyed(DownloadItem* item) override {
    DCHECK_EQ(downloads_.count(item), 1u);
    downloads_.erase(item);

    item->RemoveObserver(this);
  }

 private:
  // Called when the download manager has started the given |request|. The
  // |download_item| continues to be owned by the download system. The
  // |interrupt_reason| will indicate when a request could not be started.
  void DidStartRequest(const BackgroundFetchRequestInfo& request,
                       DownloadItem* download_item,
                       DownloadInterruptReason interrupt_reason) {
    DCHECK_EQ(interrupt_reason, DOWNLOAD_INTERRUPT_REASON_NONE);
    DCHECK(download_item);

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
    downloads_.insert(std::make_pair(download_item, request));
  }

  // Weak reference to the BackgroundFetchJobController instance that owns us.
  base::WeakPtr<BackgroundFetchJobController> io_parent_;

  // The BrowserContext that owns the JobController, and thereby us.
  BrowserContext* browser_context_;

  // The URL request context to use when issuing the requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // Map from DownloadItem* to the request info for the in-progress downloads.
  std::unordered_map<DownloadItem*, BackgroundFetchRequestInfo> downloads_;

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
  ui_core_.reset(new Core(weak_ptr_factory_.GetWeakPtr(), browser_context,
                          std::move(request_context)));

  // Get a WeakPtr over which we can talk to the |ui_core_|.
  ui_core_ptr_ = ui_core_->GetWeakPtr();
}

BackgroundFetchJobController::~BackgroundFetchJobController() = default;

void BackgroundFetchJobController::Start(
    std::vector<BackgroundFetchRequestInfo> initial_requests) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_LE(initial_requests.size(), kMaximumBackgroundFetchParallelRequests);
  DCHECK_EQ(state_, State::INITIALIZED);

  state_ = State::FETCHING;

  for (const BackgroundFetchRequestInfo& request : initial_requests)
    StartRequest(request);
}

void BackgroundFetchJobController::StartRequest(
    const BackgroundFetchRequestInfo& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(state_, State::FETCHING);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Core::StartRequest, ui_core_ptr_, request));
}

void BackgroundFetchJobController::DidStartRequest(
    const BackgroundFetchRequestInfo& request,
    const std::string& download_guid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_manager_->MarkRequestAsStarted(registration_id_, request, download_guid);
}

void BackgroundFetchJobController::DidCompleteRequest(
    const BackgroundFetchRequestInfo& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The DataManager must acknowledge that it stored the data and that there are
  // no more pending requests to avoid marking this job as completed too early.
  pending_completed_file_acknowledgements_++;

  data_manager_->MarkRequestAsCompleteAndGetNextRequest(
      registration_id_, request,
      base::BindOnce(&BackgroundFetchJobController::DidGetNextRequest,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundFetchJobController::DidGetNextRequest(
    const base::Optional<BackgroundFetchRequestInfo>& request) {
  DCHECK_LE(pending_completed_file_acknowledgements_, 1);
  pending_completed_file_acknowledgements_--;

  // If a |request| has been given, start downloading the file and bail.
  if (request) {
    StartRequest(request.value());
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
