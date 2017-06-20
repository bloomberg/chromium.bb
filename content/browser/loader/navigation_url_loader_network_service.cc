// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_network_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_resource_handler.h"
#include "content/browser/loader/navigation_resource_throttle.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_url_loader_factory.h"
#include "content/common/throttling_url_loader.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

// Request ID for browser initiated requests. We start at -2 on the same lines
// as ResourceDispatcherHostImpl.
int g_next_request_id = -2;

WebContents* GetWebContentsFromFrameTreeNodeID(int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node)
    return nullptr;

  return WebContentsImpl::FromFrameTreeNode(frame_tree_node);
}

}  // namespace

// Kept around during the lifetime of the navigation request, and is
// responsible for dispatching a ResourceRequest to the appropriate
// URLLoader.  In order to get the right URLLoader it builds a vector
// of URLLoaderRequestHandler's and successively calls MaybeCreateLoader
// on each until the request is successfully handled. The same sequence
// may be performed multiple times when redirects happen.
class NavigationURLLoaderNetworkService::URLLoaderRequestController
    : public mojom::URLLoaderClient {
 public:
  URLLoaderRequestController(
      std::unique_ptr<ResourceRequest> resource_request,
      ResourceContext* resource_context,
      scoped_refptr<URLLoaderFactoryGetter> default_url_loader_factory_getter,
      const base::WeakPtr<NavigationURLLoaderNetworkService>& owner)
      : resource_request_(std::move(resource_request)),
        resource_context_(resource_context),
        default_url_loader_factory_getter_(default_url_loader_factory_getter),
        owner_(owner) {}

  ~URLLoaderRequestController() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  }

  void Start(
      ServiceWorkerNavigationHandleCore* service_worker_navigation_handle_core,
      AppCacheNavigationHandleCore* appcache_handle_core,
      std::unique_ptr<NavigationRequestInfo> request_info,
      mojom::URLLoaderFactoryPtrInfo factory_for_webui,
      const base::Callback<WebContents*(void)>& web_contents_getter,
      std::unique_ptr<service_manager::Connector> connector) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    web_contents_getter_ = web_contents_getter;
    const ResourceType resource_type = request_info->is_main_frame
                                           ? RESOURCE_TYPE_MAIN_FRAME
                                           : RESOURCE_TYPE_SUB_FRAME;

    if (resource_request_->request_body) {
      AttachRequestBodyBlobDataHandles(resource_request_->request_body.get(),
                                       resource_context_);
    }

    // Requests to WebUI scheme won't get redirected to/from other schemes
    // or be intercepted, so we just let it go here.
    if (factory_for_webui.is_valid()) {
      webui_factory_ptr_.Bind(std::move(factory_for_webui));
      url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
          webui_factory_ptr_.get(),
          GetContentClient()->browser()->CreateURLLoaderThrottles(
              web_contents_getter_),
          0 /* routing_id? */, 0 /* request_id? */,
          mojom::kURLLoadOptionSendSSLInfo, *resource_request_, this,
          net::MutableNetworkTrafficAnnotationTag(NO_TRAFFIC_ANNOTATION_YET));
      return;
    }

    DCHECK(handlers_.empty());
    if (service_worker_navigation_handle_core) {
      RequestContextFrameType frame_type =
          request_info->is_main_frame ? REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL
                                      : REQUEST_CONTEXT_FRAME_TYPE_NESTED;

      storage::BlobStorageContext* blob_storage_context = GetBlobStorageContext(
          GetChromeBlobStorageContextForResourceContext(resource_context_));
      std::unique_ptr<URLLoaderRequestHandler> service_worker_handler =
          ServiceWorkerRequestHandler::InitializeForNavigationNetworkService(
              *resource_request_, resource_context_,
              service_worker_navigation_handle_core, blob_storage_context,
              request_info->begin_params.skip_service_worker, resource_type,
              request_info->begin_params.request_context_type, frame_type,
              request_info->are_ancestors_secure,
              request_info->common_params.post_data, web_contents_getter);
      if (service_worker_handler)
        handlers_.push_back(std::move(service_worker_handler));
    }

    if (appcache_handle_core) {
      std::unique_ptr<URLLoaderRequestHandler> appcache_handler =
          AppCacheRequestHandler::InitializeForNavigationNetworkService(
              *resource_request_, appcache_handle_core,
              default_url_loader_factory_getter_.get());
      if (appcache_handler)
        handlers_.push_back(std::move(appcache_handler));
    }

    Restart();
  }

  // This could be called multiple times.
  void Restart() {
    handler_index_ = 0;
    MaybeStartLoader(StartLoaderCallback());
  }

  void MaybeStartLoader(StartLoaderCallback start_loader_callback) {
    if (start_loader_callback) {
      url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
          std::move(start_loader_callback),
          GetContentClient()->browser()->CreateURLLoaderThrottles(
              web_contents_getter_),
          *resource_request_, this);

      DCHECK_GT(handler_index_, 0U);

      mojom::URLLoaderFactoryPtr subresource_loader_factory =
          handlers_[handler_index_ - 1]->MaybeCreateSubresourceFactory();
      if (subresource_loader_factory.get()) {
        subresource_url_loader_factory_ptr_info_ =
            subresource_loader_factory.PassInterface();
      }
      return;
    }

    if (handler_index_ < handlers_.size()) {
      handlers_[handler_index_++]->MaybeCreateLoader(
          *resource_request_, resource_context_,
          base::BindOnce(&URLLoaderRequestController::MaybeStartLoader,
                         base::Unretained(this)));
      return;
    }

    mojom::URLLoaderFactory* factory = nullptr;
    DCHECK_EQ(handlers_.size(), handler_index_);
    if (resource_request_->url.SchemeIs(url::kBlobScheme)) {
      factory = default_url_loader_factory_getter_->GetBlobFactory()->get();
    } else {
      factory = default_url_loader_factory_getter_->GetNetworkFactory()->get();
    }
    url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
        factory,
        GetContentClient()->browser()->CreateURLLoaderThrottles(
            web_contents_getter_),
        0 /* routing_id? */, 0 /* request_id? */,
        mojom::kURLLoadOptionSendSSLInfo, *resource_request_, this,
        net::MutableNetworkTrafficAnnotationTag(NO_TRAFFIC_ANNOTATION_YET));
  }

  void FollowRedirect() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(url_loader_);

    url_loader_->FollowRedirect();
  }

  // Ownership of the URLLoaderFactoryPtrInfo instance is transferred to the
  // caller.
  mojom::URLLoaderFactoryPtrInfo GetSubresourceURLLoaderFactory() {
    return std::move(subresource_url_loader_factory_ptr_info_);
  }

 private:
  // mojom::URLLoaderClient implementation:
  void OnReceiveResponse(
      const ResourceResponseHead& head,
      const base::Optional<net::SSLInfo>& ssl_info,
      mojom::DownloadedTempFilePtr downloaded_file) override {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&NavigationURLLoaderNetworkService::OnReceiveResponse,
                   owner_, head, ssl_info, base::Passed(&downloaded_file)));
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& head) override {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&NavigationURLLoaderNetworkService::OnReceiveRedirect,
                   owner_, redirect_info, head));
  }

  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override {}

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {}

  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {}

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &NavigationURLLoaderNetworkService::OnStartLoadingResponseBody,
            owner_, base::Passed(&body)));
  }

  void OnComplete(
      const ResourceRequestCompletionStatus& completion_status) override {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&NavigationURLLoaderNetworkService::OnComplete, owner_,
                   completion_status));
  }

  std::vector<std::unique_ptr<URLLoaderRequestHandler>> handlers_;
  size_t handler_index_ = 0;

  std::unique_ptr<ResourceRequest> resource_request_;
  ResourceContext* resource_context_;
  base::Callback<WebContents*()> web_contents_getter_;

  scoped_refptr<URLLoaderFactoryGetter> default_url_loader_factory_getter_;

  mojom::URLLoaderFactoryPtr webui_factory_ptr_;

  std::unique_ptr<ThrottlingURLLoader> url_loader_;

  // This is referenced only on the UI thread.
  base::WeakPtr<NavigationURLLoaderNetworkService> owner_;

  // Currently used by the AppCache loader to pass its factory to the
  // renderer which enables it to handle subresources.
  mojom::URLLoaderFactoryPtrInfo subresource_url_loader_factory_ptr_info_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderRequestController);
};

