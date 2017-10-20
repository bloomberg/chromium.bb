// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/resource_requester_info.h"
#include "content/browser/loader/url_loader_factory_impl.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/request_priority.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

// This class wraps a mojo::AssociatedInterfacePtr<URLLoader>. It also is a
// URLLoader implementation and delegates URLLoader calls to the wrapped loader.
class DelegatingURLLoader final : public mojom::URLLoader {
 public:
  explicit DelegatingURLLoader(mojom::URLLoaderPtr loader)
      : binding_(this), loader_(std::move(loader)) {}
  ~DelegatingURLLoader() override {}

  void FollowRedirect() override { loader_->FollowRedirect(); }

  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {
    loader_->SetPriority(priority, intra_priority_value);
  }

  void PauseReadingBodyFromNet() override {
    loader_->PauseReadingBodyFromNet();
  }
  void ResumeReadingBodyFromNet() override {
    loader_->ResumeReadingBodyFromNet();
  }

  mojom::URLLoaderPtr CreateInterfacePtrAndBind() {
    mojom::URLLoaderPtr loader;
    binding_.Bind(mojo::MakeRequest(&loader));
    // This unretained pointer is safe, because |binding_| is owned by |this|
    // and the callback will never be called after |this| is destroyed.
    binding_.set_connection_error_handler(
        base::BindOnce(&DelegatingURLLoader::Cancel, base::Unretained(this)));
    return loader;
  }

 private:
  // Called when the mojom::URLLoaderPtr in the service worker is deleted.
  void Cancel() {
    // Cancel loading as stated in url_loader.mojom.
    loader_ = nullptr;
  }

  mojo::Binding<mojom::URLLoader> binding_;
  mojom::URLLoaderPtr loader_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingURLLoader);
};

ServiceWorkerDevToolsAgentHost* GetAgentHost(
    const std::pair<int, int>& worker_id) {
  return ServiceWorkerDevToolsManager::GetInstance()
      ->GetDevToolsAgentHostForWorker(worker_id.first, worker_id.second);
}

void NotifyNavigationPreloadRequestSentOnUI(
    const ResourceRequest& request,
    const std::pair<int, int>& worker_id,
    const std::string& request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ServiceWorkerDevToolsAgentHost* agent_host = GetAgentHost(worker_id))
    agent_host->NavigationPreloadRequestSent(request_id, request);
}

void NotifyNavigationPreloadResponseReceivedOnUI(
    const GURL& url,
    const ResourceResponseHead& head,
    const std::pair<int, int>& worker_id,
    const std::string& request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ServiceWorkerDevToolsAgentHost* agent_host = GetAgentHost(worker_id))
    agent_host->NavigationPreloadResponseReceived(request_id, url, head);
}

void NotifyNavigationPreloadCompletedOnUI(
    const ResourceRequestCompletionStatus& completion_status,
    const std::pair<int, int>& worker_id,
    const std::string& request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ServiceWorkerDevToolsAgentHost* agent_host = GetAgentHost(worker_id))
    agent_host->NavigationPreloadCompleted(request_id, completion_status);
}

// DelegatingURLLoaderClient is the URLLoaderClient for the navigation preload
// network request. It watches as the response comes in, and pipes the response
// back to the service worker while also doing extra processing like notifying
// DevTools.
class DelegatingURLLoaderClient final : public mojom::URLLoaderClient {
 public:
  using WorkerId = std::pair<int, int>;
  explicit DelegatingURLLoaderClient(mojom::URLLoaderClientPtr client,
                                     base::OnceClosure on_response,
                                     const ResourceRequest& request)
      : binding_(this),
        client_(std::move(client)),
        on_response_(std::move(on_response)),
        url_(request.url) {
    AddDevToolsCallback(
        base::Bind(&NotifyNavigationPreloadRequestSentOnUI, request));
  }
  ~DelegatingURLLoaderClient() override {
    if (!completed_) {
      // Let the service worker know that the request has been canceled.
      ResourceRequestCompletionStatus status;
      status.error_code = net::ERR_ABORTED;
      client_->OnComplete(status);
      AddDevToolsCallback(
          base::Bind(&NotifyNavigationPreloadCompletedOnUI, status));
    }
  }

