// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_context_client.h"

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/child/background_sync/background_sync_type_converters.h"
#include "content/child/notifications/notification_data_conversions.h"
#include "content/child/request_extra_data.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_network_provider.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_provider_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/web_data_consumer_handle_impl.h"
#include "content/child/web_url_loader_impl.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/devtools_messages.h"
#include "content/common/message_port_messages.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/push_event_payload.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/renderer/devtools/devtools_agent.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/embedded_worker_devtools_agent.h"
#include "content/renderer/service_worker/embedded_worker_dispatcher.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/service_worker/service_worker_type_util.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationData.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerClientQueryOptions.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextClient.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextProxy.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerNetworkProvider.h"

namespace content {

namespace {

// For now client must be a per-thread instance.
base::LazyInstance<base::ThreadLocalPointer<ServiceWorkerContextClient>>::
    Leaky g_worker_client_tls = LAZY_INSTANCE_INITIALIZER;

void CallWorkerContextDestroyedOnMainThread(int embedded_worker_id) {
  if (!RenderThreadImpl::current() ||
      !RenderThreadImpl::current()->embedded_worker_dispatcher())
    return;
  RenderThreadImpl::current()->embedded_worker_dispatcher()->
      WorkerContextDestroyed(embedded_worker_id);
}

// We store an instance of this class in the "extra data" of the WebDataSource
// and attach a ServiceWorkerNetworkProvider to it as base::UserData.
// (see createServiceWorkerNetworkProvider).
class DataSourceExtraData
    : public blink::WebDataSource::ExtraData,
      public base::SupportsUserData {
 public:
  DataSourceExtraData() {}
  ~DataSourceExtraData() override {}
};

// Called on the main thread only and blink owns it.
class WebServiceWorkerNetworkProviderImpl
    : public blink::WebServiceWorkerNetworkProvider {
 public:
  // Blink calls this method for each request starting with the main script,
  // we tag them with the provider id.
  void willSendRequest(blink::WebDataSource* data_source,
                       blink::WebURLRequest& request) override {
    ServiceWorkerNetworkProvider* provider =
        ServiceWorkerNetworkProvider::FromDocumentState(
            static_cast<DataSourceExtraData*>(data_source->getExtraData()));
    std::unique_ptr<RequestExtraData> extra_data(new RequestExtraData);
    extra_data->set_service_worker_provider_id(provider->provider_id());
    extra_data->set_originated_from_service_worker(true);
    // Service workers are only available in secure contexts, so all requests
    // are initiated in a secure context.
    extra_data->set_initiated_in_secure_context(true);
    request.setExtraData(extra_data.release());
  }
};

ServiceWorkerStatusCode EventResultToStatus(
    blink::WebServiceWorkerEventResult result) {
  switch (result) {
    case blink::WebServiceWorkerEventResultCompleted:
      return SERVICE_WORKER_OK;
    case blink::WebServiceWorkerEventResultRejected:
      return SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED;
  }
  NOTREACHED() << "Got invalid result: " << result;
  return SERVICE_WORKER_ERROR_FAILED;
}

void SendPostMessageToClientOnMainThread(
    ThreadSafeSender* sender,
    int routing_id,
    const std::string& uuid,
    const base::string16& message,
    std::unique_ptr<blink::WebMessagePortChannelArray> channels) {
  sender->Send(new ServiceWorkerHostMsg_PostMessageToClient(
      routing_id, uuid, message,
      WebMessagePortChannelImpl::ExtractMessagePortIDs(std::move(channels))));
}

blink::WebURLRequest::FetchRequestMode GetBlinkFetchRequestMode(
    FetchRequestMode mode) {
  return static_cast<blink::WebURLRequest::FetchRequestMode>(mode);
}

blink::WebURLRequest::FetchCredentialsMode GetBlinkFetchCredentialsMode(
    FetchCredentialsMode credentials_mode) {
  return static_cast<blink::WebURLRequest::FetchCredentialsMode>(
      credentials_mode);
}

blink::WebURLRequest::FetchRedirectMode GetBlinkFetchRedirectMode(
    FetchRedirectMode redirect_mode) {
  return static_cast<blink::WebURLRequest::FetchRedirectMode>(redirect_mode);
}

blink::WebURLRequest::RequestContext GetBlinkRequestContext(
    RequestContextType request_context_type) {
  return static_cast<blink::WebURLRequest::RequestContext>(
      request_context_type);
}

blink::WebURLRequest::FrameType GetBlinkFrameType(
    RequestContextFrameType frame_type) {
  return static_cast<blink::WebURLRequest::FrameType>(frame_type);
}

blink::WebServiceWorkerClientInfo
ToWebServiceWorkerClientInfo(const ServiceWorkerClientInfo& client_info) {
  DCHECK(client_info.IsValid());

  blink::WebServiceWorkerClientInfo web_client_info;

  web_client_info.uuid = blink::WebString::fromASCII(client_info.client_uuid);
  web_client_info.pageVisibilityState = client_info.page_visibility_state;
  web_client_info.isFocused = client_info.is_focused;
  web_client_info.url = client_info.url;
  web_client_info.frameType = GetBlinkFrameType(client_info.frame_type);
  web_client_info.clientType = client_info.client_type;

  return web_client_info;
}

// Use this template in willDestroyWorkerContext to abort all the pending
// events callbacks.
template <typename T>
void AbortPendingEventCallbacks(T& callbacks) {
  for (typename T::iterator it(&callbacks); !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->Run(SERVICE_WORKER_ERROR_ABORT, base::Time::Now());
  }
}

}  // namespace

// Holding data that needs to be bound to the worker context on the
// worker thread.
struct ServiceWorkerContextClient::WorkerContextData {
  using ClientsCallbacksMap =
      IDMap<std::unique_ptr<blink::WebServiceWorkerClientsCallbacks>>;
  using ClaimClientsCallbacksMap =
      IDMap<std::unique_ptr<blink::WebServiceWorkerClientsClaimCallbacks>>;
  using ClientCallbacksMap =
      IDMap<std::unique_ptr<blink::WebServiceWorkerClientCallbacks>>;
  using SkipWaitingCallbacksMap =
      IDMap<std::unique_ptr<blink::WebServiceWorkerSkipWaitingCallbacks>>;
  using SyncEventCallbacksMap = IDMap<std::unique_ptr<const SyncCallback>>;
  using PushEventCallbacksMap =
      IDMap<std::unique_ptr<const DispatchPushEventCallback>>;
  using FetchEventCallbacksMap = IDMap<std::unique_ptr<const FetchCallback>>;
  using ExtendableMessageEventCallbacksMap =
      IDMap<std::unique_ptr<const DispatchExtendableMessageEventCallback>>;
  using NavigationPreloadRequestsMap = IDMap<
      std::unique_ptr<ServiceWorkerContextClient::NavigationPreloadRequest>>;

