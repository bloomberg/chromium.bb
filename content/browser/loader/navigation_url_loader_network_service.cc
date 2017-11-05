// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_network_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/file_url_loader_factory.h"
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
#include "content/common/navigation_subresource_loader_params.h"
#include "content/common/throttling_url_loader.h"
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
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/common/url_utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_content_disposition.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request_context.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/WebKit/common/mime_util/mime_util.h"

namespace content {

namespace {

// Request ID for browser initiated requests. We start at -2 on the same lines
// as ResourceDispatcherHostImpl.
int g_next_request_id = -2;

// Max number of http redirects to follow.  Same number as the net library.
const int kMaxRedirects = 20;

WebContents* GetWebContentsFromFrameTreeNodeID(int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node)
    return nullptr;

  return WebContentsImpl::FromFrameTreeNode(frame_tree_node);
}

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("navigation_url_loader", R"(
      semantics {
        sender: "Navigation URL Loader"
        description:
          "This request is issued by a main frame navigation to fetch the "
          "content of the page that is being navigated to."
        trigger:
          "Navigating Chrome (by clicking on a link, bookmark, history item, "
          "using session restore, etc)."
        data:
          "Arbitrary site-controlled data can be included in the URL, HTTP "
          "headers, and request body. Requests may include cookies and "
          "site-specific credentials."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "This feature cannot be disabled."
        chrome_policy {
          URLBlacklist {
            URLBlacklist: { entries: '*' }
          }
        }
        chrome_policy {
          URLWhitelist {
            URLWhitelist { }
          }
        }
      }
      comments:
        "Chrome would be unable to navigate to websites without this type of "
        "request. Using either URLBlacklist or URLWhitelist policies (or a "
        "combination of both) limits the scope of these requests."
      )");

}  // namespace