  void MayBeReportToDevTools(WorkerId worker_id, int fetch_event_id) {
    worker_id_ = worker_id;
    devtools_request_id_ = base::StringPrintf("preload-%d", fetch_event_id);
    MayBeRunDevToolsCallbacks();
  }

  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override {
    client_->OnDataDownloaded(data_length, encoded_length);
  }
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    client_->OnUploadProgress(current_position, total_size,
                              std::move(ack_callback));
  }
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {
    client_->OnReceiveCachedMetadata(data);
  }
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    client_->OnTransferSizeUpdated(transfer_size_diff);
  }
  void OnReceiveResponse(
      const ResourceResponseHead& head,
      const base::Optional<net::SSLInfo>& ssl_info,
      mojom::DownloadedTempFilePtr downloaded_file) override {
    client_->OnReceiveResponse(head, ssl_info, std::move(downloaded_file));
    DCHECK(on_response_);
    std::move(on_response_).Run();
    AddDevToolsCallback(
        base::Bind(&NotifyNavigationPreloadResponseReceivedOnUI, url_, head));
  }
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& head) override {
    completed_ = true;
    // When the server returns a redirect response, we only send
    // OnReceiveRedirect IPC and don't send OnComplete IPC. The service worker
    // will clean up the preload request when OnReceiveRedirect() is called.
    client_->OnReceiveRedirect(redirect_info, head);
    AddDevToolsCallback(
        base::Bind(&NotifyNavigationPreloadResponseReceivedOnUI, url_, head));
    ResourceRequestCompletionStatus status;
    AddDevToolsCallback(
        base::Bind(&NotifyNavigationPreloadCompletedOnUI, status));
  }
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    client_->OnStartLoadingResponseBody(std::move(body));
  }
  void OnComplete(
      const ResourceRequestCompletionStatus& completion_status) override {
    if (completed_)
      return;
    completed_ = true;
    client_->OnComplete(completion_status);
    AddDevToolsCallback(
        base::Bind(&NotifyNavigationPreloadCompletedOnUI, completion_status));
  }

  void Bind(mojom::URLLoaderClientPtr* ptr_info) {
    binding_.Bind(mojo::MakeRequest(ptr_info));
  }

 private:
  void MayBeRunDevToolsCallbacks() {
    if (!worker_id_)
      return;
    while (!devtools_callbacks.empty()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(std::move(devtools_callbacks.front()), *worker_id_,
                         devtools_request_id_));
      devtools_callbacks.pop();
    }
  }
  void AddDevToolsCallback(
      base::Callback<void(const WorkerId&, const std::string&)> callback) {
    devtools_callbacks.push(callback);
    MayBeRunDevToolsCallbacks();
  }

  mojo::Binding<mojom::URLLoaderClient> binding_;
  mojom::URLLoaderClientPtr client_;
  base::OnceClosure on_response_;
  bool completed_ = false;
  const GURL url_;

  base::Optional<std::pair<int, int>> worker_id_;
  std::string devtools_request_id_;
  base::queue<base::Callback<void(const WorkerId&, const std::string&)>>
      devtools_callbacks;
  DISALLOW_COPY_AND_ASSIGN(DelegatingURLLoaderClient);
};

using EventType = ServiceWorkerMetrics::EventType;
EventType ResourceTypeToEventType(ResourceType resource_type) {
  switch (resource_type) {
    case RESOURCE_TYPE_MAIN_FRAME:
      return EventType::FETCH_MAIN_FRAME;
    case RESOURCE_TYPE_SUB_FRAME:
      return EventType::FETCH_SUB_FRAME;
    case RESOURCE_TYPE_SHARED_WORKER:
      return EventType::FETCH_SHARED_WORKER;
    case RESOURCE_TYPE_SERVICE_WORKER:
    case RESOURCE_TYPE_LAST_TYPE:
      NOTREACHED() << resource_type;
      return EventType::FETCH_SUB_RESOURCE;
    default:
      return EventType::FETCH_SUB_RESOURCE;
  }
}