  explicit WorkerContextData(ServiceWorkerContextClient* owner)
      : event_dispatcher_binding(owner),
        weak_factory(owner),
        proxy_weak_factory(owner->proxy_) {}

  ~WorkerContextData() {
    DCHECK(thread_checker.CalledOnValidThread());
  }

  mojo::Binding<mojom::ServiceWorkerEventDispatcher> event_dispatcher_binding;

  // Pending callbacks for GetClientDocuments().
  ClientsCallbacksMap clients_callbacks;

  // Pending callbacks for OpenWindow() and FocusClient().
  ClientCallbacksMap client_callbacks;

  // Pending callbacks for SkipWaiting().
  SkipWaitingCallbacksMap skip_waiting_callbacks;

  // Pending callbacks for ClaimClients().
  ClaimClientsCallbacksMap claim_clients_callbacks;

  // Pending callbacks for Background Sync Events.
  SyncEventCallbacksMap sync_event_callbacks;

  // Pending callbacks for Push Events.
  PushEventCallbacksMap push_event_callbacks;

  // Pending callbacks for Fetch Events.
  FetchEventCallbacksMap fetch_event_callbacks;

  // Pending callbacks for Extendable Message Events.
  ExtendableMessageEventCallbacksMap message_event_callbacks;

  // Pending navigation preload requests.
  NavigationPreloadRequestsMap preload_requests;

  base::ThreadChecker thread_checker;
  base::WeakPtrFactory<ServiceWorkerContextClient> weak_factory;
  base::WeakPtrFactory<blink::WebServiceWorkerContextProxy> proxy_weak_factory;
};

class ServiceWorkerContextClient::NavigationPreloadRequest final
    : public mojom::URLLoaderClient {
 public:
  NavigationPreloadRequest(int fetch_event_id,
                           const GURL& url,
                           mojom::FetchEventPreloadHandlePtr preload_handle)
      : fetch_event_id_(fetch_event_id),
        url_(url),
        url_loader_(std::move(preload_handle->url_loader)),
        binding_(this, std::move(preload_handle->url_loader_client_request)) {}

  ~NavigationPreloadRequest() override {
  }

  void OnReceiveResponse(
      const ResourceResponseHead& response_head,
      mojom::DownloadedTempFileAssociatedPtrInfo downloaded_file) override {
    DCHECK(!response_);
    DCHECK(!downloaded_file.is_valid());
    response_ = base::MakeUnique<blink::WebURLResponse>();
    // TODO(horo): Set report_security_info to true when DevTools is attached.
    const bool report_security_info = false;
    WebURLLoaderImpl::PopulateURLResponse(url_, response_head, response_.get(),
                                          report_security_info);
    MaybeReportResponseToClient();
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override {
    // This will delete |this|.
    ReportErrorToClient(
        "Service Worker navigation preload doesn't support redirects.");
  }

  void OnDataDownloaded(int64_t data_length,
                        int64_t encoded_data_length) override {
    NOTREACHED();
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        const base::Closure& ack_callback) override {
    NOTREACHED();
  }

  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {}

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    // TODO(horo): Send this transfer size update notification to DevTools.
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    DCHECK(!body_.is_valid());
    body_ = std::move(body);
    MaybeReportResponseToClient();
  }

  void OnComplete(const ResourceRequestCompletionStatus& status) override {
    if (status.error_code != net::OK) {
      // This will delete |this|.
      ReportErrorToClient("Service Worker navigation preload network error.");
      return;
    }

    ServiceWorkerContextClient* client =
        ServiceWorkerContextClient::ThreadSpecificInstance();
    if (!client)
      return;
    // This will delete |this|.
    client->OnNavigationPreloadComplete(fetch_event_id_,
                                        status.encoded_data_length);
  }

 private:
  void MaybeReportResponseToClient() {
    if (!response_ || !body_.is_valid())
      return;
    ServiceWorkerContextClient* client =
        ServiceWorkerContextClient::ThreadSpecificInstance();
    if (!client)
      return;

    client->OnNavigationPreloadResponse(
        fetch_event_id_, std::move(response_),
        base::MakeUnique<WebDataConsumerHandleImpl>(std::move(body_)));
  }

  void ReportErrorToClient(const char* error_message) {
    ServiceWorkerContextClient* client =
        ServiceWorkerContextClient::ThreadSpecificInstance();
    if (!client)
      return;
    // This will delete |this|.
    client->OnNavigationPreloadError(
        fetch_event_id_, base::MakeUnique<blink::WebServiceWorkerError>(
                             blink::WebServiceWorkerError::ErrorTypeNetwork,
                             blink::WebString::fromUTF8(error_message)));
  }

  const int fetch_event_id_;
  const GURL url_;
  mojom::URLLoaderPtr url_loader_;
  mojo::Binding<mojom::URLLoaderClient> binding_;

  std::unique_ptr<blink::WebURLResponse> response_;
  mojo::ScopedDataPipeConsumerHandle body_;
};

ServiceWorkerContextClient*
ServiceWorkerContextClient::ThreadSpecificInstance() {
  return g_worker_client_tls.Pointer()->Get();
}

ServiceWorkerContextClient::ServiceWorkerContextClient(
    int embedded_worker_id,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url,
    mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
    std::unique_ptr<EmbeddedWorkerInstanceClientImpl> embedded_worker_client)
    : embedded_worker_id_(embedded_worker_id),
      service_worker_version_id_(service_worker_version_id),
      service_worker_scope_(service_worker_scope),
      script_url_(script_url),
      sender_(ChildThreadImpl::current()->thread_safe_sender()),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      proxy_(nullptr),
      pending_dispatcher_request_(std::move(dispatcher_request)),
      embedded_worker_client_(std::move(embedded_worker_client)) {
  TRACE_EVENT_ASYNC_BEGIN0("ServiceWorker",
                           "ServiceWorkerContextClient::StartingWorkerContext",
                           this);
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerContextClient::StartingWorkerContext",
      this,
      "PrepareWorker");
}

