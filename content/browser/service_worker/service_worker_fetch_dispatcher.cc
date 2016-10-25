// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/fetch_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/common/url_loader.mojom.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

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

void OnFetchEventFinished(ServiceWorkerVersion* version,
                          int event_finish_id,
                          ServiceWorkerStatusCode status,
                          base::Time dispatch_event_time) {
  version->FinishRequest(event_finish_id, status != SERVICE_WORKER_ERROR_ABORT,
                         dispatch_event_time);
}

}  // namespace

// Helper to receive the fetch event response even if
// ServiceWorkerFetchDispatcher has been destroyed.
class ServiceWorkerFetchDispatcher::ResponseCallback {
 public:
  ResponseCallback(base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher,
                   ServiceWorkerVersion* version)
      : fetch_dispatcher_(fetch_dispatcher), version_(version) {}

  void Run(int request_id,
           ServiceWorkerFetchEventResult fetch_result,
           const ServiceWorkerResponse& response,
           base::Time dispatch_event_time) {
    const bool handled =
        (fetch_result == SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE);
    if (!version_->FinishRequest(request_id, handled, dispatch_event_time))
      NOTREACHED() << "Should only receive one reply per event";

    // |fetch_dispatcher| is null if the URLRequest was killed.
    if (fetch_dispatcher_)
      fetch_dispatcher_->DidFinish(request_id, fetch_result, response);
  }

 private:
  base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  // Owns |this|.
  ServiceWorkerVersion* version_;

  DISALLOW_COPY_AND_ASSIGN(ResponseCallback);
};

ServiceWorkerFetchDispatcher::ServiceWorkerFetchDispatcher(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    ServiceWorkerVersion* version,
    ResourceType resource_type,
    const net::NetLogWithSource& net_log,
    const base::Closure& prepare_callback,
    const FetchCallback& fetch_callback)
    : version_(version),
      net_log_(net_log),
      prepare_callback_(prepare_callback),
      fetch_callback_(fetch_callback),
      request_(std::move(request)),
      resource_type_(resource_type),
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
        base::Bind(&ServiceWorkerFetchDispatcher::DidWaitForActivation,
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
      GetEventType(), base::Bind(&ServiceWorkerFetchDispatcher::DidStartWorker,
                                 weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceWorkerFetchDispatcher::DidFailToStartWorker,
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

  net_log_.BeginEvent(net::NetLogEventType::SERVICE_WORKER_FETCH_EVENT);
  int fetch_event_id = version_->StartRequest(
      GetEventType(),
      base::Bind(&ServiceWorkerFetchDispatcher::DidFailToDispatch,
                 weak_factory_.GetWeakPtr()));
  int event_finish_id = version_->StartRequest(
      FetchTypeToWaitUntilEventType(request_->fetch_type),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));

  ResponseCallback* response_callback =
      new ResponseCallback(weak_factory_.GetWeakPtr(), version_.get());
  version_->RegisterRequestCallback<ServiceWorkerHostMsg_FetchEventResponse>(
      fetch_event_id,
      base::Bind(&ServiceWorkerFetchDispatcher::ResponseCallback::Run,
                 base::Owned(response_callback)));

  base::WeakPtr<mojom::FetchEventDispatcher> dispatcher =
      version_->GetMojoServiceForRequest<mojom::FetchEventDispatcher>(
          event_finish_id);
  // |dispatcher| is owned by |version_|. So it is safe to pass the unretained
  // raw pointer of |version_| to OnFetchEventFinished callback.
  dispatcher->DispatchFetchEvent(
      fetch_event_id, *request_, std::move(preload_handle_),
      base::Bind(&OnFetchEventFinished, base::Unretained(version_.get()),
                 event_finish_id));
}

void ServiceWorkerFetchDispatcher::DidFailToDispatch(
    ServiceWorkerStatusCode status) {
  EndNetLogEventWithServiceWorkerStatus(
      net_log_, net::NetLogEventType::SERVICE_WORKER_FETCH_EVENT, status);
  DidFail(status);
}

void ServiceWorkerFetchDispatcher::DidFail(ServiceWorkerStatusCode status) {
  DCHECK_NE(SERVICE_WORKER_OK, status);
  Complete(status, SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
           ServiceWorkerResponse());
}