std::unique_ptr<base::Value> NetLogServiceWorkerStatusCallback(
    ServiceWorkerStatusCode status,
    net::NetLogCaptureMode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("status", ServiceWorkerStatusToString(status));
  return std::move(dict);
}

std::unique_ptr<base::Value> NetLogFetchEventCallback(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult result,
    net::NetLogCaptureMode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("status", ServiceWorkerStatusToString(status));
  dict->SetBoolean("has_response",
                   result == SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE);
  return std::move(dict);
}

void EndNetLogEventWithServiceWorkerStatus(const net::NetLogWithSource& net_log,
                                           net::NetLogEventType type,
                                           ServiceWorkerStatusCode status) {
  net_log.EndEvent(type,
                   base::Bind(&NetLogServiceWorkerStatusCallback, status));
}

ServiceWorkerMetrics::EventType FetchTypeToWaitUntilEventType(
    ServiceWorkerFetchType type) {
  if (type == ServiceWorkerFetchType::FOREIGN_FETCH)
    return ServiceWorkerMetrics::EventType::FOREIGN_FETCH_WAITUNTIL;
  return ServiceWorkerMetrics::EventType::FETCH_WAITUNTIL;
}

const net::NetworkTrafficAnnotationTag kNavigationPreloadTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("service_worker_navigation_preload",
                                        R"(
    semantics {
      sender: "Service Worker Navigation Preload"
      description:
        "This request is issued by a navigation to fetch the content of the "
        "page that is being navigated to, in the case where a service worker "
        "has been registered for the page and is using the Navigation Preload "
        "API."
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
      setting:
        "This request can be prevented by disabling service workers, which can "
        "be done by disabling cookie and site data under Settings, Content "
        "Settings, Cookies."
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
      "Chrome would be unable to use service workers if this feature were "
      "disabled, which could result in a degraded experience for websites that "
      "register a service worker. Using either URLBlacklist or URLWhitelist "
      "policies (or a combination of both) limits the scope of these requests."
)");

}  // namespace

