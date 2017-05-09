// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_network_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_resource_handler.h"
#include "content/browser/loader/navigation_resource_throttle.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_url_loader_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/referrer.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

static base::LazyInstance<mojom::URLLoaderFactoryPtr>::Leaky
    g_url_loader_factory = LAZY_INSTANCE_INITIALIZER;

// This function is called on the IO thread for POST/PUT requests for
// attaching blob information to the request body.
void HandleRequestsWithBody(
    std::unique_ptr<ResourceRequest> request,
    ResourceContext* resource_context,
    base::WeakPtr<NavigationURLLoaderNetworkService> url_loader) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request->request_body.get());

  AttachRequestBodyBlobDataHandles(request->request_body.get(),
                                    resource_context);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NavigationURLLoaderNetworkService::StartURLRequest,
                  url_loader, base::Passed(&request)));
}

}  // namespace

NavigationURLLoaderNetworkService::NavigationURLLoaderNetworkService(
    ResourceContext* resource_context,
    StoragePartition* storage_partition,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    ServiceWorkerNavigationHandle* service_worker_handle,
    AppCacheNavigationHandle* appcache_handle,
    NavigationURLLoaderDelegate* delegate)
    : delegate_(delegate),
      binding_(this),
      request_info_(std::move(request_info)),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "navigation", "Navigation timeToResponseStarted", this,
      request_info_->common_params.navigation_start, "FrameTreeNode id",
      request_info_->frame_tree_node_id);

  // TODO(scottmg): Port over stuff from RDHI::BeginNavigationRequest() here.
  auto new_request = base::MakeUnique<ResourceRequest>();

  new_request->method = request_info_->common_params.method;
  new_request->url = request_info_->common_params.url;
  new_request->first_party_for_cookies = request_info_->first_party_for_cookies;
  new_request->priority = net::HIGHEST;

  // The code below to set fields like request_initiator, referrer, etc has
  // been copied from ResourceDispatcherHostImpl. We did not refactor the
  // common code into a function, because RDHI uses accessor functions on the
  // URLRequest class to set these fields. whereas we use ResourceRequest here.
  new_request->request_initiator = request_info_->begin_params.initiator_origin;
  new_request->referrer = request_info_->common_params.referrer.url;
  new_request->referrer_policy = request_info_->common_params.referrer.policy;
  new_request->headers = request_info_->begin_params.headers;

  int load_flags = request_info_->begin_params.load_flags;
  load_flags |= net::LOAD_VERIFY_EV_CERT;
  if (request_info_->is_main_frame)
    load_flags |= net::LOAD_MAIN_FRAME_DEPRECATED;

  // Sync loads should have maximum priority and should be the only
  // requests that have the ignore limits flag set.
  DCHECK(!(load_flags & net::LOAD_IGNORE_LIMITS));

  new_request->load_flags = load_flags;

  new_request->request_body = request_info_->common_params.post_data.get();
  if (new_request->request_body.get()) {
    // The request body may need blob handles to be added to it. This
    // functionality has to be invoked on the IO thread.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&HandleRequestsWithBody,
                   base::Passed(&new_request),
                   resource_context,
                   weak_factory_.GetWeakPtr()));
    return;
  }

  StartURLRequest(std::move(new_request));
}

NavigationURLLoaderNetworkService::~NavigationURLLoaderNetworkService() {}

void NavigationURLLoaderNetworkService::OverrideURLLoaderFactoryForTesting(
    mojom::URLLoaderFactoryPtr url_loader_factory) {
  g_url_loader_factory.Get() = std::move(url_loader_factory);
}

void NavigationURLLoaderNetworkService::FollowRedirect() {
  url_loader_associated_ptr_->FollowRedirect();
}

void NavigationURLLoaderNetworkService::ProceedWithResponse() {}

void NavigationURLLoaderNetworkService::OnReceiveResponse(
    const ResourceResponseHead& head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  // TODO(scottmg): This needs to do more of what
  // NavigationResourceHandler::OnReponseStarted() does. Or maybe in
  // OnStartLoadingResponseBody().
  if (ssl_info && ssl_info->cert)
    NavigationResourceHandler::GetSSLStatusForRequest(*ssl_info, &ssl_status_);
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
    OnUploadProgressCallback callback) {}

void NavigationURLLoaderNetworkService::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {}

void NavigationURLLoaderNetworkService::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {}

void NavigationURLLoaderNetworkService::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(response_);

  TRACE_EVENT_ASYNC_END2("navigation", "Navigation timeToResponseStarted", this,
                         "&NavigationURLLoaderNetworkService", this, "success",
                         true);

  // Temporarily, we pass both a stream (null) and the data pipe to the
  // delegate until PlzNavigate has shipped and we can be comfortable fully
  // switching to the data pipe.
  delegate_->OnResponseStarted(response_, nullptr, std::move(body), ssl_status_,
                               std::unique_ptr<NavigationData>(),
                               GlobalRequestID() /* request_id? */,
                               false /* is_download? */, false /* is_stream */);
}

void NavigationURLLoaderNetworkService::OnComplete(
    const ResourceRequestCompletionStatus& completion_status) {
  if (completion_status.error_code != net::OK) {
    TRACE_EVENT_ASYNC_END2("navigation", "Navigation timeToResponseStarted",
                           this, "&NavigationURLLoaderNetworkService", this,
                           "success", false);

    delegate_->OnRequestFailed(completion_status.exists_in_cache,
                               completion_status.error_code);
  }
}

void NavigationURLLoaderNetworkService::StartURLRequest(
    std::unique_ptr<ResourceRequest> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  mojom::URLLoaderClientPtr url_loader_client_ptr_to_pass;
  binding_.Bind(&url_loader_client_ptr_to_pass);

  mojom::URLLoaderFactory* factory = nullptr;
  // This |factory_ptr| will be destroyed when it goes out of scope. Because
  // |url_loader_associated_ptr_| is associated with it, it will be disconnected
  // as well. That means NavigationURLLoaderNetworkService::FollowRedirect()
  // won't work as expected, the |url_loader_associated_ptr_| will silently drop
  // calls.
  // This is fine for now since the only user of this is WebUI which doesn't
  // need this, but we'll have to fix this when other consumers come up.
  mojom::URLLoaderFactoryPtr factory_ptr;
  const auto& schemes = URLDataManagerBackend::GetWebUISchemes();
  if (std::find(schemes.begin(), schemes.end(), request->url.scheme()) !=
      schemes.end()) {
    FrameTreeNode* frame_tree_node =
        FrameTreeNode::GloballyFindByID(request_info_->frame_tree_node_id);
    factory_ptr = GetWebUIURLLoader(frame_tree_node);
    factory = factory_ptr.get();
  }

  if (!factory)
    factory = GetURLLoaderFactory();

  factory->CreateLoaderAndStart(mojo::MakeRequest(&url_loader_associated_ptr_),
                                0 /* routing_id? */, 0 /* request_id? */,
                                mojom::kURLLoadOptionSendSSLInfo, *request,
                                std::move(url_loader_client_ptr_to_pass));
}

mojom::URLLoaderFactory*
NavigationURLLoaderNetworkService::GetURLLoaderFactory() {
  // TODO(yzshen): We will need the ability to customize the factory per frame
  // e.g., for appcache or service worker.
  if (!g_url_loader_factory.Get()) {
    ServiceManagerConnection::GetForProcess()->GetConnector()->BindInterface(
        mojom::kNetworkServiceName, &g_url_loader_factory.Get());
  }

  return g_url_loader_factory.Get().get();
}

}  // namespace content