void ServiceWorkerFetchDispatcher::DidFinish(
    int request_id,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response) {
  net_log_.EndEvent(net::NetLogEventType::SERVICE_WORKER_FETCH_EVENT);
  Complete(SERVICE_WORKER_OK, fetch_result, response);
}

void ServiceWorkerFetchDispatcher::Complete(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response) {
  DCHECK(!fetch_callback_.is_null());

  did_complete_ = true;
  net_log_.EndEvent(
      net::NetLogEventType::SERVICE_WORKER_DISPATCH_FETCH_EVENT,
      base::Bind(&NetLogFetchEventCallback, status, fetch_result));

  FetchCallback fetch_callback = fetch_callback_;
  scoped_refptr<ServiceWorkerVersion> version = version_;
  fetch_callback.Run(status, fetch_result, response, version);
}

void ServiceWorkerFetchDispatcher::MaybeStartNavigationPreload(
    net::URLRequest* original_request,
    const MojoURLLoaderFactoryGetter& url_loader_factory_getter) {
  if (resource_type_ != RESOURCE_TYPE_MAIN_FRAME &&
      resource_type_ != RESOURCE_TYPE_SUB_FRAME) {
    return;
  }
  if (!version_->navigation_preload_enabled())
    return;
  // TODO(horo): Currently NavigationPreload doesn't support request body.
  if (!request_->blob_uuid.empty())
    return;
  if (!base::FeatureList::IsEnabled(
          features::kServiceWorkerNavigationPreload)) {
    // TODO(horo): Check |version_|'s origin_trial_tokens() here if we use
    // Origin-Trial for NavigationPreload.
    return;
  }
  if (IsBrowserSideNavigationEnabled()) {
    // TODO(horo): Support NavigationPreload with PlzNavigate.
    NOTIMPLEMENTED();
    return;
  }
  DCHECK(!url_loader_factory_getter.is_null());
  mojom::URLLoaderFactoryPtr factory;
  url_loader_factory_getter.Run(mojo::GetProxy(&factory));
  if (url_loader_factory_getter.IsCancelled())
    return;

  preload_handle_ = mojom::FetchEventPreloadHandle::New();
  const ResourceRequestInfoImpl* original_info =
      ResourceRequestInfoImpl::ForRequest(original_request);

  mojom::URLLoaderClientPtr url_loader_client;
  preload_handle_->url_loader_client_request = GetProxy(&url_loader_client);

  ResourceRequest request;
  request.method = original_request->method();
  request.url = original_request->url();
  request.referrer = GURL(original_request->referrer());
  request.referrer_policy = original_info->GetReferrerPolicy();
  request.visibility_state = original_info->GetVisibilityState();
  request.load_flags = original_request->load_flags();
  // Set to SUB_RESOURCE because we shouldn't trigger NavigationResourceThrottle
  // for the service worker navigation preload request.
  request.resource_type = RESOURCE_TYPE_SUB_RESOURCE;
  request.priority = original_request->priority();
  request.skip_service_worker = SkipServiceWorker::ALL;
  request.do_not_prompt_for_login = true;
  request.render_frame_id = original_info->GetRenderFrameID();
  request.is_main_frame = original_info->IsMainFrame();
  request.parent_is_main_frame = original_info->ParentIsMainFrame();
  const int request_id = ResourceDispatcherHostImpl::Get()->MakeRequestID();
  DCHECK_LT(request_id, -1);
  // TODO(horo): Add "Service-Worker-Navigation-Preload" header.
  // See: https://github.com/w3c/ServiceWorker/issues/920#issuecomment-251150270
  factory->CreateLoaderAndStart(GetProxy(&preload_handle_->url_loader),
                                original_info->GetRouteID(), request_id,
                                request, std::move(url_loader_client));
}

ServiceWorkerMetrics::EventType ServiceWorkerFetchDispatcher::GetEventType()
    const {
  if (request_->fetch_type == ServiceWorkerFetchType::FOREIGN_FETCH)
    return ServiceWorkerMetrics::EventType::FOREIGN_FETCH;
  return ResourceTypeToEventType(resource_type_);
}

}  // namespace content