ServiceWorkerContextClient::~ServiceWorkerContextClient() {}

void ServiceWorkerContextClient::OnMessageReceived(
    int thread_id,
    int embedded_worker_id,
    const IPC::Message& message) {
  CHECK_EQ(embedded_worker_id_, embedded_worker_id);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerContextClient, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ActivateEvent, OnActivateEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_InstallEvent, OnInstallEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_NotificationClickEvent,
                        OnNotificationClickEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_NotificationCloseEvent,
                        OnNotificationCloseEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetClient, OnDidGetClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetClients, OnDidGetClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_OpenWindowResponse,
                        OnOpenWindowResponse)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_OpenWindowError,
                        OnOpenWindowError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FocusClientResponse,
                        OnFocusClientResponse)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_NavigateClientResponse,
                        OnNavigateClientResponse)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_NavigateClientError,
                        OnNavigateClientError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidSkipWaiting, OnDidSkipWaiting)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidClaimClients, OnDidClaimClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ClaimClientsError, OnClaimClientsError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_Ping, OnPing);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
}

blink::WebURL ServiceWorkerContextClient::scope() const {
  return service_worker_scope_;
}

void ServiceWorkerContextClient::getClient(
    const blink::WebString& id,
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id = context_->client_callbacks.Add(std::move(callbacks));
  Send(new ServiceWorkerHostMsg_GetClient(GetRoutingID(), request_id,
                                          id.utf8()));
}

void ServiceWorkerContextClient::getClients(
    const blink::WebServiceWorkerClientQueryOptions& weboptions,
    std::unique_ptr<blink::WebServiceWorkerClientsCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id = context_->clients_callbacks.Add(std::move(callbacks));
  ServiceWorkerClientQueryOptions options;
  options.client_type = weboptions.clientType;
  options.include_uncontrolled = weboptions.includeUncontrolled;
  Send(new ServiceWorkerHostMsg_GetClients(
      GetRoutingID(), request_id, options));
}

void ServiceWorkerContextClient::openWindow(
    const blink::WebURL& url,
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id = context_->client_callbacks.Add(std::move(callbacks));
  Send(new ServiceWorkerHostMsg_OpenWindow(
      GetRoutingID(), request_id, url));
}

void ServiceWorkerContextClient::setCachedMetadata(const blink::WebURL& url,
                                                    const char* data,
                                                    size_t size) {
  std::vector<char> copy(data, data + size);
  Send(new ServiceWorkerHostMsg_SetCachedMetadata(GetRoutingID(), url, copy));
}

void ServiceWorkerContextClient::clearCachedMetadata(
    const blink::WebURL& url) {
  Send(new ServiceWorkerHostMsg_ClearCachedMetadata(GetRoutingID(), url));
}

void ServiceWorkerContextClient::workerReadyForInspection() {
  Send(new EmbeddedWorkerHostMsg_WorkerReadyForInspection(embedded_worker_id_));
}