// ResponseCallback is owned by the callback that is passed to
// ServiceWorkerVersion::StartRequest*(), and held in pending_requests_
// until FinishRequest() is called.
class ServiceWorkerFetchDispatcher::ResponseCallback
    : public mojom::ServiceWorkerFetchResponseCallback {
 public:
  ResponseCallback(mojom::ServiceWorkerFetchResponseCallbackRequest request,
                   base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher,
                   ServiceWorkerVersion* version)
      : binding_(this, std::move(request)),
        fetch_dispatcher_(fetch_dispatcher),
        version_(version) {}

  ~ResponseCallback() override { DCHECK(fetch_event_id_.has_value()); }

  void set_fetch_event_id(int id) {
    DCHECK(!fetch_event_id_);
    fetch_event_id_ = id;
  }

  // Implements mojom::ServiceWorkerFetchResponseCallback.
  void OnResponse(const ServiceWorkerResponse& response,
                  base::Time dispatch_event_time) override {
    HandleResponse(fetch_dispatcher_, version_, fetch_event_id_, response,
                   nullptr /* body_as_stream */, nullptr /* body_as_blob */,
                   SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
                   dispatch_event_time);
  }
  void OnResponseBlob(const ServiceWorkerResponse& response,
                      blink::mojom::BlobPtr body_as_blob,
                      base::Time dispatch_event_time) override {
    HandleResponse(fetch_dispatcher_, version_, fetch_event_id_, response,
                   nullptr /* body_as_stream */, std::move(body_as_blob),
                   SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
                   dispatch_event_time);
  }
  void OnResponseStream(
      const ServiceWorkerResponse& response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      base::Time dispatch_event_time) override {
    HandleResponse(fetch_dispatcher_, version_, fetch_event_id_, response,
                   std::move(body_as_stream), nullptr /* body_as_blob */,
                   SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
                   dispatch_event_time);
  }
  void OnFallback(base::Time dispatch_event_time) override {
    HandleResponse(
        fetch_dispatcher_, version_, fetch_event_id_, ServiceWorkerResponse(),
        nullptr /* body_as_stream */, nullptr /* body_as_blob */,
        SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK, dispatch_event_time);
  }

 private:
  // static as version->FinishRequest will remove the calling ResponseCallback
  // instance.
  static void HandleResponse(
      base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher,
      ServiceWorkerVersion* version,
      base::Optional<int> fetch_event_id,
      const ServiceWorkerResponse& response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      blink::mojom::BlobPtr body_as_blob,
      ServiceWorkerFetchEventResult fetch_result,
      base::Time dispatch_event_time) {
    if (!version->FinishRequest(
            fetch_event_id.value(),
            fetch_result == SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
            dispatch_event_time))
      NOTREACHED() << "Should only receive one reply per event";
    // |fetch_dispatcher| is null if the URLRequest was killed.
    if (!fetch_dispatcher)
      return;
    fetch_dispatcher->DidFinish(fetch_event_id.value(), fetch_result, response,
                                std::move(body_as_stream),
                                std::move(body_as_blob));
  }

  mojo::Binding<mojom::ServiceWorkerFetchResponseCallback> binding_;
  base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  // Owns |this| via pending_requests_.
  ServiceWorkerVersion* version_;
  // Must be set to a non-nullopt value before the corresponding mojo
  // handle is passed to the other end (i.e. before any of OnResponse*
  // is called).
  base::Optional<int> fetch_event_id_;

  DISALLOW_COPY_AND_ASSIGN(ResponseCallback);
};

// This class keeps the URL loader related assets alive while the FetchEvent is
// ongoing in the service worker.
class ServiceWorkerFetchDispatcher::URLLoaderAssets
    : public base::RefCounted<ServiceWorkerFetchDispatcher::URLLoaderAssets> {
 public:
  URLLoaderAssets(mojom::URLLoaderFactoryPtr url_loader_factory,
                  std::unique_ptr<mojom::URLLoader> url_loader,
                  std::unique_ptr<DelegatingURLLoaderClient> url_loader_client)
      : url_loader_factory_(std::move(url_loader_factory)),
        url_loader_(std::move(url_loader)),
        url_loader_client_(std::move(url_loader_client)) {}

  void MayBeReportToDevTools(std::pair<int, int> worker_id,
                             int fetch_event_id) {
    url_loader_client_->MayBeReportToDevTools(worker_id, fetch_event_id);
  }

 private:
  friend class base::RefCounted<URLLoaderAssets>;
  virtual ~URLLoaderAssets() {}

  mojom::URLLoaderFactoryPtr url_loader_factory_;
  std::unique_ptr<mojom::URLLoader> url_loader_;
  std::unique_ptr<DelegatingURLLoaderClient> url_loader_client_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderAssets);
};

ServiceWorkerFetchDispatcher::ServiceWorkerFetchDispatcher(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    ServiceWorkerVersion* version,
    ResourceType resource_type,
    const base::Optional<base::TimeDelta>& timeout,
    const net::NetLogWithSource& net_log,
    const base::Closure& prepare_callback,
    const FetchCallback& fetch_callback)
    : version_(version),
      net_log_(net_log),
      prepare_callback_(prepare_callback),
      fetch_callback_(fetch_callback),
      request_(std::move(request)),
      resource_type_(resource_type),
      timeout_(timeout),
      did_complete_(false),
      weak_factory_(this) {
  net_log_.BeginEvent(net::NetLogEventType::SERVICE_WORKER_DISPATCH_FETCH_EVENT,
                      net::NetLog::StringCallback(
                          "event_type", ServiceWorkerMetrics::EventTypeToString(
                                            GetEventType())));
}

