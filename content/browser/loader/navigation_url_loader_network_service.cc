// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_network_service.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_scheduler/post_task.h"
#include "base/trace_event/trace_event.h"
#include "components/download/public/common/download_stats.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/file_url_loader_factory.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_resource_handler.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_url_loader_factory_for_non_network_service.h"
#include "content/browser/web_package/web_package_request_handler.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_url_loader_factory_internal.h"
#include "content/common/navigation_subresource_loader_params.h"
#include "content/common/throttling_url_loader.h"
#include "content/common/weak_wrapper_shared_url_loader_factory.h"
#include "content/common/wrapper_shared_url_loader_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_content_disposition.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/WebKit/public/common/mime_util/mime_util.h"

namespace content {

namespace {

bool IsRequestHandlerEnabled() {
  return base::FeatureList::IsEnabled(network::features::kNetworkService) ||
         base::FeatureList::IsEnabled(features::kSignedHTTPExchange);
}

// Request ID for browser initiated requests. We start at -2 on the same lines
// as ResourceDispatcherHostImpl.
int g_next_request_id = -2;
GlobalRequestID MakeGlobalRequestID() {
  return GlobalRequestID(-1, g_next_request_id--);
}

size_t GetCertificateChainsSizeInKB(const net::SSLInfo& ssl_info) {
  base::Pickle cert_pickle;
  ssl_info.cert->Persist(&cert_pickle);
  base::Pickle unverified_cert_pickle;
  ssl_info.unverified_cert->Persist(&unverified_cert_pickle);
  return (cert_pickle.size() + unverified_cert_pickle.size()) / 1000;
}

WebContents* GetWebContentsFromFrameTreeNodeID(int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node)
    return nullptr;

  return WebContentsImpl::FromFrameTreeNode(frame_tree_node);
}

const net::NetworkTrafficAnnotationTag kNavigationUrlLoaderTrafficAnnotation =
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

// TODO(arthursonzogni): IsDownload can't be determined only by the response's
// headers. The response's body might contain information to guess it.
// See MimeSniffingResourceHandler.
bool IsDownload(const network::ResourceResponse& response,
                const GURL& url,
                const std::vector<GURL>& url_chain,
                const base::Optional<url::Origin>& initiator_origin,
                const base::Optional<std::string>& suggested_filename) {
  if (response.head.headers) {
    GURL url_chain_back = url_chain.empty() ? url : url_chain.back();
    bool is_cross_origin =
        (initiator_origin.has_value() && !url_chain_back.SchemeIsBlob() &&
         !url_chain_back.SchemeIsFileSystem() &&
         !url_chain_back.SchemeIs(url::kAboutScheme) &&
         !url_chain_back.SchemeIs(url::kDataScheme) &&
         initiator_origin->GetURL() != url_chain_back.GetOrigin());

    std::string disposition;
    if (response.head.headers->GetNormalizedHeader("content-disposition",
                                                   &disposition) &&
        !disposition.empty() &&
        net::HttpContentDisposition(disposition, std::string())
            .is_attachment()) {
      return true;
    } else if (suggested_filename.has_value() && !is_cross_origin) {
      return true;
    } else if (GetContentClient()->browser()->ShouldForceDownloadResource(
                   url, response.head.mime_type)) {
      return true;
    } else if (response.head.mime_type == "multipart/related" ||
               response.head.mime_type == "message/rfc822") {
      // TODO(https://crbug.com/790734): retrieve the new NavigationUIData from
      // the request and and pass it to AllowRenderingMhtmlOverHttp().
      return !GetContentClient()->browser()->AllowRenderingMhtmlOverHttp(
          nullptr);
    }
    // TODO(qinmin): Check whether this is special-case user script that needs
    // to be downloaded.
  }

  if (blink::IsSupportedMimeType(response.head.mime_type))
    return false;

  // TODO(qinmin): Check whether there is a plugin handler.

  if (suggested_filename.has_value()) {
    download::RecordDownloadCount(
        download::CROSS_ORIGIN_DOWNLOAD_WITHOUT_CONTENT_DISPOSITION);
  }

  return (!response.head.headers ||
          response.head.headers->response_code() / 100 == 2);
}

std::unique_ptr<network::ResourceRequest> CreateResourceRequest(
    NavigationRequestInfo* request_info,
    int frame_tree_node_id,
    bool allow_download) {
  // TODO(scottmg): Port over stuff from RDHI::BeginNavigationRequest() here.
  auto new_request = std::make_unique<network::ResourceRequest>();

  new_request->method = request_info->common_params.method;
  new_request->url = request_info->common_params.url;
  new_request->site_for_cookies = request_info->site_for_cookies;

  net::RequestPriority net_priority = net::HIGHEST;
  if (!request_info->is_main_frame &&
      base::FeatureList::IsEnabled(features::kLowPriorityIframes)) {
    net_priority = net::LOWEST;
  }
  new_request->priority = net_priority;

  new_request->render_frame_id = frame_tree_node_id;

  // The code below to set fields like request_initiator, referrer, etc has
  // been copied from ResourceDispatcherHostImpl. We did not refactor the
  // common code into a function, because RDHI uses accessor functions on the
  // URLRequest class to set these fields. whereas we use ResourceRequest here.
  new_request->request_initiator = request_info->begin_params->initiator_origin;
  new_request->referrer = request_info->common_params.referrer.url;
  new_request->referrer_policy = Referrer::ReferrerPolicyForUrlRequest(
      request_info->common_params.referrer.policy);
  new_request->headers.AddHeadersFromString(
      request_info->begin_params->headers);

  std::string accept_value = network::kFrameAcceptHeader;
  if (base::FeatureList::IsEnabled(features::kSignedHTTPExchange)) {
    DCHECK(!accept_value.empty());
    accept_value.append(kAcceptHeaderSignedExchangeSuffix);
  }

  new_request->headers.SetHeader(network::kAcceptHeader, accept_value);

  new_request->resource_type = request_info->is_main_frame
                                   ? RESOURCE_TYPE_MAIN_FRAME
                                   : RESOURCE_TYPE_SUB_FRAME;
  if (request_info->is_main_frame)
    new_request->update_first_party_url_on_redirect = true;

  int load_flags = request_info->begin_params->load_flags;
  load_flags |= net::LOAD_VERIFY_EV_CERT;
  if (request_info->is_main_frame)
    load_flags |= net::LOAD_MAIN_FRAME_DEPRECATED;

  // Sync loads should have maximum priority and should be the only
  // requests that have the ignore limits flag set.
  DCHECK(!(load_flags & net::LOAD_IGNORE_LIMITS));

  new_request->load_flags = load_flags;

  new_request->request_body = request_info->common_params.post_data.get();
  new_request->report_raw_headers = request_info->report_raw_headers;
  new_request->allow_download = allow_download;
  new_request->enable_load_timing = true;

  new_request->fetch_request_mode = network::mojom::FetchRequestMode::kNavigate;
  new_request->fetch_credentials_mode =
      network::mojom::FetchCredentialsMode::kInclude;
  new_request->fetch_redirect_mode = network::mojom::FetchRedirectMode::kManual;
  new_request->fetch_request_context_type =
      request_info->begin_params->request_context_type;
  return new_request;
}

// Called for requests that we don't have a URLLoaderFactory for.
void UnknownSchemeCallback(bool handled_externally,
                           network::mojom::URLLoaderRequest request,
                           network::mojom::URLLoaderClientPtr client) {
  client->OnComplete(network::URLLoaderCompletionStatus(
      handled_externally ? net::ERR_ABORTED : net::ERR_UNKNOWN_URL_SCHEME));
}

}  // namespace