void ServiceWorkerContextClient::workerContextFailedToStart() {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!proxy_);

  Send(new EmbeddedWorkerHostMsg_WorkerScriptLoadFailed(embedded_worker_id_));

  RenderThreadImpl::current()->embedded_worker_dispatcher()->
      WorkerContextDestroyed(embedded_worker_id_);
}

void ServiceWorkerContextClient::workerScriptLoaded() {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!proxy_);

  Send(new EmbeddedWorkerHostMsg_WorkerScriptLoaded(embedded_worker_id_));
}

bool ServiceWorkerContextClient::hasAssociatedRegistration() {
  return provider_context_ && provider_context_->HasAssociatedRegistration();
}

void ServiceWorkerContextClient::workerContextStarted(
    blink::WebServiceWorkerContextProxy* proxy) {
  DCHECK(!worker_task_runner_.get());
  DCHECK_NE(0, WorkerThread::GetCurrentId());
  worker_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  // g_worker_client_tls.Pointer()->Get() could return NULL if this context
  // gets deleted before workerContextStarted() is called.
  DCHECK(g_worker_client_tls.Pointer()->Get() == NULL);
  DCHECK(!proxy_);
  g_worker_client_tls.Pointer()->Set(this);
  proxy_ = proxy;

  // Initialize pending callback maps. This needs to be freed on the
  // same thread before the worker context goes away in
  // willDestroyWorkerContext.
  context_.reset(new WorkerContextData(this));

  ServiceWorkerRegistrationObjectInfo registration_info;
  ServiceWorkerVersionAttributes version_attrs;
  provider_context_->GetAssociatedRegistration(&registration_info,
                                               &version_attrs);
  DCHECK_NE(registration_info.registration_id,
            kInvalidServiceWorkerRegistrationId);

  DCHECK(pending_dispatcher_request_.is_pending());
  DCHECK(!context_->event_dispatcher_binding.is_bound());
  context_->event_dispatcher_binding.Bind(
      std::move(pending_dispatcher_request_));

  SetRegistrationInServiceWorkerGlobalScope(registration_info, version_attrs);

  Send(new EmbeddedWorkerHostMsg_WorkerThreadStarted(
      embedded_worker_id_, WorkerThread::GetCurrentId(),
      provider_context_->provider_id()));

  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerContextClient::StartingWorkerContext",
      this,
      "ExecuteScript");
}

void ServiceWorkerContextClient::didEvaluateWorkerScript(bool success) {
  Send(new EmbeddedWorkerHostMsg_WorkerScriptEvaluated(
      embedded_worker_id_, success));

  // Schedule a task to send back WorkerStarted asynchronously,
  // so that at the time we send it we can be sure that the
  // worker run loop has been started.
  worker_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ServiceWorkerContextClient::SendWorkerStarted,
                            GetWeakPtr()));
}

void ServiceWorkerContextClient::didInitializeWorkerContext(
    v8::Local<v8::Context> context) {
  GetContentClient()
      ->renderer()
      ->DidInitializeServiceWorkerContextOnWorkerThread(
          context, service_worker_version_id_, script_url_);
}

void ServiceWorkerContextClient::willDestroyWorkerContext(
    v8::Local<v8::Context> context) {
  // At this point WillStopCurrentWorkerThread is already called, so
  // worker_task_runner_->RunsTasksOnCurrentThread() returns false
  // (while we're still on the worker thread).
  proxy_ = NULL;

  // Aborts all the pending events callbacks.
  AbortPendingEventCallbacks(context_->sync_event_callbacks);
  AbortPendingEventCallbacks(context_->push_event_callbacks);
  AbortPendingEventCallbacks(context_->fetch_event_callbacks);
  AbortPendingEventCallbacks(context_->message_event_callbacks);

  // We have to clear callbacks now, as they need to be freed on the
  // same thread.
  context_.reset();

  // This also lets the message filter stop dispatching messages to
  // this client.
  g_worker_client_tls.Pointer()->Set(NULL);

  GetContentClient()->renderer()->WillDestroyServiceWorkerContextOnWorkerThread(
      context, service_worker_version_id_, script_url_);
}

void ServiceWorkerContextClient::workerContextDestroyed() {
  DCHECK(g_worker_client_tls.Pointer()->Get() == NULL);

  // Check if mojo is enabled
  if (ServiceWorkerUtils::IsMojoForServiceWorkerEnabled()) {
    DCHECK(embedded_worker_client_);
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&EmbeddedWorkerInstanceClientImpl::StopWorkerCompleted,
                   base::Passed(&embedded_worker_client_)));
    return;
  }

  // Now we should be able to free the WebEmbeddedWorker container on the
  // main thread.
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CallWorkerContextDestroyedOnMainThread,
                 embedded_worker_id_));
}

void ServiceWorkerContextClient::reportException(
    const blink::WebString& error_message,
    int line_number,
    int column_number,
    const blink::WebString& source_url) {
  Send(new EmbeddedWorkerHostMsg_ReportException(
      embedded_worker_id_, error_message.utf16(), line_number, column_number,
      blink::WebStringToGURL(source_url)));
}

void ServiceWorkerContextClient::reportConsoleMessage(
    int source,
    int level,
    const blink::WebString& message,
    int line_number,
    const blink::WebString& source_url) {
  EmbeddedWorkerHostMsg_ReportConsoleMessage_Params params;
  params.source_identifier = source;
  params.message_level = level;
  params.message = message.utf16();
  params.line_number = line_number;
  params.source_url = blink::WebStringToGURL(source_url);

  Send(new EmbeddedWorkerHostMsg_ReportConsoleMessage(
      embedded_worker_id_, params));
}