NavigationURLLoaderNetworkService::NavigationURLLoaderNetworkService(
    ResourceContext* resource_context,
    StoragePartition* storage_partition,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    ServiceWorkerNavigationHandle* service_worker_navigation_handle,
    AppCacheNavigationHandle* appcache_handle,
    NavigationURLLoaderDelegate* delegate)
    : delegate_(delegate), weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "navigation", "Navigation timeToResponseStarted", this,
      request_info->common_params.navigation_start, "FrameTreeNode id",
      request_info->frame_tree_node_id);

  // TODO(scottmg): Port over stuff from RDHI::BeginNavigationRequest() here.
  auto new_request = base::MakeUnique<ResourceRequest>();

  new_request->method = request_info->common_params.method;
  new_request->url = request_info->common_params.url;
  new_request->first_party_for_cookies = request_info->first_party_for_cookies;
  new_request->priority = net::HIGHEST;

  // The code below to set fields like request_initiator, referrer, etc has
  // been copied from ResourceDispatcherHostImpl. We did not refactor the
  // common code into a function, because RDHI uses accessor functions on the
  // URLRequest class to set these fields. whereas we use ResourceRequest here.
  new_request->request_initiator = request_info->begin_params.initiator_origin;
  new_request->referrer = request_info->common_params.referrer.url;
  new_request->referrer_policy = request_info->common_params.referrer.policy;
  new_request->headers = request_info->begin_params.headers;

  int load_flags = request_info->begin_params.load_flags;
  load_flags |= net::LOAD_VERIFY_EV_CERT;
  if (request_info->is_main_frame)
    load_flags |= net::LOAD_MAIN_FRAME_DEPRECATED;

  // Sync loads should have maximum priority and should be the only
  // requests that have the ignore limits flag set.
  DCHECK(!(load_flags & net::LOAD_IGNORE_LIMITS));

  new_request->load_flags = load_flags;

  new_request->request_body = request_info->common_params.post_data.get();

  int frame_tree_node_id = request_info->frame_tree_node_id;

  // Check if a web UI scheme wants to handle this request.
  mojom::URLLoaderFactoryPtrInfo factory_for_webui;
  const auto& schemes = URLDataManagerBackend::GetWebUISchemes();
  if (std::find(schemes.begin(), schemes.end(), new_request->url.scheme()) !=
      schemes.end()) {
    FrameTreeNode* frame_tree_node =
        FrameTreeNode::GloballyFindByID(frame_tree_node_id);
    factory_for_webui = CreateWebUIURLLoader(frame_tree_node).PassInterface();
  }

  g_next_request_id--;

  DCHECK(!request_controller_);
  request_controller_ = base::MakeUnique<URLLoaderRequestController>(
      std::move(new_request), resource_context,
      static_cast<StoragePartitionImpl*>(storage_partition)
          ->url_loader_factory_getter(),
      weak_factory_.GetWeakPtr());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &URLLoaderRequestController::Start,
          base::Unretained(request_controller_.get()),
          service_worker_navigation_handle
              ? service_worker_navigation_handle->core()
              : nullptr,
          appcache_handle ? appcache_handle->core() : nullptr,
          base::Passed(std::move(request_info)),
          base::Passed(std::move(factory_for_webui)),
          base::Bind(&GetWebContentsFromFrameTreeNodeID, frame_tree_node_id),
          base::Passed(ServiceManagerConnection::GetForProcess()
                           ->GetConnector()
                           ->Clone())));
}