// Kept around during the lifetime of the navigation request, and is
// responsible for dispatching a ResourceRequest to the appropriate
// URLLoader.  In order to get the right URLLoader it builds a vector
// of URLLoaderRequestHandler's and successively calls MaybeCreateLoader
// on each until the request is successfully handled. The same sequence
// may be performed multiple times when redirects happen.
// TODO(michaeln): Expose this class and add more unittests.
class NavigationURLLoaderNetworkService::URLLoaderRequestController
    : public network::mojom::URLLoaderClient {
 public:
  URLLoaderRequestController(
      std::vector<std::unique_ptr<URLLoaderRequestHandler>> initial_handlers,
      std::unique_ptr<network::ResourceRequest> resource_request,
      ResourceContext* resource_context,
      scoped_refptr<URLLoaderFactoryGetter> default_url_loader_factory_getter,
      const GURL& url,
      base::Optional<url::Origin> initiator_origin,
      base::Optional<std::string> suggested_filename,
      network::mojom::URLLoaderFactoryRequest proxied_factory_request,
      network::mojom::URLLoaderFactoryPtrInfo proxied_factory_info,
      std::set<std::string> known_schemes,
      const base::WeakPtr<NavigationURLLoaderNetworkService>& owner)
      : handlers_(std::move(initial_handlers)),
        resource_request_(std::move(resource_request)),
        resource_context_(resource_context),
        default_url_loader_factory_getter_(default_url_loader_factory_getter),
        url_(url),
        initiator_origin_(initiator_origin),
        suggested_filename_(suggested_filename),
        owner_(owner),
        response_loader_binding_(this),
        proxied_factory_request_(std::move(proxied_factory_request)),
        proxied_factory_info_(std::move(proxied_factory_info)),
        known_schemes_(std::move(known_schemes)),
        weak_factory_(this) {}

  ~URLLoaderRequestController() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  }

  static uint32_t GetURLLoaderOptions(bool is_main_frame) {
    uint32_t options = network::mojom::kURLLoadOptionSendSSLInfoWithResponse;
    if (is_main_frame)
      options |= network::mojom::kURLLoadOptionSendSSLInfoForCertificateError;

    if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      options |= network::mojom::kURLLoadOptionSniffMimeType;
    } else {
      // TODO(arthursonzogni): This is a temporary option. Remove this as soon
      // as the InterceptingResourceHandler is removed.
      // See https://crbug.com/791049.
      options |= network::mojom::kURLLoadOptionPauseOnResponseStarted;
    }

    return options;
  }

  void CreateNonNetworkServiceURLLoader(
      net::URLRequestContextGetter* url_request_context_getter,
      storage::FileSystemContext* upload_file_system_context,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      ServiceWorkerNavigationHandleCore* service_worker_navigation_handle_core,
      AppCacheNavigationHandleCore* appcache_handle_core,
      network::mojom::URLLoaderRequest url_loader,
      network::mojom::URLLoaderClientPtr url_loader_client) {
    DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    default_loader_used_ = true;
    if (base::FeatureList::IsEnabled(features::kSignedHTTPExchange)) {
      DCHECK(!default_url_loader_factory_getter_);
      // It is safa to pass the callback of CreateURLLoaderThrottles with the
      // unretained |this|, because the passed callback will be used by a
      // SignedExchangeHandler which is indirectly owned by |this| until its
      // header is verified and parsed, that's where the getter is used.
      handlers_.push_back(std::make_unique<WebPackageRequestHandler>(
          url::Origin::Create(request_info->common_params.url),
          GetURLLoaderOptions(request_info->is_main_frame),
          base::MakeRefCounted<
              SignedExchangeURLLoaderFactoryForNonNetworkService>(
              resource_context_, url_request_context_getter),
          base::BindRepeating(
              &URLLoaderRequestController::CreateURLLoaderThrottles,
              base::Unretained(this)),
          url_request_context_getter));
    }

    // The ResourceDispatcherHostImpl can be null in unit tests.
    if (ResourceDispatcherHostImpl::Get()) {
      ResourceDispatcherHostImpl::Get()->BeginNavigationRequest(
          resource_context_, url_request_context_getter->GetURLRequestContext(),
          upload_file_system_context, *request_info,
          std::move(navigation_ui_data), nullptr, std::move(url_loader_client),
          std::move(url_loader), service_worker_navigation_handle_core,
          appcache_handle_core,
          GetURLLoaderOptions(request_info->is_main_frame),
          &global_request_id_);
    }

    // TODO(arthursonzogni): Detect when the ResourceDispatcherHost didn't
    // create a URLLoader. When it doesn't, do not send OnRequestStarted().
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&NavigationURLLoaderNetworkService::OnRequestStarted,
                       owner_, base::TimeTicks::Now()));
  }

  // TODO(arthursonzogni): See if this could eventually be unified with Start().
  void StartWithoutNetworkService(
      net::URLRequestContextGetter* url_request_context_getter,
      storage::FileSystemContext* upload_file_system_context,
      ServiceWorkerNavigationHandleCore* service_worker_navigation_handle_core,
      AppCacheNavigationHandleCore* appcache_handle_core,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data) {
    DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(!started_);
    started_ = true;

    frame_tree_node_id_ = request_info->frame_tree_node_id;
    web_contents_getter_ = base::BindRepeating(
        &GetWebContentsFromFrameTreeNodeID, frame_tree_node_id_);

    std::vector<std::unique_ptr<content::URLLoaderThrottle>> throttles =
        GetContentClient()->browser()->CreateURLLoaderThrottles(
            *resource_request_, resource_context_, web_contents_getter_,
            navigation_ui_data.get(), frame_tree_node_id_);

    auto load_single_request = base::BindOnce(
        &URLLoaderRequestController::CreateNonNetworkServiceURLLoader,
        weak_factory_.GetWeakPtr(),
        base::Unretained(url_request_context_getter),
        base::Unretained(upload_file_system_context), std::move(request_info),
        std::move(navigation_ui_data),
        base::Unretained(service_worker_navigation_handle_core),
        base::Unretained(appcache_handle_core));

    url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
        base::MakeRefCounted<SingleRequestURLLoaderFactory>(
            std::move(load_single_request)),
        std::move(throttles), -1 /* routing_id */, 0 /* request_id */,
        network::mojom::kURLLoadOptionNone, resource_request_.get(),
        this /* client */, kNavigationUrlLoaderTrafficAnnotation,
        base::ThreadTaskRunnerHandle::Get());
  }

  void Start(
      net::URLRequestContextGetter* url_request_context_getter,
      ServiceWorkerNavigationHandleCore* service_worker_navigation_handle_core,
      AppCacheNavigationHandleCore* appcache_handle_core,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      network::mojom::URLLoaderFactoryPtrInfo factory_for_webui,
      int frame_tree_node_id,
      std::unique_ptr<service_manager::Connector> connector) {
    DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(!started_);
    global_request_id_ = MakeGlobalRequestID();
    frame_tree_node_id_ = frame_tree_node_id;
    started_ = true;
    web_contents_getter_ =
        base::Bind(&GetWebContentsFromFrameTreeNodeID, frame_tree_node_id);
    navigation_ui_data_ = std::move(navigation_ui_data);
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
      url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
          base::MakeRefCounted<WrapperSharedURLLoaderFactory>(
              std::move(factory_for_webui)),
          CreateURLLoaderThrottles(), 0 /* routing_id */, 0 /* request_id? */,
          network::mojom::kURLLoadOptionNone, resource_request_.get(), this,
          kNavigationUrlLoaderTrafficAnnotation,
          base::ThreadTaskRunnerHandle::Get());
      return;
    }

    if (service_worker_navigation_handle_core) {
      network::mojom::RequestContextFrameType frame_type =
          request_info->is_main_frame
              ? network::mojom::RequestContextFrameType::kTopLevel
              : network::mojom::RequestContextFrameType::kNested;

      storage::BlobStorageContext* blob_storage_context = GetBlobStorageContext(
          GetChromeBlobStorageContextForResourceContext(resource_context_));
      std::unique_ptr<URLLoaderRequestHandler> service_worker_handler =
          ServiceWorkerRequestHandler::InitializeForNavigationNetworkService(
              *resource_request_, resource_context_,
              service_worker_navigation_handle_core, blob_storage_context,
              request_info->begin_params->skip_service_worker, resource_type,
              request_info->begin_params->request_context_type, frame_type,
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

    if (base::FeatureList::IsEnabled(features::kSignedHTTPExchange)) {
      // It is safa to pass the callback of CreateURLLoaderThrottles with the
      // unretained |this|, because the passed callback will be used by a
      // SignedExchangeHandler which is indirectly owned by |this| until its
      // header is verified and parsed, that's where the getter is used.
      handlers_.push_back(std::make_unique<WebPackageRequestHandler>(
          url::Origin::Create(request_info->common_params.url),
          GetURLLoaderOptions(request_info->is_main_frame),
          default_url_loader_factory_getter_->GetNetworkFactory(),
          base::BindRepeating(
              &URLLoaderRequestController::CreateURLLoaderThrottles,
              base::Unretained(this)),
          url_request_context_getter));
    }

    Restart();
  }

  // This could be called multiple times to follow a chain of redirects.
  void Restart() {
    DCHECK(IsRequestHandlerEnabled());
    // Clear |url_loader_| if it's not the default one (network). This allows
    // the restarted request to use a new loader, instead of, e.g., reusing the
    // AppCache or service worker loader. For an optimization, we keep and reuse
    // the default url loader if the all |handlers_| doesn't handle the
    // redirected request.
    if (!default_loader_used_)
      url_loader_.reset();
    handler_index_ = 0;
    received_response_ = false;
    MaybeStartLoader(nullptr /* handler */, {} /* single_request_handler */);
  }

  // |handler| is the one who called this method (as a LoaderCallback), nullptr
  // if this method is not called by a handler.
  // |single_request_handler| is the RequestHandler given by the |handler|,
  // non-null if the handler wants to handle the request.
  void MaybeStartLoader(
      URLLoaderRequestHandler* handler,
      SingleRequestURLLoaderFactory::RequestHandler single_request_handler) {
    DCHECK(IsRequestHandlerEnabled());
    if (single_request_handler) {
      // |handler| wants to handle the request.
      DCHECK(handler);
      default_loader_used_ = false;
      url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
          base::MakeRefCounted<SingleRequestURLLoaderFactory>(
              std::move(single_request_handler)),
          CreateURLLoaderThrottles(), frame_tree_node_id_, 0 /* request_id? */,
          network::mojom::kURLLoadOptionNone, resource_request_.get(), this,
          kNavigationUrlLoaderTrafficAnnotation,
          base::ThreadTaskRunnerHandle::Get());

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
    // subresource loading. In that case we want to skip AppCache.
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

    // TODO(https://crbug.com/796425): We temporarily wrap raw
    // mojom::URLLoaderFactory pointers into SharedURLLoaderFactory. Need to
    // further refactor the factory getters to avoid this.
    scoped_refptr<network::SharedURLLoaderFactory> factory;
    DCHECK_EQ(handlers_.size(), handler_index_);
    if (resource_request_->url.SchemeIs(url::kBlobScheme)) {
      factory = base::MakeRefCounted<WeakWrapperSharedURLLoaderFactory>(
          default_url_loader_factory_getter_->GetBlobFactory());
    } else if (!IsURLHandledByNetworkService(resource_request_->url) &&
               !resource_request_->url.SchemeIs(url::kDataScheme)) {
      if (known_schemes_.find(resource_request_->url.scheme()) ==
          known_schemes_.end()) {
        bool handled = GetContentClient()->browser()->HandleExternalProtocol(
            resource_request_->url, web_contents_getter_,
            ChildProcessHost::kInvalidUniqueID, navigation_ui_data_.get(),
            resource_request_->resource_type == RESOURCE_TYPE_MAIN_FRAME,
            static_cast<ui::PageTransition>(resource_request_->transition_type),
            resource_request_->has_user_gesture);
        factory = base::MakeRefCounted<SingleRequestURLLoaderFactory>(
            base::BindOnce(UnknownSchemeCallback, handled));
      } else {
        network::mojom::URLLoaderFactoryPtr& non_network_factory =
            non_network_url_loader_factories_[resource_request_->url.scheme()];
        if (!non_network_factory.is_bound()) {
          BrowserThread::PostTask(
              BrowserThread::UI, FROM_HERE,
              base::BindOnce(&NavigationURLLoaderNetworkService ::
                                 BindNonNetworkURLLoaderFactoryRequest,
                             owner_, frame_tree_node_id_,
                             resource_request_->url,
                             mojo::MakeRequest(&non_network_factory)));
        }
        factory = base::MakeRefCounted<WeakWrapperSharedURLLoaderFactory>(
            non_network_factory.get());
      }
    } else {
      default_loader_used_ = true;

      // NOTE: We only support embedders proxying network-service-bound requests
      // not handled by URLLoaderRequestHandlers above (e.g. Service Worker or
      // AppCache). Hence this code is only reachable when one of the above
      // handlers isn't used and the URL is either a data URL or has a scheme
      // which is handled by the network service. We explicitly avoid proxying
      // the data URL case here.
      if (proxied_factory_request_.is_pending() &&
          !resource_request_->url.SchemeIs(url::kDataScheme)) {
        DCHECK(proxied_factory_info_.is_valid());
        // We don't worry about reconnection since it's a single navigation.
        default_url_loader_factory_getter_->CloneNetworkFactory(
            std::move(proxied_factory_request_));
        factory = base::MakeRefCounted<WrapperSharedURLLoaderFactory>(
            std::move(proxied_factory_info_));
      } else {
        factory = default_url_loader_factory_getter_->GetNetworkFactory();
      }
    }
    url_chain_.push_back(resource_request_->url);
    uint32_t options = GetURLLoaderOptions(resource_request_->resource_type ==
                                           RESOURCE_TYPE_MAIN_FRAME);
    url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
        factory, CreateURLLoaderThrottles(), frame_tree_node_id_,
        0 /* request_id? */, options, resource_request_.get(), this,
        kNavigationUrlLoaderTrafficAnnotation,
        base::ThreadTaskRunnerHandle::Get());
  }

  void FollowRedirect() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(!redirect_info_.new_url.is_empty());

    if (!IsRequestHandlerEnabled()) {
      url_loader_->FollowRedirect();
      return;
    }

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
    resource_request_->referrer_policy = redirect_info_.new_referrer_policy;
    url_chain_.push_back(redirect_info_.new_url);

    Restart();
  }

  base::Optional<SubresourceLoaderParams> TakeSubresourceLoaderParams() {
    return std::move(subresource_loader_params_);
  }

 private:
  // network::mojom::URLLoaderClient implementation:
  void OnReceiveResponse(
      const network::ResourceResponseHead& head,
      const base::Optional<net::SSLInfo>& ssl_info,
      network::mojom::DownloadedTempFilePtr downloaded_file) override {
    received_response_ = true;

    // If the default loader (network) was used to handle the URL load request
    // we need to see if the handlers want to potentially create a new loader
    // for the response. e.g. AppCache.
    if (MaybeCreateLoaderForResponse(head))
      return;

    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints;

    // Currently only plugin handlers may intercept the response. Don't treat
    // the response as download if it has been handled by plugins.
    bool response_intercepted = false;
    if (url_loader_) {
      url_loader_client_endpoints = url_loader_->Unbind();
      response_intercepted = url_loader_->response_intercepted();
    } else {
      url_loader_client_endpoints =
          network::mojom::URLLoaderClientEndpoints::New(
              response_url_loader_.PassInterface(),
              response_loader_binding_.Unbind());
    }

    scoped_refptr<network::ResourceResponse> response(
        new network::ResourceResponse());
    response->head = head;

    bool is_download;
    bool is_stream;
    std::unique_ptr<NavigationData> cloned_navigation_data;
    if (IsRequestHandlerEnabled()) {
      is_download = !response_intercepted &&
                    IsDownload(*response.get(), url_, url_chain_,
                               initiator_origin_, suggested_filename_);
      is_stream = false;
    } else {
      ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
      net::URLRequest* url_request = rdh->GetURLRequest(global_request_id_);
      ResourceRequestInfoImpl* info =
          ResourceRequestInfoImpl::ForRequest(url_request);
      is_download = !response_intercepted && info->IsDownload();
      is_stream = info->is_stream();
      if (rdh->delegate()) {
        NavigationData* navigation_data =
            rdh->delegate()->GetNavigationData(url_request);

        // Clone the embedder's NavigationData before moving it to the UI
        // thread.
        if (navigation_data)
          cloned_navigation_data = navigation_data->Clone();
      }

      // This is similar to what is done in
      // ServiceWorkerControlleeHandler::MaybeCreateSubresourceLoaderParams().
      // It takes the matching ControllerServiceWorkerInfo (if any) associated
      // with the request. It will be sent to the renderer process and used to
      // intercept requests.
      // TODO(arthursonzogni): This is needed only for the non-S13nServiceWorker
      // case. The S13nServiceWorker case is still not supported without the
      // NetworkService. This block needs to be updated once support for it will
      // be added.
      ServiceWorkerProviderHost* sw_provider_host =
          ServiceWorkerRequestHandler::GetProviderHost(url_request);
      if (sw_provider_host && sw_provider_host->controller()) {
        subresource_loader_params_ = SubresourceLoaderParams();
        subresource_loader_params_->controller_service_worker_info =
            mojom::ControllerServiceWorkerInfo::New();
        subresource_loader_params_->controller_service_worker_info
            ->object_info = sw_provider_host->GetOrCreateServiceWorkerHandle(
            sw_provider_host->controller());
      }
    }

    // Make a copy of the ResourceResponse before it is passed to another
    // thread.
    //
    // TODO(davidben): This copy could be avoided if ResourceResponse weren't
    // reference counted and the loader stack passed unique ownership of the
    // response. https://crbug.com/416050
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&NavigationURLLoaderNetworkService::OnReceiveResponse,
                       owner_, response->DeepCopy(),
                       std::move(url_loader_client_endpoints),
                       std::move(ssl_info), std::move(cloned_navigation_data),
                       global_request_id_, is_download, is_stream,
                       std::move(downloaded_file)));
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override {
    if (--redirect_limit_ == 0) {
      OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_TOO_MANY_REDIRECTS));
      return;
    }

    // Store the redirect_info for later use in FollowRedirect where we give
    // our handlers_ a chance to intercept the request for the new location.
    redirect_info_ = redirect_info;

    scoped_refptr<network::ResourceResponse> response(
        new network::ResourceResponse());
    response->head = head;
    url_ = redirect_info.new_url;

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

  void OnStartLoadingResponseBody(mojo::ScopedDataPipeConsumerHandle) override {
    // Not reached. At this point, the loader and client endpoints must have
    // been unbound and forwarded to the renderer.
    CHECK(false);
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    UMA_HISTOGRAM_BOOLEAN(
        "Navigation.URLLoaderNetworkService.OnCompleteHadSSLInfo",
        status.ssl_info.has_value());
    if (status.ssl_info.has_value()) {
      UMA_HISTOGRAM_MEMORY_KB(
          "Navigation.URLLoaderNetworkService.OnCompleteCertificateChainsSize",
          GetCertificateChainsSizeInKB(status.ssl_info.value()));
    }

    if (status.error_code != net::OK && !received_response_) {
      // If the default loader (network) was used to handle the URL load
      // request we need to see if the handlers want to potentially create a
      // new loader for the response. e.g. AppCache.
      if (MaybeCreateLoaderForResponse(network::ResourceResponseHead()))
        return;
    }
    status_ = status;

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&NavigationURLLoaderNetworkService::OnComplete, owner_,
                       status));
  }

  // Returns true if a handler wants to handle the response, i.e. return a
  // different response. For e.g. AppCache may have fallback content.
  bool MaybeCreateLoaderForResponse(
      const network::ResourceResponseHead& response) {
    if (!IsRequestHandlerEnabled())
      return false;

    if (!default_loader_used_)
      return false;

    for (auto& handler : handlers_) {
      network::mojom::URLLoaderClientRequest response_client_request;
      if (handler->MaybeCreateLoaderForResponse(response, &response_url_loader_,
                                                &response_client_request,
                                                url_loader_.get())) {
        response_loader_binding_.Bind(std::move(response_client_request));
        default_loader_used_ = false;
        url_loader_.reset();
        return true;
      }
    }
    return false;
  }

  std::vector<std::unique_ptr<content::URLLoaderThrottle>>
  CreateURLLoaderThrottles() {
    return GetContentClient()->browser()->CreateURLLoaderThrottles(
        *resource_request_, resource_context_, web_contents_getter_,
        navigation_ui_data_.get(), frame_tree_node_id_);
  }

  std::vector<std::unique_ptr<URLLoaderRequestHandler>> handlers_;
  size_t handler_index_ = 0;

  std::unique_ptr<network::ResourceRequest> resource_request_;
  int frame_tree_node_id_ = 0;
  GlobalRequestID global_request_id_;
  net::RedirectInfo redirect_info_;
  int redirect_limit_ = net::URLRequest::kMaxRedirects;
  ResourceContext* resource_context_;
  base::Callback<WebContents*()> web_contents_getter_;
  std::unique_ptr<NavigationUIData> navigation_ui_data_;
  scoped_refptr<URLLoaderFactoryGetter> default_url_loader_factory_getter_;

  std::unique_ptr<ThrottlingURLLoader> url_loader_;

  BlobHandles blob_handles_;
  std::vector<GURL> url_chain_;

  // Current URL that is being navigated, updated after redirection.
  GURL url_;

  base::Optional<url::Origin> initiator_origin_;

  // If this request was triggered by an anchor tag with a download attribute,
  // the |suggested_filename_| will be the (possibly empty) value of said
  // attribute.
  base::Optional<std::string> suggested_filename_;

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
  mojo::Binding<network::mojom::URLLoaderClient> response_loader_binding_;

  // URLLoader instance for response loaders, i.e loaders created for handing
  // responses received from the network URLLoader.
  network::mojom::URLLoaderPtr response_url_loader_;

  // Set to true if we receive a valid response from a URLLoader, i.e.
  // URLLoaderClient::OnReceivedResponse() is called.
  bool received_response_ = false;

  bool started_ = false;

  // Lazily initialized and used in the case of non-network resource
  // navigations. Keyed by URL scheme.
  std::map<std::string, network::mojom::URLLoaderFactoryPtr>
      non_network_url_loader_factories_;

  // The completion status if it has been received. This is needed to handle
  // the case that the response is intercepted by download, and OnComplete() is
  // already called while we are transferring the |url_loader_| and response
  // body to download code.
  base::Optional<network::URLLoaderCompletionStatus> status_;

  // Before creating this URLLoaderRequestController on UI thread, the embedder
  // may have elected to proxy the URLLoaderFactory request, in which case these
  // fields will contain input (info) and output (request) endpoints for the
  // proxy. If this controller is handling a request for which proxying is
  // supported, requests will be plumbed through these endpoints.
  //
  // Note that these are only used for requests that go to the Network Service.
  network::mojom::URLLoaderFactoryRequest proxied_factory_request_;
  network::mojom::URLLoaderFactoryPtrInfo proxied_factory_info_;

  // The schemes that this loader can use. For anything else we'll try external
  // protocol handlers.
  std::set<std::string> known_schemes_;

  base::WeakPtrFactory<URLLoaderRequestController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderRequestController);
};