void ServiceWorkerContextClient::sendDevToolsMessage(
    int session_id,
    int call_id,
    const blink::WebString& message,
    const blink::WebString& state_cookie) {
  embedded_worker_client_->devtools_agent()->SendMessage(
      sender_.get(), session_id, call_id, message.utf8(), state_cookie.utf8());
}

blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
ServiceWorkerContextClient::createDevToolsMessageLoop() {
  return DevToolsAgent::createMessageLoopWrapper();
}

void ServiceWorkerContextClient::didHandleActivateEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  Send(new ServiceWorkerHostMsg_ActivateEventFinished(
      GetRoutingID(), request_id, result,
      base::Time::FromDoubleT(event_dispatch_time)));
}

void ServiceWorkerContextClient::didHandleExtendableMessageEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  const DispatchExtendableMessageEventCallback* callback =
      context_->message_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  callback->Run(EventResultToStatus(result),
                base::Time::FromDoubleT(event_dispatch_time));
  context_->message_event_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::didHandleInstallEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  Send(new ServiceWorkerHostMsg_InstallEventFinished(
      GetRoutingID(), request_id, result, proxy_->hasFetchEventHandler(),
      base::Time::FromDoubleT(event_dispatch_time)));
}

void ServiceWorkerContextClient::respondToFetchEvent(
    int fetch_event_id,
    double event_dispatch_time) {
  Send(new ServiceWorkerHostMsg_FetchEventResponse(
      GetRoutingID(), fetch_event_id,
      SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK, ServiceWorkerResponse(),
      base::Time::FromDoubleT(event_dispatch_time)));
}

void ServiceWorkerContextClient::respondToFetchEvent(
    int fetch_event_id,
    const blink::WebServiceWorkerResponse& web_response,
    double event_dispatch_time) {
  Send(new ServiceWorkerHostMsg_FetchEventResponse(
      GetRoutingID(), fetch_event_id,
      SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
      GetServiceWorkerResponseFromWebResponse(web_response),
      base::Time::FromDoubleT(event_dispatch_time)));
}

void ServiceWorkerContextClient::didHandleFetchEvent(
    int fetch_event_id,
    blink::WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  const FetchCallback* callback =
      context_->fetch_event_callbacks.Lookup(fetch_event_id);
  DCHECK(callback);
  callback->Run(EventResultToStatus(result),
                base::Time::FromDoubleT(event_dispatch_time));
  context_->fetch_event_callbacks.Remove(fetch_event_id);
}

void ServiceWorkerContextClient::didHandleNotificationClickEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  Send(new ServiceWorkerHostMsg_NotificationClickEventFinished(
      GetRoutingID(), request_id, result,
      base::Time::FromDoubleT(event_dispatch_time)));
}

void ServiceWorkerContextClient::didHandleNotificationCloseEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  Send(new ServiceWorkerHostMsg_NotificationCloseEventFinished(
      GetRoutingID(), request_id, result,
      base::Time::FromDoubleT(event_dispatch_time)));
}

void ServiceWorkerContextClient::didHandlePushEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  const DispatchPushEventCallback* callback =
      context_->push_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  callback->Run(result == blink::WebServiceWorkerEventResultCompleted
                    ? SERVICE_WORKER_OK
                    : SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED,
                base::Time::FromDoubleT(event_dispatch_time));
  context_->push_event_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::didHandleSyncEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result,
    double event_dispatch_time) {
  const SyncCallback* callback =
      context_->sync_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  callback->Run(EventResultToStatus(result),
                base::Time::FromDoubleT(event_dispatch_time));
  context_->sync_event_callbacks.Remove(request_id);
}

blink::WebServiceWorkerNetworkProvider*
ServiceWorkerContextClient::createServiceWorkerNetworkProvider(
    blink::WebDataSource* data_source) {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());

  // Create a content::ServiceWorkerNetworkProvider for this data source so
  // we can observe its requests.
  std::unique_ptr<ServiceWorkerNetworkProvider> provider =
      base::MakeUnique<ServiceWorkerNetworkProvider>(
          MSG_ROUTING_NONE, SERVICE_WORKER_PROVIDER_FOR_CONTROLLER,
          true /* is_parent_frame_secure */);
  provider_context_ = provider->context();

  // Tell the network provider about which version to load.
  provider->SetServiceWorkerVersionId(service_worker_version_id_,
                                      embedded_worker_id_);

  // The provider is kept around for the lifetime of the DataSource
  // and ownership is transferred to the DataSource.
  DataSourceExtraData* extra_data = new DataSourceExtraData();
  data_source->setExtraData(extra_data);
  ServiceWorkerNetworkProvider::AttachToDocumentState(extra_data,
                                                      std::move(provider));

  // Blink is responsible for deleting the returned object.
  return new WebServiceWorkerNetworkProviderImpl();
}

blink::WebServiceWorkerProvider*
ServiceWorkerContextClient::createServiceWorkerProvider() {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(provider_context_);

  // Blink is responsible for deleting the returned object.
  return new WebServiceWorkerProviderImpl(
      sender_.get(), provider_context_.get());
}