ServiceWorkerFetchDispatcher::~ServiceWorkerFetchDispatcher() {
  if (!did_complete_)
    net_log_.EndEvent(
        net::NetLogEventType::SERVICE_WORKER_DISPATCH_FETCH_EVENT);
}

void ServiceWorkerFetchDispatcher::Run() {
  DCHECK(version_->status() == ServiceWorkerVersion::ACTIVATING ||
         version_->status() == ServiceWorkerVersion::ACTIVATED)
      << version_->status();

  if (version_->status() == ServiceWorkerVersion::ACTIVATING) {
    net_log_.BeginEvent(
        net::NetLogEventType::SERVICE_WORKER_WAIT_FOR_ACTIVATION);
    version_->RegisterStatusChangeCallback(
        base::BindOnce(&ServiceWorkerFetchDispatcher::DidWaitForActivation,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  StartWorker();
}

void ServiceWorkerFetchDispatcher::DidWaitForActivation() {
  net_log_.EndEvent(net::NetLogEventType::SERVICE_WORKER_WAIT_FOR_ACTIVATION);
  StartWorker();
}

void ServiceWorkerFetchDispatcher::StartWorker() {
  // We might be REDUNDANT if a new worker started activating and kicked us out
  // before we could finish activation.
  if (version_->status() != ServiceWorkerVersion::ACTIVATED) {
    DCHECK_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());
    DidFail(SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED);
    return;
  }

  if (version_->running_status() == EmbeddedWorkerStatus::RUNNING) {
    DispatchFetchEvent();
    return;
  }

  net_log_.BeginEvent(net::NetLogEventType::SERVICE_WORKER_START_WORKER);
  version_->RunAfterStartWorker(
      GetEventType(),
      base::BindOnce(&ServiceWorkerFetchDispatcher::DidStartWorker,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&ServiceWorkerFetchDispatcher::DidFailToStartWorker,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerFetchDispatcher::DidStartWorker() {
  net_log_.EndEvent(net::NetLogEventType::SERVICE_WORKER_START_WORKER);
  DispatchFetchEvent();
}

void ServiceWorkerFetchDispatcher::DidFailToStartWorker(
    ServiceWorkerStatusCode status) {
  EndNetLogEventWithServiceWorkerStatus(
      net_log_, net::NetLogEventType::SERVICE_WORKER_START_WORKER, status);
  DidFail(status);
}

void ServiceWorkerFetchDispatcher::DispatchFetchEvent() {
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status())
      << "Worker stopped too soon after it was started.";
  DCHECK(!prepare_callback_.is_null());
  base::Closure prepare_callback = prepare_callback_;
  prepare_callback.Run();

  mojom::ServiceWorkerFetchResponseCallbackPtr mojo_response_callback_ptr;
  auto response_callback = base::MakeUnique<ResponseCallback>(
      mojo::MakeRequest(&mojo_response_callback_ptr),
      weak_factory_.GetWeakPtr(), version_.get());
  ResponseCallback* response_callback_rawptr = response_callback.get();

  net_log_.BeginEvent(net::NetLogEventType::SERVICE_WORKER_FETCH_EVENT);
  int fetch_event_id;
  int event_finish_id;
  if (timeout_) {
    fetch_event_id = version_->StartRequestWithCustomTimeout(
        GetEventType(),
        base::BindOnce(&ServiceWorkerFetchDispatcher::DidFailToDispatch,
                       weak_factory_.GetWeakPtr(),
                       std::move(response_callback)),
        *timeout_, ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);
    event_finish_id = version_->StartRequestWithCustomTimeout(
        FetchTypeToWaitUntilEventType(request_->fetch_type),
        base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback), *timeout_,
        ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);
  } else {
    fetch_event_id = version_->StartRequest(
        GetEventType(),
        base::BindOnce(&ServiceWorkerFetchDispatcher::DidFailToDispatch,
                       weak_factory_.GetWeakPtr(),
                       std::move(response_callback)));
    event_finish_id = version_->StartRequest(
        FetchTypeToWaitUntilEventType(request_->fetch_type),
        base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
  }

  response_callback_rawptr->set_fetch_event_id(fetch_event_id);

  if (url_loader_assets_) {
    url_loader_assets_->MayBeReportToDevTools(
        std::make_pair(
            version_->embedded_worker()->process_id(),
            version_->embedded_worker()->worker_devtools_agent_route_id()),
        fetch_event_id);
  }

  // |event_dispatcher| is owned by |version_|. So it is safe to pass the
  // unretained raw pointer of |version_| to OnFetchEventFinished callback.
  // Pass |url_loader_assets_| to the callback to keep the URL loader related
  // assets alive while the FetchEvent is ongoing in the service worker.
  version_->event_dispatcher()->DispatchFetchEvent(
      *request_, std::move(preload_handle_),
      std::move(mojo_response_callback_ptr),
      base::BindOnce(&ServiceWorkerFetchDispatcher::OnFetchEventFinished,
                     base::Unretained(version_.get()), event_finish_id,
                     url_loader_assets_));
}

void ServiceWorkerFetchDispatcher::DidFailToDispatch(
    std::unique_ptr<ResponseCallback> response_callback,
    ServiceWorkerStatusCode status) {
  EndNetLogEventWithServiceWorkerStatus(
      net_log_, net::NetLogEventType::SERVICE_WORKER_FETCH_EVENT, status);
  DidFail(status);
}

void ServiceWorkerFetchDispatcher::DidFail(ServiceWorkerStatusCode status) {
  DCHECK_NE(SERVICE_WORKER_OK, status);
  Complete(status, SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
           ServiceWorkerResponse(), nullptr /* body_as_stream */,
           nullptr /* body_as_blob */);
}

void ServiceWorkerFetchDispatcher::DidFinish(
    int request_id,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    blink::mojom::BlobPtr body_as_blob) {
  net_log_.EndEvent(net::NetLogEventType::SERVICE_WORKER_FETCH_EVENT);
  Complete(SERVICE_WORKER_OK, fetch_result, response, std::move(body_as_stream),
           std::move(body_as_blob));
}

void ServiceWorkerFetchDispatcher::Complete(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    blink::mojom::BlobPtr body_as_blob) {
  DCHECK(!fetch_callback_.is_null());

  did_complete_ = true;
  net_log_.EndEvent(
      net::NetLogEventType::SERVICE_WORKER_DISPATCH_FETCH_EVENT,
      base::Bind(&NetLogFetchEventCallback, status, fetch_result));

  FetchCallback fetch_callback = fetch_callback_;
  scoped_refptr<ServiceWorkerVersion> version = version_;
  fetch_callback.Run(status, fetch_result, response, std::move(body_as_stream),
                     std::move(body_as_blob), version);
}

bool ServiceWorkerFetchDispatcher::MaybeStartNavigationPreload(
    net::URLRequest* original_request,
    base::OnceClosure on_response) {
  if (resource_type_ != RESOURCE_TYPE_MAIN_FRAME &&
      resource_type_ != RESOURCE_TYPE_SUB_FRAME) {
    return false;
  }
  if (!version_->navigation_preload_state().enabled)
    return false;
  // TODO(horo): Currently NavigationPreload doesn't support request body.
  if (!request_->blob_uuid.empty())
    return false;

  ResourceRequestInfoImpl* original_info =
      ResourceRequestInfoImpl::ForRequest(original_request);
  ResourceRequesterInfo* requester_info = original_info->requester_info();
  if (IsBrowserSideNavigationEnabled()) {
    DCHECK(requester_info->IsBrowserSideNavigation());
  } else {
    DCHECK(requester_info->IsRenderer());
    if (!requester_info->filter())
      return false;
  }

  DCHECK(!url_loader_assets_);

  mojom::URLLoaderFactoryPtr url_loader_factory;
  URLLoaderFactoryImpl::Create(
      ResourceRequesterInfo::CreateForNavigationPreload(requester_info),
      mojo::MakeRequest(&url_loader_factory),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));

  ResourceRequest request;
  request.method = original_request->method();
  request.url = original_request->url();
  // TODO(horo): Set site_for_cookies to support Same-site Cookies.
  request.request_initiator = original_request->initiator().has_value()
                                  ? original_request->initiator()
                                  : url::Origin(original_request->url());
  request.referrer = GURL(original_request->referrer());
  request.referrer_policy = original_info->GetReferrerPolicy();
  request.visibility_state = original_info->GetVisibilityState();
  request.load_flags = original_request->load_flags();
  // Set to SUB_RESOURCE because we shouldn't trigger NavigationResourceThrottle
  // for the service worker navigation preload request.
  request.resource_type = RESOURCE_TYPE_SUB_RESOURCE;
  request.priority = original_request->priority();
  request.service_worker_mode = ServiceWorkerMode::NONE;
  request.do_not_prompt_for_login = true;
  request.render_frame_id = original_info->GetRenderFrameID();
  request.is_main_frame = original_info->IsMainFrame();
  request.enable_load_timing = original_info->is_load_timing_enabled();
  request.report_raw_headers = original_info->ShouldReportRawHeaders();

  DCHECK(net::HttpUtil::IsValidHeaderValue(
      version_->navigation_preload_state().header));
  ServiceWorkerMetrics::RecordNavigationPreloadRequestHeaderSize(
      version_->navigation_preload_state().header.length());
  request.headers.CopyFrom(original_request->extra_request_headers());
  request.headers.SetHeader("Service-Worker-Navigation-Preload",
                            version_->navigation_preload_state().header);

  const int request_id = ResourceDispatcherHostImpl::Get()->MakeRequestID();
  DCHECK_LT(request_id, -1);

  preload_handle_ = mojom::FetchEventPreloadHandle::New();
  mojom::URLLoaderClientPtr url_loader_client_ptr;
  preload_handle_->url_loader_client_request =
      mojo::MakeRequest(&url_loader_client_ptr);
  auto url_loader_client = base::MakeUnique<DelegatingURLLoaderClient>(
      std::move(url_loader_client_ptr), std::move(on_response), request);
  mojom::URLLoaderClientPtr url_loader_client_ptr_to_pass;
  url_loader_client->Bind(&url_loader_client_ptr_to_pass);
  mojom::URLLoaderPtr url_loader_associated_ptr;

  url_loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&url_loader_associated_ptr),
      original_info->GetRouteID(), request_id, mojom::kURLLoadOptionNone,
      request, std::move(url_loader_client_ptr_to_pass),
      net::MutableNetworkTrafficAnnotationTag(
          original_request->traffic_annotation()));

  auto url_loader = std::make_unique<DelegatingURLLoader>(
      std::move(url_loader_associated_ptr));
  preload_handle_->url_loader = url_loader->CreateInterfacePtrAndBind();
  url_loader_assets_ = base::MakeRefCounted<URLLoaderAssets>(
      std::move(url_loader_factory), std::move(url_loader),
      std::move(url_loader_client));
  return true;
}

// S13nServiceWorker
bool ServiceWorkerFetchDispatcher::MaybeStartNavigationPreloadWithURLLoader(
    const ResourceRequest& original_request,
    URLLoaderFactoryGetter* url_loader_factory_getter,
    base::OnceClosure on_response) {
  if (resource_type_ != RESOURCE_TYPE_MAIN_FRAME &&
      resource_type_ != RESOURCE_TYPE_SUB_FRAME) {
    return false;
  }
  if (!version_->navigation_preload_state().enabled)
    return false;
  // TODO(horo): Currently NavigationPreload doesn't support request body.
  if (!request_->blob_uuid.empty())
    return false;

  ResourceRequest resource_request(original_request);
  // Set to SUB_RESOURCE because we shouldn't trigger NavigationResourceThrottle
  // for the service worker navigation preload request.
  resource_request.resource_type = RESOURCE_TYPE_SUB_RESOURCE;
  resource_request.service_worker_mode = ServiceWorkerMode::NONE;
  resource_request.do_not_prompt_for_login = true;
  DCHECK(net::HttpUtil::IsValidHeaderValue(
      version_->navigation_preload_state().header));
  // TODO(crbug/762357): Record header size UMA, but not until *all* the
  // navigation preload metrics are recorded on the S13N path; otherwise the
  // metrics will get unbalanced.
  // ServiceWorkerMetrics::RecordNavigationPreloadRequestHeaderSize(
  //     version_->navigation_preload_state().header.length());
  resource_request.headers.SetHeader(
      "Service-Worker-Navigation-Preload",
      version_->navigation_preload_state().header);

  preload_handle_ = mojom::FetchEventPreloadHandle::New();

  // Create the DelegatingURLLoaderClient, which becomes the
  // URLLoaderClient for the navigation preload network request.
  mojom::URLLoaderClientPtr url_loader_client_ptr;
  preload_handle_->url_loader_client_request =
      mojo::MakeRequest(&url_loader_client_ptr);
  auto url_loader_client = base::MakeUnique<DelegatingURLLoaderClient>(
      std::move(url_loader_client_ptr), std::move(on_response),
      resource_request);

  // Start the network request for the URL using the network loader.
  // TODO(falken): What to do about routing_id, request_id?
  mojom::URLLoaderClientPtr url_loader_client_ptr_to_pass;
  url_loader_client->Bind(&url_loader_client_ptr_to_pass);
  mojom::URLLoaderPtr url_loader_associated_ptr;
  url_loader_factory_getter->GetNetworkFactory()->get()->CreateLoaderAndStart(
      mojo::MakeRequest(&url_loader_associated_ptr), -1 /* routing_id? */,
      -1 /* request_id? */, mojom::kURLLoadOptionNone, resource_request,
      std::move(url_loader_client_ptr_to_pass),
      net::MutableNetworkTrafficAnnotationTag(
          kNavigationPreloadTrafficAnnotation));

  // Hook the load up to DelegatingURLLoader, which will call our
  // DelegatingURLLoaderClient.
  auto url_loader = std::make_unique<DelegatingURLLoader>(
      std::move(url_loader_associated_ptr));
  preload_handle_->url_loader = url_loader->CreateInterfacePtrAndBind();

  DCHECK(!url_loader_assets_);
  // Unlike the non-S13N code path, we don't own the URLLoaderFactory being used
  // (it's the generic network factory), so we don't need to pass it to
  // URLLoaderAssets to keep it alive.
  mojom::URLLoaderFactoryPtr null_factory;
  url_loader_assets_ = base::MakeRefCounted<URLLoaderAssets>(
      std::move(null_factory), std::move(url_loader),
      std::move(url_loader_client));
  return true;
}

ServiceWorkerMetrics::EventType ServiceWorkerFetchDispatcher::GetEventType()
    const {
  if (request_->fetch_type == ServiceWorkerFetchType::FOREIGN_FETCH)
    return ServiceWorkerMetrics::EventType::FOREIGN_FETCH;
  return ResourceTypeToEventType(resource_type_);
}

// static
void ServiceWorkerFetchDispatcher::OnFetchEventFinished(
    ServiceWorkerVersion* version,
    int event_finish_id,
    scoped_refptr<URLLoaderAssets> url_loader_assets,
    blink::mojom::ServiceWorkerEventStatus status,
    base::Time dispatch_event_time) {
  version->FinishRequest(
      event_finish_id,
      status != blink::mojom::ServiceWorkerEventStatus::ABORTED,
      dispatch_event_time);
}

}  // namespace content