// TODO(https://crbug.com/790734): pass |navigation_ui_data| along with the
// request so that it could be modified.
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
  int frame_tree_node_id = request_info->frame_tree_node_id;

  TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "navigation", "Navigation timeToResponseStarted", this,
      request_info->common_params.navigation_start, "FrameTreeNode id",
      frame_tree_node_id);

  ServiceWorkerNavigationHandleCore* service_worker_navigation_handle_core =
      service_worker_navigation_handle
          ? service_worker_navigation_handle->core()
          : nullptr;

  AppCacheNavigationHandleCore* appcache_handle_core =
      appcache_handle ? appcache_handle->core() : nullptr;

  std::unique_ptr<network::ResourceRequest> new_request = CreateResourceRequest(
      request_info.get(), frame_tree_node_id, allow_download_);

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    DCHECK(!request_controller_);
    request_controller_ = std::make_unique<URLLoaderRequestController>(
        /* initial_handlers = */
        std::vector<std::unique_ptr<URLLoaderRequestHandler>>(),
        std::move(new_request), resource_context,
        /* default_url_factory_getter = */ nullptr,
        request_info->common_params.url,
        request_info->begin_params->initiator_origin,
        request_info->common_params.suggested_filename,
        /* proxied_url_loader_factory_request */ nullptr,
        /* proxied_url_loader_factory_info */ nullptr, std::set<std::string>(),
        weak_factory_.GetWeakPtr());

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &URLLoaderRequestController::StartWithoutNetworkService,
            base::Unretained(request_controller_.get()),
            base::RetainedRef(storage_partition->GetURLRequestContext()),
            base::Unretained(storage_partition->GetFileSystemContext()),
            base::Unretained(service_worker_navigation_handle_core),
            base::Unretained(appcache_handle_core), std::move(request_info),
            std::move(navigation_ui_data)));
    return;
  }

  // Check if a web UI scheme wants to handle this request.
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  network::mojom::URLLoaderFactoryPtrInfo factory_for_webui;
  const auto& schemes = URLDataManagerBackend::GetWebUISchemes();
  std::string scheme = new_request->url.scheme();
  if (std::find(schemes.begin(), schemes.end(), scheme) != schemes.end()) {
    factory_for_webui = CreateWebUIURLLoaderBinding(
                            frame_tree_node->current_frame_host(), scheme)
                            .PassInterface();
  }

  network::mojom::URLLoaderFactoryPtrInfo proxied_factory_info;
  network::mojom::URLLoaderFactoryRequest proxied_factory_request;
  if (frame_tree_node) {
    // |frame_tree_node| may be null in some unit test environments.
    GetContentClient()
        ->browser()
        ->RegisterNonNetworkNavigationURLLoaderFactories(
            frame_tree_node->current_frame_host(),
            &non_network_url_loader_factories_);

    // The embedder may want to proxy all network-bound URLLoaderFactory
    // requests that it can. If it elects to do so, we'll pass its proxy
    // endpoints off to the URLLoaderRequestController where wthey will be
    // connected if the request type supports proxying.
    network::mojom::URLLoaderFactoryPtrInfo factory_info;
    auto factory_request = mojo::MakeRequest(&factory_info);
    bool use_proxy = GetContentClient()->browser()->WillCreateURLLoaderFactory(
        frame_tree_node->current_frame_host(), true /* is_navigation */,
        &factory_request);
    if (RenderFrameDevToolsAgentHost::WillCreateURLLoaderFactory(
            frame_tree_node->current_frame_host(), true, &factory_request)) {
      use_proxy = true;
    }
    if (use_proxy) {
      proxied_factory_request = std::move(factory_request);
      proxied_factory_info = std::move(factory_info);
    }
  }

  auto* partition = static_cast<StoragePartitionImpl*>(storage_partition);
  non_network_url_loader_factories_[url::kFileScheme] =
      std::make_unique<FileURLLoaderFactory>(
          partition->browser_context()->GetPath(),
          base::CreateSequencedTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::BACKGROUND,
               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  std::set<std::string> known_schemes;
  for (auto& iter : non_network_url_loader_factories_)
    known_schemes.insert(iter.first);

  DCHECK(!request_controller_);
  request_controller_ = std::make_unique<URLLoaderRequestController>(
      std::move(initial_handlers), std::move(new_request), resource_context,
      partition->url_loader_factory_getter(), request_info->common_params.url,
      request_info->begin_params->initiator_origin,
      request_info->common_params.suggested_filename,
      std::move(proxied_factory_request), std::move(proxied_factory_info),
      std::move(known_schemes), weak_factory_.GetWeakPtr());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &URLLoaderRequestController::Start,
          base::Unretained(request_controller_.get()),
          base::RetainedRef(storage_partition->GetURLRequestContext()),
          service_worker_navigation_handle_core, appcache_handle_core,
          std::move(request_info), std::move(navigation_ui_data),
          std::move(factory_for_webui), frame_tree_node_id,
          ServiceManagerConnection::GetForProcess()->GetConnector()->Clone()));
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