void ServiceWorkerContextClient::postMessageToClient(
    const blink::WebString& uuid,
    const blink::WebString& message,
    blink::WebMessagePortChannelArray* channels) {
  // This may send channels for MessagePorts, and all internal book-keeping
  // messages for MessagePort (e.g. QueueMessages) are sent from main thread
  // (with thread hopping), so we need to do the same thread hopping here not
  // to overtake those messages.
  std::unique_ptr<blink::WebMessagePortChannelArray> channel_array(channels);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SendPostMessageToClientOnMainThread,
                 base::RetainedRef(sender_), GetRoutingID(), uuid.utf8(),
                 message.utf16(), base::Passed(&channel_array)));
}

void ServiceWorkerContextClient::focus(
    const blink::WebString& uuid,
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callback) {
  DCHECK(callback);
  int request_id = context_->client_callbacks.Add(std::move(callback));
  Send(new ServiceWorkerHostMsg_FocusClient(GetRoutingID(), request_id,
                                            uuid.utf8()));
}

void ServiceWorkerContextClient::navigate(
    const blink::WebString& uuid,
    const blink::WebURL& url,
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callback) {
  DCHECK(callback);
  int request_id = context_->client_callbacks.Add(std::move(callback));
  Send(new ServiceWorkerHostMsg_NavigateClient(GetRoutingID(), request_id,
                                               uuid.utf8(), url));
}

void ServiceWorkerContextClient::skipWaiting(
    std::unique_ptr<blink::WebServiceWorkerSkipWaitingCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id = context_->skip_waiting_callbacks.Add(std::move(callbacks));
  Send(new ServiceWorkerHostMsg_SkipWaiting(GetRoutingID(), request_id));
}

void ServiceWorkerContextClient::claim(
    std::unique_ptr<blink::WebServiceWorkerClientsClaimCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id = context_->claim_clients_callbacks.Add(std::move(callbacks));
  Send(new ServiceWorkerHostMsg_ClaimClients(GetRoutingID(), request_id));
}

void ServiceWorkerContextClient::registerForeignFetchScopes(
    const blink::WebVector<blink::WebURL>& sub_scopes,
    const blink::WebVector<blink::WebSecurityOrigin>& origins) {
  Send(new ServiceWorkerHostMsg_RegisterForeignFetchScopes(
      GetRoutingID(), std::vector<GURL>(sub_scopes.begin(), sub_scopes.end()),
      std::vector<url::Origin>(origins.begin(), origins.end())));
}

void ServiceWorkerContextClient::DispatchSyncEvent(
    const std::string& tag,
    blink::mojom::BackgroundSyncEventLastChance last_chance,
    const DispatchSyncEventCallback& callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchSyncEvent");
  int request_id = context_->sync_event_callbacks.Add(
      base::MakeUnique<SyncCallback>(callback));

  // TODO(shimazu): Use typemap when this is moved to blink-side.
  blink::WebServiceWorkerContextProxy::LastChanceOption web_last_chance =
      mojo::ConvertTo<blink::WebServiceWorkerContextProxy::LastChanceOption>(
          last_chance);

  // TODO(jkarlin): Make this blink::WebString::FromUTF8Lenient once
  // https://crrev.com/1768063002/ lands.
  proxy_->dispatchSyncEvent(request_id, blink::WebString::fromUTF8(tag),
                            web_last_chance);
}

void ServiceWorkerContextClient::DispatchPaymentRequestEvent(
    payments::mojom::PaymentAppRequestDataPtr data,
    const DispatchPaymentRequestEventCallback& callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerContextClient::Send(IPC::Message* message) {
  sender_->Send(message);
}

void ServiceWorkerContextClient::SendWorkerStarted() {
  DCHECK(worker_task_runner_->RunsTasksOnCurrentThread());
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerContextClient::StartingWorkerContext",
                         this);
  Send(new EmbeddedWorkerHostMsg_WorkerStarted(embedded_worker_id_));
}

void ServiceWorkerContextClient::SetRegistrationInServiceWorkerGlobalScope(
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  DCHECK(worker_task_runner_->RunsTasksOnCurrentThread());
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
          sender_.get(), main_thread_task_runner_.get());

  // Register a registration and its version attributes with the dispatcher
  // living on the worker thread.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration(
      dispatcher->GetOrCreateRegistration(info, attrs));

  proxy_->setRegistration(
      WebServiceWorkerRegistrationImpl::CreateHandle(registration));
}

void ServiceWorkerContextClient::OnActivateEvent(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnActivateEvent");
  proxy_->dispatchActivateEvent(request_id);
}

