// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_context_client.h"

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/pickle.h"
#include "base/threading/thread_local.h"
#include "content/child/request_extra_data.h"
#include "content/child/service_worker/service_worker_network_provider.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_task_runner.h"
#include "content/child/worker_thread_task_runner.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/renderer/document_state.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/embedded_worker_dispatcher.h"
#include "content/renderer/service_worker/service_worker_script_context.h"
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
    const GURL& script_url)
    : embedded_worker_id_(embedded_worker_id),
      service_worker_version_id_(service_worker_version_id),
      service_worker_scope_(service_worker_scope),
      script_url_(script_url),
      sender_(ChildThread::current()->thread_safe_sender()),
      main_thread_proxy_(base::MessageLoopProxy::current()),
      weak_factory_(this) {
}

EmbeddedWorkerContextClient::~EmbeddedWorkerContextClient() {
  // g_worker_client_tls.Pointer()->Get() could be NULL if this gets
  // deleted before workerContextStarted() is called.
  g_worker_client_tls.Pointer()->Set(NULL);
}

bool EmbeddedWorkerContextClient::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerContextClient, msg)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerContextMsg_SendMessageToWorker,
                        OnSendMessageToWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void EmbeddedWorkerContextClient::SendMessageToBrowser(
    int request_id,
    const IPC::Message& message) {
  sender_->Send(new EmbeddedWorkerHostMsg_SendMessageToBrowser(
      embedded_worker_id_, request_id, message));
}

void EmbeddedWorkerContextClient::workerContextFailedToStart() {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  DCHECK(!script_context_);

  RenderThreadImpl::current()->embedded_worker_dispatcher()->
      WorkerContextDestroyed(embedded_worker_id_);
}

void EmbeddedWorkerContextClient::workerContextStarted(
    blink::WebServiceWorkerContextProxy* proxy) {
  DCHECK(!worker_task_runner_);
  worker_task_runner_ = new WorkerThreadTaskRunner(
      WorkerTaskRunner::Instance()->CurrentWorkerId());
  DCHECK_NE(0, WorkerTaskRunner::Instance()->CurrentWorkerId());
  DCHECK(g_worker_client_tls.Pointer()->Get() == NULL);
  DCHECK(!script_context_);
  g_worker_client_tls.Pointer()->Set(this);
  script_context_.reset(new ServiceWorkerScriptContext(this, proxy));

  // Schedule a task to send back WorkerStarted asynchronously,
  // so that at the time we send it we can be sure that the worker
  // script has been evaluated and worker run loop has been started.
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerContextClient::SendWorkerStarted,
                 weak_factory_.GetWeakPtr()));
}

void EmbeddedWorkerContextClient::workerContextDestroyed() {
  // At this point OnWorkerRunLoopStopped is already called, so
  // worker_task_runner_->RunsTasksOnCurrentThread() returns false
  // (while we're still on the worker thread).
  script_context_.reset();
  main_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CallWorkerContextDestroyedOnMainThread,
                 embedded_worker_id_));
}

void EmbeddedWorkerContextClient::reportException(
    const blink::WebString& error_message,
    int line_number,
    int column_number,
    const blink::WebString& source_url) {
  sender_->Send(new EmbeddedWorkerHostMsg_ReportException(
      embedded_worker_id_, error_message, line_number,
      column_number, GURL(source_url)));
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
  ServiceWorkerResponse response(web_response.statusCode(),
                                 web_response.statusText().utf8(),
                                 web_response.method().utf8(),
                                 std::map<std::string, std::string>());
  script_context_->DidHandleFetchEvent(
      request_id, SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE, response);
}

blink::WebServiceWorkerNetworkProvider*
EmbeddedWorkerContextClient::createServiceWorkerNetworkProvider(
    blink::WebDataSource* data_source) {
  // Create a content::ServiceWorkerNetworkProvider for this data source so
  // we can observe its requests.
  scoped_ptr<ServiceWorkerNetworkProvider> provider(
      new ServiceWorkerNetworkProvider());

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

void EmbeddedWorkerContextClient::didHandleSyncEvent(int request_id) {
  DCHECK(script_context_);
  script_context_->DidHandleSyncEvent(request_id);
}

blink::WebURL EmbeddedWorkerContextClient::scope() const {
  return service_worker_scope_;
}

void EmbeddedWorkerContextClient::OnSendMessageToWorker(
    int thread_id,
    int embedded_worker_id,
    int request_id,
    const IPC::Message& message) {
  if (!script_context_)
    return;
  DCHECK_EQ(embedded_worker_id_, embedded_worker_id);
  script_context_->OnMessageReceived(request_id, message);
}

void EmbeddedWorkerContextClient::SendWorkerStarted() {
  DCHECK(worker_task_runner_->RunsTasksOnCurrentThread());
  sender_->Send(new EmbeddedWorkerHostMsg_WorkerStarted(
      WorkerTaskRunner::Instance()->CurrentWorkerId(),
      embedded_worker_id_));
}

}  // namespace content
