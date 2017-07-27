// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"

#include <unordered_map>
#include <utility>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"

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

// Internal functionality of the BackgroundFetchDelegateProxy that lives on the
// UI thread, where all interaction with the download manager must happen.
class BackgroundFetchDelegateProxy::Core {
 public:
  Core(const base::WeakPtr<BackgroundFetchDelegateProxy>& io_parent,
       BrowserContext* browser_context,
       scoped_refptr<net::URLRequestContextGetter> request_context)
      : io_parent_(io_parent),
        browser_context_(browser_context),
        request_context_(std::move(request_context)),
        traffic_annotation_(
            net::DefineNetworkTrafficAnnotation("background_fetch_context",
                                                R"(
            semantics {
              sender: "Background Fetch API"
              description:
                "The Background Fetch API enables developers to upload or "
                "download files on behalf of the user. Such fetches will yield "
                "a user visible notification to inform the user of the "
                "operation, through which it can be suspended, resumed and/or "
                "cancelled. The developer retains control of the file once the "
                "fetch is completed,  similar to XMLHttpRequest and other "
                "mechanisms for fetching resources using JavaScript."
              trigger:
                "When the website uses the Background Fetch API to request "
                "fetching a file and/or a list of files. This is a Web "
                "Platform API for which no express user permission is required."
              data:
                "The request headers and data as set by the website's "
                "developer."
              destination: WEBSITE
            }
            policy {
              cookies_allowed: YES
              cookies_store: "user"
              setting: "This feature cannot be disabled in settings."
              policy_exception_justification: "Not implemented."
            })")),
        weak_ptr_factory_(this) {
    // Although the Core lives only on the UI thread, it is constructed on IO.
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  }

  ~Core() { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

  base::WeakPtr<Core> GetWeakPtrOnUI() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void StartRequest(
      const base::WeakPtr<BackgroundFetchJobController>& job_controller,
      const url::Origin& origin,
      scoped_refptr<BackgroundFetchRequestInfo> request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(request_context_);
    DCHECK(request);

    DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(browser_context_);
    DCHECK(download_manager);

    const ServiceWorkerFetchRequest& fetch_request = request->fetch_request();

    std::unique_ptr<DownloadUrlParameters> download_parameters(
        base::MakeUnique<DownloadUrlParameters>(
            fetch_request.url, request_context_.get(), traffic_annotation_));

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
      download_parameters->add_request_header("Origin", origin.Serialize());
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

    download_parameters->set_callback(
        base::Bind(&Core::DidStartRequest, weak_ptr_factory_.GetWeakPtr(),
                   job_controller, std::move(request)));

    download_manager->DownloadUrl(std::move(download_parameters));
  }

 private:
  class DownloadItemObserver : public DownloadItem::Observer {
   public:
    DownloadItemObserver(
        base::WeakPtr<Core> core,
        const base::WeakPtr<BackgroundFetchJobController>& job_controller)
        : core_(core), job_controller_(job_controller) {
      DCHECK_CURRENTLY_ON(BrowserThread::UI);
    }

    void OnDownloadUpdated(DownloadItem* download_item) override;
    void OnDownloadDestroyed(DownloadItem* download_item) override;

   private:
    base::WeakPtr<Core> core_;
    base::WeakPtr<BackgroundFetchJobController> job_controller_;
  };

  // Called when the download manager has started the given |request|. The
  // |download_item| continues to be owned by the download system. The
  // |interrupt_reason| will indicate when a request could not be started.
  void DidStartRequest(
      const base::WeakPtr<BackgroundFetchJobController>& job_controller,
      scoped_refptr<BackgroundFetchRequestInfo> request,
      DownloadItem* download_item,
      DownloadInterruptReason interrupt_reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(interrupt_reason, DOWNLOAD_INTERRUPT_REASON_NONE);
    DCHECK(download_item);

    request->PopulateDownloadStateOnUI(download_item, interrupt_reason);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BackgroundFetchRequestInfo::SetDownloadStatePopulated,
                   request));

    // TODO(peter): The above two DCHECKs are assumptions our implementation
    // currently makes, but are not fit for production. We need to handle such
    // failures gracefully.

    // Register for updates on the download's progress.
    download_item->AddObserver(new DownloadItemObserver(
        weak_ptr_factory_.GetWeakPtr(), job_controller));

    // Inform the host about the |request| having started.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BackgroundFetchDelegateProxy::DidStartRequest, io_parent_,
                   job_controller, request, download_item->GetGuid()));

    // Associate the |download_item| with the |request| so that we can retrieve
    // its information when further updates happen.
    downloads_.insert(std::make_pair(download_item, std::move(request)));
  }

  // Weak reference to the BackgroundFetchJobController instance that owns us.
  base::WeakPtr<BackgroundFetchDelegateProxy> io_parent_;

  // The BrowserContext that owns the JobController, and thereby us.
  BrowserContext* browser_context_;

  // The URL request context to use when issuing the requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // Traffic annotation for network request.
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // Map from DownloadItem* to the request info for the in-progress downloads.
  std::unordered_map<DownloadItem*, scoped_refptr<BackgroundFetchRequestInfo>>
      downloads_;

  base::WeakPtrFactory<Core> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