void ServiceWorkerContextClient::DispatchExtendableMessageEvent(
    mojom::ExtendableMessageEventPtr event,
    const DispatchExtendableMessageEventCallback& callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchExtendableMessageEvent");
  int request_id = context_->message_event_callbacks.Add(
      base::MakeUnique<DispatchExtendableMessageEventCallback>(callback));

  blink::WebMessagePortChannelArray ports =
      WebMessagePortChannelImpl::CreatePorts(event->message_ports,
                                             event->new_routing_ids,
                                             main_thread_task_runner_);
  if (event->source.client_info.IsValid()) {
    blink::WebServiceWorkerClientInfo web_client =
        ToWebServiceWorkerClientInfo(event->source.client_info);
    proxy_->dispatchExtendableMessageEvent(
        request_id, blink::WebString::fromUTF16(event->message),
        event->source_origin, ports, web_client);
    return;
  }

  DCHECK(event->source.service_worker_info.IsValid());
  std::unique_ptr<ServiceWorkerHandleReference> handle =
      ServiceWorkerHandleReference::Adopt(event->source.service_worker_info,
                                          sender_.get());
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
          sender_.get(), main_thread_task_runner_.get());
  scoped_refptr<WebServiceWorkerImpl> worker =
      dispatcher->GetOrCreateServiceWorker(std::move(handle));
  proxy_->dispatchExtendableMessageEvent(
      request_id, blink::WebString::fromUTF16(event->message),
      event->source_origin, ports, WebServiceWorkerImpl::CreateHandle(worker));
}

void ServiceWorkerContextClient::OnInstallEvent(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnInstallEvent");
  proxy_->dispatchInstallEvent(request_id);
}

void ServiceWorkerContextClient::DispatchFetchEvent(
    int fetch_event_id,
    const ServiceWorkerFetchRequest& request,
    mojom::FetchEventPreloadHandlePtr preload_handle,
    const DispatchFetchEventCallback& callback) {
  std::unique_ptr<NavigationPreloadRequest> preload_request;
  if (preload_handle) {
    proxy_->onNavigationPreloadSent(fetch_event_id, request.url);
    preload_request = base::MakeUnique<NavigationPreloadRequest>(
        fetch_event_id, request.url, std::move(preload_handle));
  }
  const bool navigation_preload_sent = !!preload_request;
  blink::WebServiceWorkerRequest webRequest;
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchFetchEvent");
  context_->fetch_event_callbacks.AddWithID(
      base::MakeUnique<FetchCallback>(callback), fetch_event_id);
  if (preload_request) {
    context_->preload_requests.AddWithID(std::move(preload_request),
                                         fetch_event_id);
  }

  webRequest.setURL(blink::WebURL(request.url));
  webRequest.setMethod(blink::WebString::fromUTF8(request.method));
  for (ServiceWorkerHeaderMap::const_iterator it = request.headers.begin();
       it != request.headers.end();
       ++it) {
    webRequest.setHeader(blink::WebString::fromUTF8(it->first),
                         blink::WebString::fromUTF8(it->second));
  }
  if (!request.blob_uuid.empty()) {
    webRequest.setBlob(blink::WebString::fromASCII(request.blob_uuid),
                       request.blob_size);
  }
  webRequest.setReferrer(
      blink::WebString::fromUTF8(request.referrer.url.spec()),
      request.referrer.policy);
  webRequest.setMode(GetBlinkFetchRequestMode(request.mode));
  webRequest.setIsMainResourceLoad(request.is_main_resource_load);
  webRequest.setCredentialsMode(
      GetBlinkFetchCredentialsMode(request.credentials_mode));
  webRequest.setRedirectMode(GetBlinkFetchRedirectMode(request.redirect_mode));
  webRequest.setRequestContext(
      GetBlinkRequestContext(request.request_context_type));
  webRequest.setFrameType(GetBlinkFrameType(request.frame_type));
  webRequest.setClientId(blink::WebString::fromUTF8(request.client_id));
  webRequest.setIsReload(request.is_reload);
  if (request.fetch_type == ServiceWorkerFetchType::FOREIGN_FETCH) {
    proxy_->dispatchForeignFetchEvent(fetch_event_id, webRequest);
  } else {
    proxy_->dispatchFetchEvent(fetch_event_id, webRequest,
                               navigation_preload_sent);
  }
}

void ServiceWorkerContextClient::OnNotificationClickEvent(
    int request_id,
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    int action_index,
    const base::NullableString16& reply) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnNotificationClickEvent");
  proxy_->dispatchNotificationClickEvent(
      request_id, blink::WebString::fromUTF8(notification_id),
      ToWebNotificationData(notification_data), action_index,
      blink::WebString::fromUTF16(reply));
}

void ServiceWorkerContextClient::OnNotificationCloseEvent(
    int request_id,
    const std::string& notification_id,
    const PlatformNotificationData& notification_data) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnNotificationCloseEvent");
  proxy_->dispatchNotificationCloseEvent(
      request_id, blink::WebString::fromUTF8(notification_id),
      ToWebNotificationData(notification_data));
}

void ServiceWorkerContextClient::DispatchPushEvent(
    const PushEventPayload& payload,
    const DispatchPushEventCallback& callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchPushEvent");
  int request_id = context_->push_event_callbacks.Add(
      base::MakeUnique<DispatchPushEventCallback>(callback));

  // Only set data to be a valid string if the payload had decrypted data.
  blink::WebString data;
  if (!payload.is_null)
    data.assign(blink::WebString::fromUTF8(payload.data));
  proxy_->dispatchPushEvent(request_id, data);
}