NavigationURLLoaderNetworkService::~NavigationURLLoaderNetworkService() {
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE,
                            request_controller_.release());
}

void NavigationURLLoaderNetworkService::FollowRedirect() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&URLLoaderRequestController::FollowRedirect,
                 base::Unretained(request_controller_.get())));
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
  response_ = base::MakeRefCounted<ResourceResponse>();
  response_->head = head;
}

void NavigationURLLoaderNetworkService::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& head) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(kinuko): Perform the necessary check and call
  // URLLoaderRequestController::Restart with the new URL??
  scoped_refptr<ResourceResponse> response(new ResourceResponse());
  response->head = head;
  delegate_->OnRequestRedirected(redirect_info, response);
}

void NavigationURLLoaderNetworkService::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(response_);

  TRACE_EVENT_ASYNC_END2("navigation", "Navigation timeToResponseStarted", this,
                         "&NavigationURLLoaderNetworkService", this, "success",
                         true);

  // Temporarily, we pass both a stream (null) and the data pipe to the
  // delegate until PlzNavigate has shipped and we can be comfortable fully
  // switching to the data pipe.
  delegate_->OnResponseStarted(
      response_, nullptr, std::move(body), ssl_status_,
      std::unique_ptr<NavigationData>(), GlobalRequestID(-1, g_next_request_id),
      false /* is_download? */, false /* is_stream */,
      request_controller_->GetSubresourceURLLoaderFactory());
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

}  // namespace content
