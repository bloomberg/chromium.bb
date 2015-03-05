// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_context_client.h"

#include <map>
#include <string>

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/pickle.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event.h"
#include "content/child/request_extra_data.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_network_provider.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_provider_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_task_runner.h"
#include "content/common/devtools_messages.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/renderer/document_state.h"
#include "content/renderer/devtools/devtools_agent.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/embedded_worker_dispatcher.h"
#include "content/renderer/service_worker/service_worker_script_context.h"
#include "content/renderer/service_worker/service_worker_type_util.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerResponse.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebServiceWorkerNetworkProvider.h"

namespace content {

namespace {

// For now client must be a per-thread instance.
// TODO(kinuko): This needs to be refactored when we start using thread pool
// or having multiple clients per one thread.
base::LazyInstance<base::ThreadLocalPointer<EmbeddedWorkerContextClient> >::
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
  virtual ~DataSourceExtraData() {}
};

// Called on the main thread only and blink owns it.
class WebServiceWorkerNetworkProviderImpl
    : public blink::WebServiceWorkerNetworkProvider {
 public:
  // Blink calls this method for each request starting with the main script,
  // we tag them with the provider id.
  virtual void willSendRequest(
      blink::WebDataSource* data_source,
      blink::WebURLRequest& request) {
    ServiceWorkerNetworkProvider* provider =
        ServiceWorkerNetworkProvider::FromDocumentState(
            static_cast<DataSourceExtraData*>(data_source->extraData()));
    scoped_ptr<RequestExtraData> extra_data(new RequestExtraData);
    extra_data->set_service_worker_provider_id(provider->provider_id());
    request.setExtraData(extra_data.release());
  }
};

}  // namespace

EmbeddedWorkerContextClient*
EmbeddedWorkerContextClient::ThreadSpecificInstance() {
  return g_worker_client_tls.Pointer()->Get();
}

EmbeddedWorkerContextClient::EmbeddedWorkerContextClient(
    int embedded_worker_id,
    int64 service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url,
    int worker_devtools_agent_route_id)
    : embedded_worker_id_(embedded_worker_id),
      service_worker_version_id_(service_worker_version_id),
      service_worker_scope_(service_worker_scope),
      script_url_(script_url),
      worker_devtools_agent_route_id_(worker_devtools_agent_route_id),
      sender_(ChildThreadImpl::current()->thread_safe_sender()),
      main_thread_task_runner_(RenderThreadImpl::current()->GetTaskRunner()),
      weak_factory_(this) {
  TRACE_EVENT_ASYNC_BEGIN0("ServiceWorker",
                           "EmbeddedWorkerContextClient::StartingWorkerContext",
                           this);
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "EmbeddedWorkerContextClient::StartingWorkerContext",
      this,
      "PrepareWorker");
}

EmbeddedWorkerContextClient::~EmbeddedWorkerContextClient() {
}

bool EmbeddedWorkerContextClient::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerContextClient, msg)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerContextMsg_MessageToWorker,
                        OnMessageToWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void EmbeddedWorkerContextClient::Send(IPC::Message* message) {
  sender_->Send(message);
}

blink::WebURL EmbeddedWorkerContextClient::scope() const {
  return service_worker_scope_;
}

blink::WebServiceWorkerCacheStorage*
    EmbeddedWorkerContextClient::cacheStorage() {
  return script_context_->cache_storage();
}

void EmbeddedWorkerContextClient::didPauseAfterDownload() {
  Send(new EmbeddedWorkerHostMsg_DidPauseAfterDownload(embedded_worker_id_));
}

void EmbeddedWorkerContextClient::getClients(
    blink::WebServiceWorkerClientsCallbacks* callbacks) {
  DCHECK(script_context_);
  script_context_->GetClientDocuments(callbacks);
}

void EmbeddedWorkerContextClient::openWindow(
    const blink::WebURL& url,
    blink::WebServiceWorkerClientCallbacks* callbacks) {
  DCHECK(script_context_);
  script_context_->OpenWindow(url, callbacks);
}

void EmbeddedWorkerContextClient::setCachedMetadata(const blink::WebURL& url,
                                                    const char* data,
                                                    size_t size) {
  DCHECK(script_context_);
  script_context_->SetCachedMetadata(url, data, size);
}

void EmbeddedWorkerContextClient::clearCachedMetadata(
    const blink::WebURL& url) {
  DCHECK(script_context_);
  script_context_->ClearCachedMetadata(url);
}

void EmbeddedWorkerContextClient::workerReadyForInspection() {
  Send(new EmbeddedWorkerHostMsg_WorkerReadyForInspection(embedded_worker_id_));
}

void EmbeddedWorkerContextClient::workerContextFailedToStart() {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!script_context_);

  Send(new EmbeddedWorkerHostMsg_WorkerScriptLoadFailed(embedded_worker_id_));

  RenderThreadImpl::current()->embedded_worker_dispatcher()->
      WorkerContextDestroyed(embedded_worker_id_);
}