void ServiceWorkerContextClient::OnDidGetClient(
    int request_id,
    const ServiceWorkerClientInfo& client) {
  TRACE_EVENT0("ServiceWorker", "ServiceWorkerContextClient::OnDidGetClient");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      context_->client_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  std::unique_ptr<blink::WebServiceWorkerClientInfo> web_client;
  if (!client.IsEmpty()) {
    DCHECK(client.IsValid());
    web_client.reset(new blink::WebServiceWorkerClientInfo(
        ToWebServiceWorkerClientInfo(client)));
  }
  callbacks->onSuccess(std::move(web_client));
  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnDidGetClients(
    int request_id, const std::vector<ServiceWorkerClientInfo>& clients) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnDidGetClients");
  blink::WebServiceWorkerClientsCallbacks* callbacks =
      context_->clients_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  blink::WebServiceWorkerClientsInfo info;
  blink::WebVector<blink::WebServiceWorkerClientInfo> convertedClients(
      clients.size());
  for (size_t i = 0; i < clients.size(); ++i)
    convertedClients[i] = ToWebServiceWorkerClientInfo(clients[i]);
  info.clients.swap(convertedClients);
  callbacks->onSuccess(info);
  context_->clients_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnOpenWindowResponse(
    int request_id,
    const ServiceWorkerClientInfo& client) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnOpenWindowResponse");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      context_->client_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  std::unique_ptr<blink::WebServiceWorkerClientInfo> web_client;
  if (!client.IsEmpty()) {
    DCHECK(client.IsValid());
    web_client.reset(new blink::WebServiceWorkerClientInfo(
        ToWebServiceWorkerClientInfo(client)));
  }
  callbacks->onSuccess(std::move(web_client));
  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnOpenWindowError(
    int request_id,
    const std::string& message) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnOpenWindowError");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      context_->client_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  callbacks->onError(blink::WebServiceWorkerError(
      blink::WebServiceWorkerError::ErrorTypeNavigation,
      blink::WebString::fromUTF8(message)));
  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnFocusClientResponse(
    int request_id, const ServiceWorkerClientInfo& client) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnFocusClientResponse");
  blink::WebServiceWorkerClientCallbacks* callback =
      context_->client_callbacks.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  if (!client.IsEmpty()) {
    DCHECK(client.IsValid());
    std::unique_ptr<blink::WebServiceWorkerClientInfo> web_client(
        new blink::WebServiceWorkerClientInfo(
            ToWebServiceWorkerClientInfo(client)));
    callback->onSuccess(std::move(web_client));
  } else {
    callback->onError(blink::WebServiceWorkerError(
        blink::WebServiceWorkerError::ErrorTypeNotFound,
        "The WindowClient was not found."));
  }

  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnNavigateClientResponse(
    int request_id,
    const ServiceWorkerClientInfo& client) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnNavigateClientResponse");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      context_->client_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  std::unique_ptr<blink::WebServiceWorkerClientInfo> web_client;
  if (!client.IsEmpty()) {
    DCHECK(client.IsValid());
    web_client.reset(new blink::WebServiceWorkerClientInfo(
        ToWebServiceWorkerClientInfo(client)));
  }
  callbacks->onSuccess(std::move(web_client));
  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnNavigateClientError(int request_id,
                                                       const GURL& url) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnNavigateClientError");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      context_->client_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  std::string message = "Cannot navigate to URL: " + url.spec();
  callbacks->onError(blink::WebServiceWorkerError(
      blink::WebServiceWorkerError::ErrorTypeNavigation,
      blink::WebString::fromUTF8(message)));
  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnDidSkipWaiting(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnDidSkipWaiting");
  blink::WebServiceWorkerSkipWaitingCallbacks* callbacks =
      context_->skip_waiting_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  callbacks->onSuccess();
  context_->skip_waiting_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnDidClaimClients(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnDidClaimClients");
  blink::WebServiceWorkerClientsClaimCallbacks* callbacks =
      context_->claim_clients_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  callbacks->onSuccess();
  context_->claim_clients_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnClaimClientsError(
    int request_id,
    blink::WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnClaimClientsError");
  blink::WebServiceWorkerClientsClaimCallbacks* callbacks =
      context_->claim_clients_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  callbacks->onError(blink::WebServiceWorkerError(
      error_type, blink::WebString::fromUTF16(message)));
  context_->claim_clients_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnPing() {
  Send(new ServiceWorkerHostMsg_Pong(GetRoutingID()));
}

void ServiceWorkerContextClient::OnNavigationPreloadResponse(
    int fetch_event_id,
    std::unique_ptr<blink::WebURLResponse> response,
    std::unique_ptr<blink::WebDataConsumerHandle> data_consumer_handle) {
  proxy_->onNavigationPreloadResponse(fetch_event_id, std::move(response),
                                      std::move(data_consumer_handle));
}

void ServiceWorkerContextClient::OnNavigationPreloadError(
    int fetch_event_id,
    std::unique_ptr<blink::WebServiceWorkerError> error) {
  proxy_->onNavigationPreloadError(fetch_event_id, std::move(error));
  context_->preload_requests.Remove(fetch_event_id);
}

void ServiceWorkerContextClient::OnNavigationPreloadComplete(
    int fetch_event_id,
    int64_t encoded_data_length) {
  proxy_->onNavigationPreloadCompleted(fetch_event_id, encoded_data_length);
  context_->preload_requests.Remove(fetch_event_id);
}

base::WeakPtr<ServiceWorkerContextClient>
ServiceWorkerContextClient::GetWeakPtr() {
  DCHECK(worker_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(context_);
  return context_->weak_factory.GetWeakPtr();
}

}  // namespace content