// Kept around during the lifetime of the navigation request, and is
// responsible for dispatching a ResourceRequest to the appropriate
// URLLoader.  In order to get the right URLLoader it builds a vector
// of URLLoaderRequestHandler's and successively calls MaybeCreateLoader
// on each until the request is successfully handled. The same sequence
// may be performed multiple times when redirects happen.
// TODO(michaeln): Expose this class and add more unittests.
class NavigationURLLoaderNetworkService::URLLoaderRequestController
    : public mojom::URLLoaderClient {
 public:
  URLLoaderRequestController(
      std::vector<std::unique_ptr<URLLoaderRequestHandler>> initial_handlers,
      std::unique_ptr<ResourceRequest> resource_request,
      ResourceContext* resource_context,
      scoped_refptr<URLLoaderFactoryGetter> default_url_loader_factory_getter,
      const base::WeakPtr<NavigationURLLoaderNetworkService>& owner)
      : handlers_(std::move(initial_handlers)),
        resource_request_(std::move(resource_request)),
        resource_context_(resource_context),
        default_url_loader_factory_getter_(default_url_loader_factory_getter),
        owner_(owner),
        response_loader_binding_(this) {}

  ~URLLoaderRequestController() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  }

  void Start(
      ServiceWorkerNavigationHandleCore* service_worker_navigation_handle_core,
      AppCacheNavigationHandleCore* appcache_handle_core,
      std::unique_ptr<NavigationRequestInfo> request_info,
      mojom::URLLoaderFactoryPtrInfo factory_for_webui,
      mojom::URLLoaderFactoryPtrInfo subresource_factory_for_webui,
      int frame_tree_node_id,
      std::unique_ptr<service_manager::Connector> connector) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(!started_);
    frame_tree_node_id_ = frame_tree_node_id;
    started_ = true;
    web_contents_getter_ =
        base::Bind(&GetWebContentsFromFrameTreeNodeID, frame_tree_node_id);
    const ResourceType resource_type = request_info->is_main_frame
                                           ? RESOURCE_TYPE_MAIN_FRAME
                                           : RESOURCE_TYPE_SUB_FRAME;

    if (resource_request_->request_body) {
      GetBodyBlobDataHandles(resource_request_->request_body.get(),
                             resource_context_, &blob_handles_);
    }

    // Requests to WebUI scheme won't get redirected to/from other schemes
    // or be intercepted, so we just let it go here.
    if (factory_for_webui.is_valid()) {
      webui_factory_ptr_.Bind(std::move(factory_for_webui));
      url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
          webui_factory_ptr_.get(),
          GetContentClient()->browser()->CreateURLLoaderThrottles(
              web_contents_getter_),
          frame_tree_node_id_, 0 /* request_id? */, mojom::kURLLoadOptionNone,
          *resource_request_, this, kTrafficAnnotation);
      SubresourceLoaderParams params;
      params.loader_factory_info = std::move(subresource_factory_for_webui);
      subresource_loader_params_ = std::move(params);
      return;
    }

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
              request_info->common_params.post_data, web_contents_getter_);
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

  // This could be called multiple times to follow a chain of redirects.
  void Restart() {
    // Clear |url_loader_| if it's not the default one (network). This allows
    // the restarted request to use a new loader, instead of, e.g., reusing the
    // AppCache or service worker loader. For an optimization, we keep and reuse
    // the default url loader if the all |handlers_| doesn't handle the
    // redirected request.
    if (!default_loader_used_)
      url_loader_.reset();
    handler_index_ = 0;
    received_response_ = false;
    MaybeStartLoader(nullptr /* handler */, StartLoaderCallback());
  }

  // |handler| is the one who called this method (as a LoaderCallback), nullptr
  // if this method is not called by a handler.
  // |start_loader_callback| is the callback given by the |handler|, non-null
  // if the |handler| wants to handle the request.
  void MaybeStartLoader(URLLoaderRequestHandler* handler,
                        StartLoaderCallback start_loader_callback) {
    if (start_loader_callback) {
      // |handler| wants to handle the request.
      DCHECK(handler);
      default_loader_used_ = false;
      url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
          std::move(start_loader_callback),
          GetContentClient()->browser()->CreateURLLoaderThrottles(
              web_contents_getter_),
          frame_tree_node_id_, *resource_request_, this, kTrafficAnnotation);

      subresource_loader_params_ =
          handler->MaybeCreateSubresourceLoaderParams();

      return;
    }

    // Before falling back to the next handler, see if |handler| still wants
    // to give additional info to the frame for subresource loading.
    // In that case we will just fall back to the default loader (i.e.
    // won't go on to the next handlers) but send the subresource_loader_params
    // to the child process. This is necessary for correctness in the cases
    // where, e.g. there's a controlling ServiceWorker that doesn't handle main
    // resource loading, but may still want to control the page and/or handle
    // subresource loading. In that case we want to skip APpCache.
    if (handler) {
      subresource_loader_params_ =
          handler->MaybeCreateSubresourceLoaderParams();

      // If non-null |subresource_loader_params_| is returned, make sure
      // we skip the next handlers.
      if (subresource_loader_params_)
        handler_index_ = handlers_.size();
    }

    // See if the next handler wants to handle the request.
    if (handler_index_ < handlers_.size()) {
      DCHECK(!subresource_loader_params_);
      auto* next_handler = handlers_[handler_index_++].get();
      next_handler->MaybeCreateLoader(
          *resource_request_, resource_context_,
          base::BindOnce(&URLLoaderRequestController::MaybeStartLoader,
                         base::Unretained(this), next_handler));
      return;
    }

    if (url_loader_) {
      DCHECK(!redirect_info_.new_url.is_empty());
      url_loader_->FollowRedirect();
      return;
    }

    mojom::URLLoaderFactory* factory = nullptr;
    DCHECK_EQ(handlers_.size(), handler_index_);
    if (resource_request_->url.SchemeIs(url::kBlobScheme)) {
      factory = default_url_loader_factory_getter_->GetBlobFactory()->get();
    } else if (!IsURLHandledByNetworkService(resource_request_->url) &&
               !resource_request_->url.SchemeIs(url::kDataScheme)) {
      mojom::URLLoaderFactoryPtr& non_network_factory =
          non_network_url_loader_factories_[resource_request_->url.scheme()];
      if (!non_network_factory.is_bound()) {
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::BindOnce(&NavigationURLLoaderNetworkService ::
                               BindNonNetworkURLLoaderFactoryRequest,
                           owner_, resource_request_->url,
                           mojo::MakeRequest(&non_network_factory)));
      }
      factory = non_network_factory.get();
    } else {
      factory = default_url_loader_factory_getter_->GetNetworkFactory()->get();
      default_loader_used_ = true;
    }
    url_chain_.push_back(resource_request_->url);
    url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
        factory,
        GetContentClient()->browser()->CreateURLLoaderThrottles(
            web_contents_getter_),
        frame_tree_node_id_, 0 /* request_id? */,
        mojom::kURLLoadOptionSendSSLInfo | mojom::kURLLoadOptionSniffMimeType,
        *resource_request_, this, kTrafficAnnotation);
  }

  void FollowRedirect() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(url_loader_);
    DCHECK(!response_url_loader_);
    DCHECK(!redirect_info_.new_url.is_empty());

    // Update resource_request_ and call Restart to give our handlers_ a chance
    // at handling the new location. If no handler wants to take over, we'll
    // use the existing url_loader to follow the redirect, see MaybeStartLoader.
    // TODO(michaeln): This is still WIP and is based on URLRequest::Redirect,
    // there likely remains more to be done.
    // a. For subframe navigations, the Origin header may need to be modified
    //    differently?
    // b. How should redirect_info_.referred_token_binding_host be handled?

    bool should_clear_upload = false;
    net::RedirectUtil::UpdateHttpRequest(
        resource_request_->url, resource_request_->method, redirect_info_,
        &resource_request_->headers, &should_clear_upload);
    if (should_clear_upload) {
      // The request body is no longer applicable.
      resource_request_->request_body = nullptr;
      blob_handles_.clear();
    }

    resource_request_->url = redirect_info_.new_url;
    resource_request_->method = redirect_info_.new_method;
    resource_request_->site_for_cookies = redirect_info_.new_site_for_cookies;
    resource_request_->referrer = GURL(redirect_info_.new_referrer);
    resource_request_->referrer_policy =
        Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
            redirect_info_.new_referrer_policy);
    url_chain_.push_back(redirect_info_.new_url);

    Restart();
  }

  // Navigation is intercepted, transfer the |resource_request_|, |url_loader_|
  // and the |completion_status_| to the new owner. The new owner is
  // responsible for handling all the mojom::URLLoaderClient callbacks from now
  // on.
  void InterceptNavigation(
      NavigationURLLoader::NavigationInterceptionCB callback) {
    std::move(callback).Run(std::move(resource_request_),
                            std::move(url_loader_),
                            std::move(url_chain_),
                            std::move(completion_status_));
  }

  base::Optional<SubresourceLoaderParams> TakeSubresourceLoaderParams() {
    return std::move(subresource_loader_params_);
  }

 private:
  // mojom::URLLoaderClient implementation:
  void OnReceiveResponse(
      const ResourceResponseHead& head,
      const base::Optional<net::SSLInfo>& ssl_info,
      mojom::DownloadedTempFilePtr downloaded_file) override {
    received_response_ = true;
    // If the default loader (network) was used to handle the URL load request
    // we need to see if the handlers want to potentially create a new loader
    // for the response. e.g. AppCache.
    if (MaybeCreateLoaderForResponse(head))
      return;
    scoped_refptr<ResourceResponse> response(new ResourceResponse());
    response->head = head;

    // Make a copy of the ResourceResponse before it is passed to another
    // thread.
    //
    // TODO(davidben): This copy could be avoided if ResourceResponse weren't
    // reference counted and the loader stack passed unique ownership of the
    // response. https://crbug.com/416050
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&NavigationURLLoaderNetworkService::OnReceiveResponse,
                       owner_, response->DeepCopy(), ssl_info,
                       base::Passed(&downloaded_file)));
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& head) override {
    if (--redirect_limit_ == 0) {
      OnComplete(ResourceRequestCompletionStatus(net::ERR_TOO_MANY_REDIRECTS));
      return;
    }

    // Store the redirect_info for later use in FollowRedirect where we give
    // our handlers_ a chance to intercept the request for the new location.
    redirect_info_ = redirect_info;

    scoped_refptr<ResourceResponse> response(new ResourceResponse());
    response->head = head;

    // Make a copy of the ResourceResponse before it is passed to another
    // thread.
    //
    // TODO(davidben): This copy could be avoided if ResourceResponse weren't
    // reference counted and the loader stack passed unique ownership of the
    // response. https://crbug.com/416050
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&NavigationURLLoaderNetworkService::OnReceiveRedirect,
                       owner_, redirect_info, response->DeepCopy()));
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
        base::BindOnce(
            &NavigationURLLoaderNetworkService::OnStartLoadingResponseBody,
            owner_, base::Passed(&body)));
  }

  void OnComplete(
      const ResourceRequestCompletionStatus& completion_status) override {
    if (completion_status.error_code != net::OK && !received_response_) {
      // If the default loader (network) was used to handle the URL load
      // request we need to see if the handlers want to potentially create a
      // new loader for the response. e.g. AppCache.
      if (MaybeCreateLoaderForResponse(ResourceResponseHead()))
        return;
    }
    completion_status_ = completion_status;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&NavigationURLLoaderNetworkService::OnComplete, owner_,
                       completion_status));
  }

  // Returns true if a handler wants to handle the response, i.e. return a
  // different response. For e.g. AppCache may have fallback content.
  bool MaybeCreateLoaderForResponse(const ResourceResponseHead& response) {
    if (!default_loader_used_)
      return false;

    for (auto& handler : handlers_) {
      mojom::URLLoaderClientRequest response_client_request;
      if (handler->MaybeCreateLoaderForResponse(response, &response_url_loader_,
                                                &response_client_request)) {
        response_loader_binding_.Bind(std::move(response_client_request));
        default_loader_used_ = false;
        url_loader_.reset();
        return true;
      }
    }
    return false;
  }

  std::vector<std::unique_ptr<URLLoaderRequestHandler>> handlers_;
  size_t handler_index_ = 0;

  std::unique_ptr<ResourceRequest> resource_request_;
  int frame_tree_node_id_ = 0;
  net::RedirectInfo redirect_info_;
  int redirect_limit_ = kMaxRedirects;
  ResourceContext* resource_context_;
  base::Callback<WebContents*()> web_contents_getter_;
  scoped_refptr<URLLoaderFactoryGetter> default_url_loader_factory_getter_;
  mojom::URLLoaderFactoryPtr webui_factory_ptr_;
  std::unique_ptr<ThrottlingURLLoader> url_loader_;
  BlobHandles blob_handles_;
  std::vector<GURL> url_chain_;

  // Currently used by the AppCache loader to pass its factory to the
  // renderer which enables it to handle subresources.
  base::Optional<SubresourceLoaderParams> subresource_loader_params_;

  // This is referenced only on the UI thread.
  base::WeakPtr<NavigationURLLoaderNetworkService> owner_;

  // Set to true if the default URLLoader (network service) was used for the
  // current navigation.
  bool default_loader_used_ = false;

  // URLLoaderClient binding for loaders created for responses received from the
  // network loader.
  mojo::Binding<mojom::URLLoaderClient> response_loader_binding_;

  // URLLoader instance for response loaders, i.e loaders created for handing
  // responses received from the network URLLoader.
  mojom::URLLoaderPtr response_url_loader_;

  // Set to true if we receive a valid response from a URLLoader, i.e.
  // URLLoaderClient::OnReceivedResponse() is called.
  bool received_response_ = false;

  bool started_ = false;

  // Lazily initialized and used in the case of non-network resource
  // navigations. Keyed by URL scheme.
  std::map<std::string, mojom::URLLoaderFactoryPtr>
      non_network_url_loader_factories_;

  // The completion status if it has been received. This is needed to handle
  // the case that the response is intercepted by download, and OnComplete() is
  // already called while we are transferring the |url_loader_| and response
  // body to download code.
  base::Optional<ResourceRequestCompletionStatus> completion_status_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderRequestController);
};

