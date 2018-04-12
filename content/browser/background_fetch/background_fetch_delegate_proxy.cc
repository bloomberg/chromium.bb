// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"

#include <utility>

#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/download_manager.h"
#include "ui/gfx/geometry/size.h"

class SkBitmap;

namespace content {

// Internal functionality of the BackgroundFetchDelegateProxy that lives on the
// UI thread, where all interaction with the download manager must happen.
class BackgroundFetchDelegateProxy::Core
    : public BackgroundFetchDelegate::Client {
 public:
  Core(const base::WeakPtr<BackgroundFetchDelegateProxy>& io_parent,
       BackgroundFetchDelegate* delegate)
      : io_parent_(io_parent), delegate_(delegate), weak_ptr_factory_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Some BrowserContext implementations return nullptr for their delegate
    // implementation and the feature should be disabled in that case.
    if (delegate_)
      delegate_->SetDelegateClient(GetWeakPtrOnUI());
  }

  ~Core() override { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

  base::WeakPtr<Core> GetWeakPtrOnUI() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return weak_ptr_factory_.GetWeakPtr();
  }

  void ForwardGetIconDisplaySizeCallbackToIO(
      BackgroundFetchDelegate::GetIconDisplaySizeCallback callback,
      const gfx::Size& display_size) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::BindOnce(std::move(callback), display_size));
  }

  void GetIconDisplaySize(
      BackgroundFetchDelegate::GetIconDisplaySizeCallback callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (delegate_) {
      delegate_->GetIconDisplaySize(
          base::BindOnce(&Core::ForwardGetIconDisplaySizeCallbackToIO,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    } else {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::BindOnce(std::move(callback), gfx::Size(0, 0)));
    }
  }

  void CreateDownloadJob(const std::string& job_unique_id,
                         const std::string& title,
                         const url::Origin& origin,
                         const SkBitmap& icon,
                         int completed_parts,
                         int total_parts,
                         const std::vector<std::string>& current_guids) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (delegate_) {
      delegate_->CreateDownloadJob(job_unique_id, title, origin, icon,
                                   completed_parts, total_parts, current_guids);
    }
  }

  void StartRequest(const std::string& job_unique_id,
                    const url::Origin& origin,
                    scoped_refptr<BackgroundFetchRequestInfo> request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(request);

    // TODO(crbug/757760): This can be nullptr, if the delegate has shut down,
    // in which case we need to make sure this is retried when the browser
    // restarts.
    if (!delegate_)
      return;

    const ServiceWorkerFetchRequest& fetch_request = request->fetch_request();

    const net::NetworkTrafficAnnotationTag traffic_annotation(
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
            })"));

    // TODO(peter): The |headers| should be populated with all the properties
    // set in the |fetch_request| structure.
    net::HttpRequestHeaders headers;
    for (const auto& pair : fetch_request.headers)
      headers.SetHeader(pair.first, pair.second);

    // Append the Origin header for requests whose CORS flag is set, or whose
    // request method is not GET or HEAD. See section 3.1 of the standard:
    // https://fetch.spec.whatwg.org/#origin-header
    if (fetch_request.mode == network::mojom::FetchRequestMode::kCORS ||
        fetch_request.mode ==
            network::mojom::FetchRequestMode::kCORSWithForcedPreflight ||
        (fetch_request.method != "GET" && fetch_request.method != "POST")) {
      headers.SetHeader("Origin", origin.Serialize());
    }

    delegate_->DownloadUrl(job_unique_id, request->download_guid(),
                           fetch_request.method, fetch_request.url,
                           traffic_annotation, headers);
  }

  void Abort(const std::string& job_unique_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (delegate_)
      delegate_->Abort(job_unique_id);
  }

  // BackgroundFetchDelegate::Client implementation:
  void OnJobCancelled(const std::string& job_unique_id) override;
  void OnDownloadUpdated(const std::string& job_unique_id,
                         const std::string& guid,
                         uint64_t bytes_downloaded) override;
  void OnDownloadComplete(
      const std::string& job_unique_id,
      const std::string& guid,
      std::unique_ptr<BackgroundFetchResult> result) override;
  void OnDownloadStarted(
      const std::string& job_unique_id,
      const std::string& guid,
      std::unique_ptr<content::BackgroundFetchResponse> response) override;
  void OnDelegateShutdown() override;

 private:
  // Weak reference to the IO thread outer class that owns us.
  base::WeakPtr<BackgroundFetchDelegateProxy> io_parent_;

  // Delegate is owned elsewhere and is valid from construction until
  // OnDelegateShutDown (if not initially nullptr).
  BackgroundFetchDelegate* delegate_;

  base::WeakPtrFactory<Core> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

void BackgroundFetchDelegateProxy::Core::OnJobCancelled(
    const std::string& job_unique_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BackgroundFetchDelegateProxy::OnJobCancelled, io_parent_,
                     job_unique_id));
}

void BackgroundFetchDelegateProxy::Core::OnDownloadUpdated(
    const std::string& job_unique_id,
    const std::string& guid,
    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BackgroundFetchDelegateProxy::OnDownloadUpdated,
                     io_parent_, job_unique_id, guid, bytes_downloaded));
}

void BackgroundFetchDelegateProxy::Core::OnDownloadComplete(
    const std::string& job_unique_id,
    const std::string& guid,
    std::unique_ptr<BackgroundFetchResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BackgroundFetchDelegateProxy::OnDownloadComplete,
                     io_parent_, job_unique_id, guid, std::move(result)));
}

void BackgroundFetchDelegateProxy::Core::OnDownloadStarted(
    const std::string& job_unique_id,
    const std::string& guid,
    std::unique_ptr<content::BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BackgroundFetchDelegateProxy::DidStartRequest, io_parent_,
                     job_unique_id, guid, std::move(response)));
}

