// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_network_service.h"

#include "base/memory/ptr_util.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "net/url_request/url_request_context.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

NavigationURLLoaderNetworkService::NavigationURLLoaderNetworkService(
    ResourceContext* resource_context,
    StoragePartition* storage_partition,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    ServiceWorkerNavigationHandle* service_worker_handle,
    AppCacheNavigationHandle* appcache_handle,
    NavigationURLLoaderDelegate* delegate)
    : delegate_(delegate), binding_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(scottmg): Maybe some of this setup should be done only once, instead
  // of every time.
  url_loader_factory_request_ = mojo::MakeRequest(&url_loader_factory_);

  ServiceManagerConnection::GetForProcess()->GetConnector()->BindInterface(
      mojom::kNetworkServiceName, &url_loader_factory_);

  // TODO(scottmg): Port over stuff from RDHI::BeginNavigationRequest() here.
  auto new_request = base::MakeUnique<ResourceRequest>();
  new_request->method = "GET";
  new_request->url = request_info->common_params.url;
  new_request->priority = net::HIGHEST;

  mojom::URLLoaderClientPtr url_loader_client_ptr;
  mojom::URLLoaderClientRequest url_loader_client_request =
      mojo::MakeRequest(&url_loader_client_ptr);
  mojom::URLLoaderClientPtr url_loader_client_ptr_to_pass;
  binding_.Bind(&url_loader_client_ptr_to_pass);

  url_loader_factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&url_loader_associated_ptr_), 0 /* routing_id? */,
      0 /* request_id? */, *new_request,
      std::move(url_loader_client_ptr_to_pass));
}

NavigationURLLoaderNetworkService::~NavigationURLLoaderNetworkService() {}

void NavigationURLLoaderNetworkService::FollowRedirect() {
  url_loader_associated_ptr_->FollowRedirect();
}

void NavigationURLLoaderNetworkService::ProceedWithResponse() {}

void NavigationURLLoaderNetworkService::OnReceiveResponse(
    const ResourceResponseHead& head,
    mojom::DownloadedTempFilePtr downloaded_file) {
  // TODO(scottmg): This needs to do more of what
  // NavigationResourceHandler::OnReponseStarted() does. Or maybe in
  // OnStartLoadingResponseBody().
  response_ = base::MakeShared<ResourceResponse>();
  response_->head = head;
}

void NavigationURLLoaderNetworkService::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& head) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<ResourceResponse> response(new ResourceResponse());
  response->head = head;
  delegate_->OnRequestRedirected(redirect_info, response);
}

void NavigationURLLoaderNetworkService::OnDataDownloaded(
    int64_t data_length,
    int64_t encoded_length) {}

void NavigationURLLoaderNetworkService::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    const OnUploadProgressCallback& callback) {}

void NavigationURLLoaderNetworkService::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {}

void NavigationURLLoaderNetworkService::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {}

void NavigationURLLoaderNetworkService::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(response_);
  // Temporarily, we pass both a stream (null) and the data pipe to the
  // delegate until PlzNavigate has shipped and we can be comfortable fully
  // switching to the data pipe.
  delegate_->OnResponseStarted(response_, nullptr, std::move(body), SSLStatus(),
                               std::unique_ptr<NavigationData>(),
                               GlobalRequestID() /* request_id? */,
                               false /* is_download? */, false /* is_stream */);
}

void NavigationURLLoaderNetworkService::OnComplete(
    const ResourceRequestCompletionStatus& completion_status) {}

}  // namespace content