NavigationURLLoaderNetworkService::NavigationURLLoaderNetworkService(
    ResourceContext* resource_context,
    StoragePartition* storage_partition,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    ServiceWorkerNavigationHandle* service_worker_navigation_handle,
    AppCacheNavigationHandle* appcache_handle,
    NavigationURLLoaderDelegate* delegate,
    std::vector<std::unique_ptr<URLLoaderRequestHandler>> initial_handlers)
    : delegate_(delegate),
      allow_download_(request_info->common_params.allow_download),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "navigation", "Navigation timeToResponseStarted", this,
      request_info->common_params.navigation_start, "FrameTreeNode id",
      request_info->frame_tree_node_id);

  // TODO(scottmg): Port over stuff from RDHI::BeginNavigationRequest() here.
  auto new_request = std::make_unique<ResourceRequest>();

  new_request->method = request_info->common_params.method;
  new_request->url = request_info->common_params.url;
  new_request->site_for_cookies = request_info->site_for_cookies;
  new_request->priority = net::HIGHEST;

  // The code below to set fields like request_initiator, referrer, etc has
  // been copied from ResourceDispatcherHostImpl. We did not refactor the
  // common code into a function, because RDHI uses accessor functions on the
  // URLRequest class to set these fields. whereas we use ResourceRequest here.
  new_request->request_initiator = request_info->begin_params.initiator_origin;
  new_request->referrer = request_info->common_params.referrer.url;
  new_request->referrer_policy = request_info->common_params.referrer.policy;
  new_request->headers.AddHeadersFromString(request_info->begin_params.headers);

  new_request->resource_type = request_info->is_main_frame
                                   ? RESOURCE_TYPE_MAIN_FRAME
                                   : RESOURCE_TYPE_SUB_FRAME;

  int load_flags = request_info->begin_params.load_flags;
  load_flags |= net::LOAD_VERIFY_EV_CERT;
  if (request_info->is_main_frame)
    load_flags |= net::LOAD_MAIN_FRAME_DEPRECATED;

  // Sync loads should have maximum priority and should be the only
  // requests that have the ignore limits flag set.
  DCHECK(!(load_flags & net::LOAD_IGNORE_LIMITS));

  new_request->load_flags = load_flags;

  new_request->request_body = request_info->common_params.post_data.get();
  new_request->report_raw_headers = request_info->report_raw_headers;
  new_request->allow_download = allow_download_;

  new_request->fetch_request_mode = network::mojom::FetchRequestMode::kNavigate;
  new_request->fetch_credentials_mode =
      network::mojom::FetchCredentialsMode::kInclude;
  new_request->fetch_redirect_mode = FetchRedirectMode::MANUAL_MODE;

  int frame_tree_node_id = request_info->frame_tree_node_id;

  // Check if a web UI scheme wants to handle this request.
  mojom::URLLoaderFactoryPtrInfo factory_for_webui;
  mojom::URLLoaderFactoryPtrInfo subresource_factory_for_webui;
  const auto& schemes = URLDataManagerBackend::GetWebUISchemes();
  if (std::find(schemes.begin(), schemes.end(), new_request->url.scheme()) !=
      schemes.end()) {
    FrameTreeNode* frame_tree_node =
        FrameTreeNode::GloballyFindByID(frame_tree_node_id);
    factory_for_webui = CreateWebUIURLLoader(frame_tree_node).PassInterface();
    subresource_factory_for_webui =
        CreateWebUIURLLoader(frame_tree_node).PassInterface();
  }

  g_next_request_id--;

  auto* partition = static_cast<StoragePartitionImpl*>(storage_partition);
  DCHECK(!request_controller_);
  request_controller_ = std::make_unique<URLLoaderRequestController>(
      std::move(initial_handlers), std::move(new_request), resource_context,
      partition->url_loader_factory_getter(), weak_factory_.GetWeakPtr());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&URLLoaderRequestController::Start,
                     base::Unretained(request_controller_.get()),
                     service_worker_navigation_handle
                         ? service_worker_navigation_handle->core()
                         : nullptr,
                     appcache_handle ? appcache_handle->core() : nullptr,
                     base::Passed(std::move(request_info)),
                     base::Passed(std::move(factory_for_webui)),
                     base::Passed(std::move(subresource_factory_for_webui)),
                     frame_tree_node_id,
                     base::Passed(ServiceManagerConnection::GetForProcess()
                                      ->GetConnector()
                                      ->Clone())));

  non_network_url_loader_factories_[url::kFileScheme] =
      std::make_unique<FileURLLoaderFactory>(
          partition->browser_context()->GetPath(),
          base::CreateSequencedTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::BACKGROUND,
               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
}