void EmbeddedWorkerContextClient::workerContextStarted(
    blink::WebServiceWorkerContextProxy* proxy) {
  DCHECK(!worker_task_runner_.get());
  DCHECK_NE(0, WorkerTaskRunner::Instance()->CurrentWorkerId());
  worker_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  // g_worker_client_tls.Pointer()->Get() could return NULL if this context
  // gets deleted before workerContextStarted() is called.
  DCHECK(g_worker_client_tls.Pointer()->Get() == NULL);
  DCHECK(!script_context_);
  g_worker_client_tls.Pointer()->Set(this);
  script_context_.reset(new ServiceWorkerScriptContext(this, proxy));

  SetRegistrationInServiceWorkerGlobalScope();

  Send(new EmbeddedWorkerHostMsg_WorkerScriptLoaded(
      embedded_worker_id_,
      WorkerTaskRunner::Instance()->CurrentWorkerId(),
      provider_context_->provider_id()));

  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "EmbeddedWorkerContextClient::StartingWorkerContext",
      this,
      "ExecuteScript");
}

void EmbeddedWorkerContextClient::didEvaluateWorkerScript(bool success) {
  Send(new EmbeddedWorkerHostMsg_WorkerScriptEvaluated(
      embedded_worker_id_, success));

  // Schedule a task to send back WorkerStarted asynchronously,
  // so that at the time we send it we can be sure that the
  // worker run loop has been started.
  worker_task_runner_->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerContextClient::SendWorkerStarted,
                            weak_factory_.GetWeakPtr()));
}

void EmbeddedWorkerContextClient::willDestroyWorkerContext() {
  // At this point OnWorkerRunLoopStopped is already called, so
  // worker_task_runner_->RunsTasksOnCurrentThread() returns false
  // (while we're still on the worker thread).
  script_context_.reset();

  // This also lets the message filter stop dispatching messages to
  // this client.
  g_worker_client_tls.Pointer()->Set(NULL);
}

void EmbeddedWorkerContextClient::workerContextDestroyed() {
  DCHECK(g_worker_client_tls.Pointer()->Get() == NULL);

  // Now we should be able to free the WebEmbeddedWorker container on the
  // main thread.
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CallWorkerContextDestroyedOnMainThread,
                 embedded_worker_id_));
}

void EmbeddedWorkerContextClient::reportException(
    const blink::WebString& error_message,
    int line_number,
    int column_number,
    const blink::WebString& source_url) {
  Send(new EmbeddedWorkerHostMsg_ReportException(
      embedded_worker_id_, error_message, line_number,
      column_number, GURL(source_url)));
}

void EmbeddedWorkerContextClient::reportConsoleMessage(
    int source,
    int level,
    const blink::WebString& message,
    int line_number,
    const blink::WebString& source_url) {
  EmbeddedWorkerHostMsg_ReportConsoleMessage_Params params;
  params.source_identifier = source;
  params.message_level = level;
  params.message = message;
  params.line_number = line_number;
  params.source_url = GURL(source_url);

  Send(new EmbeddedWorkerHostMsg_ReportConsoleMessage(
      embedded_worker_id_, params));
}

void EmbeddedWorkerContextClient::sendDevToolsMessage(
    int call_id,
    const blink::WebString& message,
    const blink::WebString& state_cookie) {
  DevToolsAgent::SendChunkedProtocolMessage(
      sender_.get(), worker_devtools_agent_route_id_,
      call_id, message.utf8(), state_cookie.utf8());
}

void EmbeddedWorkerContextClient::didHandleActivateEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  DCHECK(script_context_);
  script_context_->DidHandleActivateEvent(request_id, result);
}

void EmbeddedWorkerContextClient::didHandleInstallEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  DCHECK(script_context_);
  script_context_->DidHandleInstallEvent(request_id, result);
}

void EmbeddedWorkerContextClient::didHandleFetchEvent(int request_id) {
  DCHECK(script_context_);
  script_context_->DidHandleFetchEvent(
      request_id,
      SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
      ServiceWorkerResponse());
}

void EmbeddedWorkerContextClient::didHandleFetchEvent(
    int request_id,
    const blink::WebServiceWorkerResponse& web_response) {
  DCHECK(script_context_);
  ServiceWorkerHeaderMap headers;
  GetServiceWorkerHeaderMapFromWebResponse(web_response, &headers);
  ServiceWorkerResponse response(web_response.url(),
                                 web_response.status(),
                                 web_response.statusText().utf8(),
                                 web_response.responseType(),
                                 headers,
                                 web_response.blobUUID().utf8(),
                                 web_response.blobSize(),
                                 web_response.streamURL());
  script_context_->DidHandleFetchEvent(
      request_id, SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE, response);
}

void EmbeddedWorkerContextClient::didHandleNotificationClickEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  DCHECK(script_context_);
  script_context_->DidHandleNotificationClickEvent(request_id, result);
}

