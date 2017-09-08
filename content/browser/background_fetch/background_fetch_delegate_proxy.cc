// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"

#include <utility>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_delegate.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_response.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"

namespace content {

// Internal functionality of the BackgroundFetchDelegateProxy that lives on the
// UI thread, where all interaction with the download manager must happen.
class BackgroundFetchDelegateProxy::Core
    : public BackgroundFetchDelegate::Client {
 public:
  Core(const base::WeakPtr<BackgroundFetchDelegateProxy>& io_parent,
       base::WeakPtr<BackgroundFetchDelegate> delegate)
      : io_parent_(io_parent), delegate_(delegate), weak_ptr_factory_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    delegate_->SetDelegateClient(weak_ptr_factory_.GetWeakPtr());
  }

  ~Core() override { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

  base::WeakPtr<Core> GetWeakPtrOnUI() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return weak_ptr_factory_.GetWeakPtr();
  }

  void StartRequest(const std::string& guid,
                    const url::Origin& origin,
                    scoped_refptr<BackgroundFetchRequestInfo> request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(request);

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
    if (fetch_request.mode == FETCH_REQUEST_MODE_CORS ||
        fetch_request.mode == FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT ||
        (fetch_request.method != "GET" && fetch_request.method != "POST")) {
      headers.SetHeader("Origin", origin.Serialize());
    }

    delegate_->DownloadUrl(guid, fetch_request.method, fetch_request.url,
                           traffic_annotation, headers);
  }

  // BackgroundFetchDelegate::Client implementation:
  void OnDownloadUpdated(const std::string& guid,
                         uint64_t bytes_downloaded) override;
  void OnDownloadComplete(
      const std::string& guid,
      std::unique_ptr<BackgroundFetchResult> result) override;
  void OnDownloadStarted(
      const std::string& guid,
      std::unique_ptr<content::BackgroundFetchResponse> response) override;

 private:
  // Weak reference to the IO thread outer class that owns us.
  base::WeakPtr<BackgroundFetchDelegateProxy> io_parent_;

  base::WeakPtr<BackgroundFetchDelegate> delegate_;

  base::WeakPtrFactory<Core> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

void BackgroundFetchDelegateProxy::Core::OnDownloadUpdated(
    const std::string& guid,
    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BackgroundFetchDelegateProxy::Core::OnDownloadComplete(
    const std::string& guid,
    std::unique_ptr<BackgroundFetchResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BackgroundFetchDelegateProxy::OnDownloadComplete,
                     io_parent_, guid, std::move(result)));
}

void BackgroundFetchDelegateProxy::Core::OnDownloadStarted(
    const std::string& guid,
    std::unique_ptr<content::BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BackgroundFetchDelegateProxy::DidStartRequest, io_parent_,
                     guid, std::move(response)));
}

BackgroundFetchDelegateProxy::BackgroundFetchDelegateProxy(
    base::WeakPtr<BackgroundFetchDelegate> delegate)
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

void BackgroundFetchDelegateProxy::StartRequest(
    base::WeakPtr<Controller> job_controller,
    const url::Origin& origin,
    scoped_refptr<BackgroundFetchRequestInfo> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(job_controller);

  std::string guid(base::GenerateGUID());

  controller_map_[guid] = std::make_pair(request, job_controller);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&Core::StartRequest, ui_core_ptr_, guid, origin, request));
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
    const std::string& guid,
    std::unique_ptr<BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const scoped_refptr<BackgroundFetchRequestInfo>& request_info =
      controller_map_[guid].first;
  base::WeakPtr<Controller> job_controller = controller_map_[guid].second;

  request_info->PopulateWithResponse(std::move(response));

  if (job_controller)
    job_controller->DidStartRequest(request_info, guid);
}

void BackgroundFetchDelegateProxy::OnDownloadComplete(
    const std::string& guid,
    std::unique_ptr<BackgroundFetchResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const scoped_refptr<BackgroundFetchRequestInfo>& request_info =
      controller_map_[guid].first;
  base::WeakPtr<Controller> job_controller = controller_map_[guid].second;

  request_info->SetResult(std::move(result));

  if (job_controller)
    job_controller->DidCompleteRequest(request_info);
}

}  // namespace content