NavigationURLLoaderNetworkService::~NavigationURLLoaderNetworkService() {
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE,
                            request_controller_.release());
}

void NavigationURLLoaderNetworkService::FollowRedirect() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&URLLoaderRequestController::FollowRedirect,
                     base::Unretained(request_controller_.get())));
}

void NavigationURLLoaderNetworkService::ProceedWithResponse() {}

void NavigationURLLoaderNetworkService::InterceptNavigation(
    NavigationURLLoader::NavigationInterceptionCB callback) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&URLLoaderRequestController::InterceptNavigation,
                     base::Unretained(request_controller_.get()),
                     std::move(callback)));
}

void NavigationURLLoaderNetworkService::OnReceiveResponse(
    scoped_refptr<ResourceResponse> response,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  // TODO(scottmg): This needs to do more of what
  // NavigationResourceHandler::OnResponseStarted() does. Or maybe in
  // OnStartLoadingResponseBody().
  if (ssl_info && ssl_info->cert)
    NavigationResourceHandler::GetSSLStatusForRequest(*ssl_info, &ssl_status_);
  response_ = std::move(response);
  ssl_info_ = ssl_info;
}

void NavigationURLLoaderNetworkService::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    scoped_refptr<ResourceResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnRequestRedirected(redirect_info, std::move(response));
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
      IsDownload(), false /* is_stream */,
      request_controller_->TakeSubresourceLoaderParams());
}