void EmbeddedWorkerContextClient::didHandlePushEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  DCHECK(script_context_);
  script_context_->DidHandlePushEvent(request_id, result);
}

void EmbeddedWorkerContextClient::didHandleSyncEvent(int request_id) {
  DCHECK(script_context_);
  script_context_->DidHandleSyncEvent(request_id);
}

void EmbeddedWorkerContextClient::didHandleCrossOriginConnectEvent(
    int request_id,
    bool accept_connection) {
  DCHECK(script_context_);
  script_context_->DidHandleCrossOriginConnectEvent(request_id,
                                                    accept_connection);
}

blink::WebServiceWorkerNetworkProvider*
EmbeddedWorkerContextClient::createServiceWorkerNetworkProvider(
    blink::WebDataSource* data_source) {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());

  // Create a content::ServiceWorkerNetworkProvider for this data source so
  // we can observe its requests.
  scoped_ptr<ServiceWorkerNetworkProvider> provider(
      new ServiceWorkerNetworkProvider(
          MSG_ROUTING_NONE, SERVICE_WORKER_PROVIDER_FOR_CONTROLLER));
  provider_context_ = provider->context();

  // Tell the network provider about which version to load.
  provider->SetServiceWorkerVersionId(service_worker_version_id_);

  // The provider is kept around for the lifetime of the DataSource
  // and ownership is transferred to the DataSource.
  DataSourceExtraData* extra_data = new DataSourceExtraData();
  data_source->setExtraData(extra_data);
  ServiceWorkerNetworkProvider::AttachToDocumentState(
      extra_data, provider.Pass());

  // Blink is responsible for deleting the returned object.
  return new WebServiceWorkerNetworkProviderImpl();
}

blink::WebServiceWorkerProvider*
EmbeddedWorkerContextClient::createServiceWorkerProvider() {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(provider_context_);

  // Blink is responsible for deleting the returned object.
  return new WebServiceWorkerProviderImpl(
      thread_safe_sender(), provider_context_.get());
}

void EmbeddedWorkerContextClient::postMessageToClient(
    int client_id,
    const blink::WebString& message,
    blink::WebMessagePortChannelArray* channels) {
  DCHECK(script_context_);
  script_context_->PostMessageToDocument(client_id, message,
                                         make_scoped_ptr(channels));
}

void EmbeddedWorkerContextClient::postMessageToCrossOriginClient(
    const blink::WebCrossOriginServiceWorkerClient& client,
    const blink::WebString& message,
    blink::WebMessagePortChannelArray* channels) {
  DCHECK(script_context_);
  script_context_->PostCrossOriginMessageToClient(client, message,
                                                  make_scoped_ptr(channels));
}

void EmbeddedWorkerContextClient::focus(
    int client_id, blink::WebServiceWorkerClientCallbacks* callback) {
  DCHECK(script_context_);
  script_context_->FocusClient(client_id, callback);
}

void EmbeddedWorkerContextClient::skipWaiting(
    blink::WebServiceWorkerSkipWaitingCallbacks* callbacks) {
  DCHECK(script_context_);
  script_context_->SkipWaiting(callbacks);
}

void EmbeddedWorkerContextClient::claim(
    blink::WebServiceWorkerClientsClaimCallbacks* callbacks) {
  DCHECK(script_context_);
  script_context_->ClaimClients(callbacks);
}

void EmbeddedWorkerContextClient::OnMessageToWorker(
    int thread_id,
    int embedded_worker_id,
    const IPC::Message& message) {
  if (!script_context_)
    return;
  DCHECK_EQ(embedded_worker_id_, embedded_worker_id);
  script_context_->OnMessageReceived(message);
}

void EmbeddedWorkerContextClient::SendWorkerStarted() {
  DCHECK(worker_task_runner_->RunsTasksOnCurrentThread());
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "EmbeddedWorkerContextClient::StartingWorkerContext",
                         this);
  Send(new EmbeddedWorkerHostMsg_WorkerStarted(embedded_worker_id_));
}

void EmbeddedWorkerContextClient::SetRegistrationInServiceWorkerGlobalScope() {
  DCHECK(worker_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(provider_context_);
  DCHECK(script_context_);

  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  bool found =
      provider_context_->GetRegistrationInfoAndVersionAttributes(&info, &attrs);
  if (!found)
    return;  // Cannot be associated with a registration in some tests.

  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
          thread_safe_sender());

  // Register a registration and its version attributes with the dispatcher
  // living on the worker thread.
  scoped_ptr<WebServiceWorkerRegistrationImpl> registration(
      dispatcher->CreateServiceWorkerRegistration(info, false));
  registration->SetInstalling(
      dispatcher->GetServiceWorker(attrs.installing, false));
  registration->SetWaiting(
      dispatcher->GetServiceWorker(attrs.waiting, false));
  registration->SetActive(
      dispatcher->GetServiceWorker(attrs.active, false));

  script_context_->SetRegistrationInServiceWorkerGlobalScope(
      registration.Pass());
}

}  // namespace content