void NavigationURLLoaderNetworkService::OnReceiveResponse(
    scoped_refptr<network::ResourceResponse> response,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    const base::Optional<net::SSLInfo>& maybe_ssl_info,
    std::unique_ptr<NavigationData> navigation_data,
    const GlobalRequestID& global_request_id,
    bool is_download,
    bool is_stream,
    network::mojom::DownloadedTempFilePtr downloaded_file) {
  TRACE_EVENT_ASYNC_END2("navigation", "Navigation timeToResponseStarted", this,
                         "&NavigationURLLoaderNetworkService", this, "success",
                         true);

  // TODO(scottmg): This needs to do more of what
  // NavigationResourceHandler::OnResponseStarted() does.
  net::SSLInfo ssl_info;
  if (maybe_ssl_info.has_value())
    ssl_info = maybe_ssl_info.value();

  delegate_->OnResponseStarted(
      std::move(response), std::move(url_loader_client_endpoints), nullptr,
      std::move(ssl_info), std::move(navigation_data), global_request_id,
      allow_download_ && is_download, is_stream,
      request_controller_->TakeSubresourceLoaderParams());
}

void NavigationURLLoaderNetworkService::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    scoped_refptr<network::ResourceResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnRequestRedirected(redirect_info, std::move(response));
}

void NavigationURLLoaderNetworkService::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code == net::OK)
    return;

  TRACE_EVENT_ASYNC_END2("navigation", "Navigation timeToResponseStarted", this,
                         "&NavigationURLLoaderNetworkService", this, "success",
                         false);

  delegate_->OnRequestFailed(status.exists_in_cache, status.error_code,
                             status.ssl_info);
}

void NavigationURLLoaderNetworkService::OnRequestStarted(
    base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnRequestStarted(timestamp);
}

void NavigationURLLoaderNetworkService::BindNonNetworkURLLoaderFactoryRequest(
    int frame_tree_node_id,
    const GURL& url,
    network::mojom::URLLoaderFactoryRequest factory) {
  auto it = non_network_url_loader_factories_.find(url.scheme());
  if (it == non_network_url_loader_factories_.end()) {
    DVLOG(1) << "Ignoring request with unknown scheme: " << url.spec();
    return;
  }
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      frame_tree_node->current_frame_host(), true /* is_navigation */,
      &factory);
  it->second->Clone(std::move(factory));
}

}  // namespace content