void NavigationURLLoaderNetworkService::OnComplete(
    const ResourceRequestCompletionStatus& completion_status) {
  if (completion_status.error_code == net::OK)
    return;

  TRACE_EVENT_ASYNC_END2("navigation", "Navigation timeToResponseStarted", this,
                         "&NavigationURLLoaderNetworkService", this, "success",
                         false);

  // TODO(https://crbug.com/757633): Pass real values in the case of cert
  // errors.
  bool should_ssl_errors_be_fatal = true;

  delegate_->OnRequestFailed(completion_status.exists_in_cache,
                             completion_status.error_code, ssl_info_,
                             should_ssl_errors_be_fatal);
}

bool NavigationURLLoaderNetworkService::IsDownload() const {
  DCHECK(response_);

  if (!allow_download_)
    return false;

  if (response_->head.headers) {
    std::string disposition;
    if (response_->head.headers->GetNormalizedHeader("content-disposition",
                                                     &disposition) &&
        !disposition.empty() &&
        net::HttpContentDisposition(disposition, std::string())
            .is_attachment()) {
      return true;
    }
    // TODO(qinmin): Check whether this is special-case user script that needs
    // to be downloaded.
  }

  if (blink::IsSupportedMimeType(response_->head.mime_type))
    return false;

  // TODO(qinmin): Check whether there is a plugin handler.

  return (!response_->head.headers ||
          response_->head.headers->response_code() / 100 == 2);
}

void NavigationURLLoaderNetworkService::BindNonNetworkURLLoaderFactoryRequest(
    const GURL& url,
    mojom::URLLoaderFactoryRequest factory) {
  auto it = non_network_url_loader_factories_.find(url.scheme());
  if (it == non_network_url_loader_factories_.end()) {
    DVLOG(1) << "Ignoring request with unknown scheme: " << url.spec();
    return;
  }
  it->second->Clone(std::move(factory));
}

}  // namespace content