void BackgroundFetchDelegateProxy::Core::OnDelegateShutdown() {
  delegate_ = nullptr;
}

BackgroundFetchDelegateProxy::JobDetails::JobDetails(
    base::WeakPtr<Controller> controller)
    : controller(controller) {}

BackgroundFetchDelegateProxy::JobDetails::JobDetails(JobDetails&& details) =
    default;

BackgroundFetchDelegateProxy::JobDetails::~JobDetails() = default;

BackgroundFetchDelegateProxy::BackgroundFetchDelegateProxy(
    BackgroundFetchDelegate* delegate)
    : weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Normally it would be unsafe to obtain a weak pointer on the UI thread from
  // a factory that lives on the IO thread, but it's ok in the constructor as
  // |this| can't be destroyed before the constructor finishes.
  ui_core_.reset(new Core(weak_ptr_factory_.GetWeakPtr(), delegate));

  // Since this constructor runs on the UI thread, a WeakPtr can be safely
  // obtained from the Core.
  ui_core_ptr_ = ui_core_->GetWeakPtrOnUI();
}

BackgroundFetchDelegateProxy::~BackgroundFetchDelegateProxy() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchDelegateProxy::GetIconDisplaySize(
    BackgroundFetchDelegate::GetIconDisplaySizeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(&Core::GetIconDisplaySize,
                                         ui_core_ptr_, std::move(callback)));
}

void BackgroundFetchDelegateProxy::CreateDownloadJob(
    const std::string& job_unique_id,
    const std::string& title,
    const url::Origin& origin,
    const SkBitmap& icon,
    base::WeakPtr<Controller> controller,
    int completed_parts,
    int total_parts,
    const std::vector<std::string>& current_guids) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!job_details_map_.count(job_unique_id));
  job_details_map_.emplace(job_unique_id, JobDetails(controller));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&Core::CreateDownloadJob, ui_core_ptr_, job_unique_id,
                     title, origin, icon, completed_parts, total_parts,
                     current_guids));
}

void BackgroundFetchDelegateProxy::StartRequest(
    const std::string& job_unique_id,
    const url::Origin& origin,
    scoped_refptr<BackgroundFetchRequestInfo> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(job_details_map_.count(job_unique_id));
  JobDetails& job_details = job_details_map_.find(job_unique_id)->second;
  DCHECK(job_details.controller);

  std::string download_guid = request->download_guid();
  DCHECK(!download_guid.empty());

  job_details.current_request_map[download_guid] = request;

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(&Core::StartRequest, ui_core_ptr_,
                                         job_unique_id, origin, request));
}

void BackgroundFetchDelegateProxy::UpdateUI(const std::string& job_unique_id,
                                            const std::string& title) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(delphick): Update the user interface with |title|.
}

void BackgroundFetchDelegateProxy::Abort(const std::string& job_unique_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&Core::Abort, ui_core_ptr_, job_unique_id));

  job_details_map_.erase(job_details_map_.find(job_unique_id));
}

void BackgroundFetchDelegateProxy::OnJobCancelled(
    const std::string& job_unique_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(delphick): The controller may not exist as persistence is not yet
  // implemented.
  auto job_details_iter = job_details_map_.find(job_unique_id);
  if (job_details_iter == job_details_map_.end())
    return;

  JobDetails& job_details = job_details_iter->second;
  if (job_details.controller)
    job_details.controller->AbortFromUser();
}

void BackgroundFetchDelegateProxy::DidStartRequest(
    const std::string& job_unique_id,
    const std::string& guid,
    std::unique_ptr<BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(delphick): The controller may not exist as persistence is not yet
  // implemented.
  auto job_details_iter = job_details_map_.find(job_unique_id);
  if (job_details_iter == job_details_map_.end())
    return;

  JobDetails& job_details = job_details_iter->second;

  const scoped_refptr<BackgroundFetchRequestInfo>& request_info =
      job_details.current_request_map[guid];
  DCHECK_EQ(guid, request_info->download_guid());

  request_info->PopulateWithResponse(std::move(response));

  if (job_details.controller)
    job_details.controller->DidStartRequest(request_info);
}

void BackgroundFetchDelegateProxy::OnDownloadUpdated(
    const std::string& job_unique_id,
    const std::string& guid,
    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(delphick): The controller may not exist as persistence is not yet
  // implemented.
  auto job_details_iter = job_details_map_.find(job_unique_id);
  if (job_details_iter == job_details_map_.end())
    return;

  JobDetails& job_details = job_details_iter->second;

  // TODO(peter): Should we update the |request_info| with the progress?
  if (job_details.controller) {
    const scoped_refptr<BackgroundFetchRequestInfo>& request_info =
        job_details.current_request_map[guid];
    DCHECK_EQ(guid, request_info->download_guid());
    job_details.controller->DidUpdateRequest(request_info, bytes_downloaded);
  }
}

void BackgroundFetchDelegateProxy::OnDownloadComplete(
    const std::string& job_unique_id,
    const std::string& guid,
    std::unique_ptr<BackgroundFetchResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(delphick): The controller may not exist as persistence is not yet
  // implemented.
  auto job_details_iter = job_details_map_.find(job_unique_id);
  if (job_details_iter == job_details_map_.end())
    return;

  JobDetails& job_details = job_details_iter->second;

  const scoped_refptr<BackgroundFetchRequestInfo>& request_info =
      job_details.current_request_map[guid];
  DCHECK_EQ(guid, request_info->download_guid());
  request_info->SetResult(std::move(result));

  if (job_details.controller)
    job_details.controller->DidCompleteRequest(request_info);
}

}  // namespace content