void BackgroundFetchDelegateProxy::Core::DownloadItemObserver::
    OnDownloadUpdated(DownloadItem* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!core_.get()) {
    download_item->RemoveObserver(this);
    delete this;
    return;
  }

  auto iter = core_->downloads_.find(download_item);
  DCHECK(iter != core_->downloads_.end());

  scoped_refptr<BackgroundFetchRequestInfo> request = iter->second;

  switch (download_item->GetState()) {
    case DownloadItem::DownloadState::COMPLETE:
      request->PopulateResponseFromDownloadItemOnUI(download_item);
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&BackgroundFetchRequestInfo::SetResponseDataPopulated,
                     request));

      // Inform the host about |host| having completed.
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&BackgroundFetchDelegateProxy::DidCompleteRequest,
                     core_->io_parent_, job_controller_, std::move(request)));

      // Clear the local state for the |request|, it no longer is our
      // concern.
      core_->downloads_.erase(iter);

      download_item->RemoveObserver(this);
      delete this;
      // Cannot access this after deleting itself so return immediately.
      return;
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

void BackgroundFetchDelegateProxy::Core::DownloadItemObserver::
    OnDownloadDestroyed(DownloadItem* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (core_.get()) {
    DCHECK_EQ(core_->downloads_.count(download_item), 1u);
    core_->downloads_.erase(download_item);
  }

  download_item->RemoveObserver(this);
  delete this;
}

BackgroundFetchDelegateProxy::BackgroundFetchDelegateProxy(
    BrowserContext* browser_context,
    scoped_refptr<net::URLRequestContextGetter> request_context)
    : weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ui_core_.reset(new Core(weak_ptr_factory_.GetWeakPtr(), browser_context,
                          request_context));

  // Get a WeakPtr over which we can talk to the |ui_core_|. Normally it
  // would be unsafe to obtain a weak pointer on the IO thread from a
  // factory that lives on the UI thread, but it's ok in this constructor
  // since the Core can't be destroyed before this constructor finishes.
  ui_core_ptr_ = ui_core_->GetWeakPtrOnUI();
}

BackgroundFetchDelegateProxy::~BackgroundFetchDelegateProxy() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchDelegateProxy::StartRequest(
    BackgroundFetchJobController* job_controller,
    scoped_refptr<BackgroundFetchRequestInfo> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&Core::StartRequest, ui_core_ptr_,
                                     job_controller->GetWeakPtr(),
                                     job_controller->registration_id().origin(),
                                     std::move(request)));
}

void BackgroundFetchDelegateProxy::UpdateUI(const std::string& title) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(delphick): Update the user interface with |title|.
}

void BackgroundFetchDelegateProxy::Abort() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(delphick): Abort all in-progress downloads.
}

void BackgroundFetchDelegateProxy::DidStartRequest(
    const base::WeakPtr<BackgroundFetchJobController>& job_controller,
    scoped_refptr<BackgroundFetchRequestInfo> request,
    const std::string& download_guid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (job_controller) {
    job_controller->DidStartRequest(request, download_guid);
  }
}

void BackgroundFetchDelegateProxy::DidCompleteRequest(
    const base::WeakPtr<BackgroundFetchJobController>& job_controller,
    scoped_refptr<BackgroundFetchRequestInfo> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (job_controller) {
    job_controller->DidCompleteRequest(request);
  }
}

}  // namespace content
