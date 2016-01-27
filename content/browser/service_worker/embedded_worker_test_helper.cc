// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_test_helper.h"

#include <map>
#include <string>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/embedded_worker_setup.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/common/push_event_payload.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockMessagePortMessageFilter : public MessagePortMessageFilter {
 public:
  MockMessagePortMessageFilter()
      : MessagePortMessageFilter(
            base::Bind(&base::AtomicSequenceNumber::GetNext,
                       base::Unretained(&next_routing_id_))) {}

  bool Send(IPC::Message* message) override {
    message_queue_.push_back(message);
    return true;
  }

 private:
  ~MockMessagePortMessageFilter() override {}
  base::AtomicSequenceNumber next_routing_id_;
  ScopedVector<IPC::Message> message_queue_;
};

}  // namespace

class EmbeddedWorkerTestHelper::MockEmbeddedWorkerSetup
    : public EmbeddedWorkerSetup {
 public:
  static void Create(const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
                     mojo::InterfaceRequest<EmbeddedWorkerSetup> request) {
    new MockEmbeddedWorkerSetup(helper, std::move(request));
  }

  void ExchangeServiceProviders(
      int32_t thread_id,
      mojo::InterfaceRequest<mojo::ServiceProvider> services,
      mojo::ServiceProviderPtr exposed_services) override {
    if (!helper_)
      return;
    helper_->OnSetupMojoStub(thread_id, std::move(services),
                             std::move(exposed_services));
  }

 private:
  MockEmbeddedWorkerSetup(const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
                          mojo::InterfaceRequest<EmbeddedWorkerSetup> request)
      : helper_(helper), binding_(this, std::move(request)) {}

  base::WeakPtr<EmbeddedWorkerTestHelper> helper_;
  mojo::StrongBinding<EmbeddedWorkerSetup> binding_;
};

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory)
    : browser_context_(new TestBrowserContext),
      render_process_host_(new MockRenderProcessHost(browser_context_.get())),
      wrapper_(new ServiceWorkerContextWrapper(browser_context_.get())),
      next_thread_id_(0),
      mock_render_process_id_(render_process_host_->GetID()),
      weak_factory_(this) {
  scoped_ptr<MockServiceWorkerDatabaseTaskManager> database_task_manager(
      new MockServiceWorkerDatabaseTaskManager(
          base::ThreadTaskRunnerHandle::Get()));
  wrapper_->InitInternal(user_data_directory, std::move(database_task_manager),
                         base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  wrapper_->process_manager()->SetProcessIdForTest(mock_render_process_id());
  wrapper_->process_manager()->SetNewProcessIdForTest(new_render_process_id());
  registry()->AddChildProcessSender(mock_render_process_id_, this,
                                    NewMessagePortMessageFilter());

  // Setup process level mojo service registry pair.
  scoped_ptr<ServiceRegistryImpl> host_service_registry(
      new ServiceRegistryImpl);
  render_process_service_registry_.ServiceRegistry::AddService(
      base::Bind(&MockEmbeddedWorkerSetup::Create, weak_factory_.GetWeakPtr()));
  mojo::ServiceProviderPtr services;
  render_process_service_registry_.Bind(mojo::GetProxy(&services));
  host_service_registry->BindRemoteServiceProvider(std::move(services));
  render_process_host_->SetServiceRegistry(std::move(host_service_registry));
}

EmbeddedWorkerTestHelper::~EmbeddedWorkerTestHelper() {
  if (wrapper_.get())
    wrapper_->Shutdown();
}

void EmbeddedWorkerTestHelper::SimulateAddProcessToPattern(
    const GURL& pattern,
    int process_id) {
  registry()->AddChildProcessSender(process_id, this,
                                    NewMessagePortMessageFilter());
  wrapper_->process_manager()->AddProcessReferenceToPattern(
      pattern, process_id);
}

bool EmbeddedWorkerTestHelper::Send(IPC::Message* message) {
  OnMessageReceived(*message);
  delete message;
  return true;
}

bool EmbeddedWorkerTestHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerTestHelper, message)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerMsg_StartWorker, OnStartWorkerStub)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerMsg_StopWorker, OnStopWorkerStub)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerContextMsg_MessageToWorker,
                        OnMessageToWorkerStub)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // IPC::TestSink only records messages that are not handled by filters,
  // so we just forward all messages to the separate sink.
  sink_.OnMessageReceived(message);

  return handled;
}

ServiceWorkerContextCore* EmbeddedWorkerTestHelper::context() {
  return wrapper_->context();
}

void EmbeddedWorkerTestHelper::ShutdownContext() {
  wrapper_->Shutdown();
  wrapper_ = NULL;
}

void EmbeddedWorkerTestHelper::OnStartWorker(int embedded_worker_id,
                                             int64_t service_worker_version_id,
                                             const GURL& scope,
                                             const GURL& script_url) {
  embedded_worker_id_service_worker_version_id_map_[embedded_worker_id] =
      service_worker_version_id;
  SimulateWorkerReadyForInspection(embedded_worker_id);
  SimulateWorkerScriptCached(embedded_worker_id);
  SimulateWorkerScriptLoaded(embedded_worker_id);
  SimulateWorkerThreadStarted(GetNextThreadId(), embedded_worker_id);
  SimulateWorkerScriptEvaluated(embedded_worker_id, true /* success */);
  SimulateWorkerStarted(embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnStopWorker(int embedded_worker_id) {
  // By default just notify the sender that the worker is stopped.
  SimulateWorkerStopped(embedded_worker_id);
}

bool EmbeddedWorkerTestHelper::OnMessageToWorker(
    int thread_id,
    int embedded_worker_id,
    const IPC::Message& message) {
  bool handled = true;
  current_embedded_worker_id_ = embedded_worker_id;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerTestHelper, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ActivateEvent, OnActivateEventStub)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_InstallEvent, OnInstallEventStub)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FetchEvent, OnFetchEventStub)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_PushEvent, OnPushEventStub)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // Record all messages directed to inner script context.
  inner_sink_.OnMessageReceived(message);
  return handled;
}

void EmbeddedWorkerTestHelper::OnSetupMojo(ServiceRegistry* service_registry) {}

void EmbeddedWorkerTestHelper::OnActivateEvent(int embedded_worker_id,
                                               int request_id) {
  SimulateSend(
      new ServiceWorkerHostMsg_ActivateEventFinished(
          embedded_worker_id, request_id,
          blink::WebServiceWorkerEventResultCompleted));
}

void EmbeddedWorkerTestHelper::OnInstallEvent(int embedded_worker_id,
                                              int request_id) {
  // The installing worker may have been doomed and terminated.
  if (!registry()->GetWorker(embedded_worker_id))
    return;
  SimulateSend(
      new ServiceWorkerHostMsg_InstallEventFinished(
          embedded_worker_id, request_id,
          blink::WebServiceWorkerEventResultCompleted));
}

void EmbeddedWorkerTestHelper::OnFetchEvent(
    int embedded_worker_id,
    int request_id,
    const ServiceWorkerFetchRequest& request) {
  SimulateSend(new ServiceWorkerHostMsg_FetchEventFinished(
      embedded_worker_id, request_id,
      SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
      ServiceWorkerResponse(GURL(), 200, "OK",
                            blink::WebServiceWorkerResponseTypeDefault,
                            ServiceWorkerHeaderMap(), std::string(), 0, GURL(),
                            blink::WebServiceWorkerResponseErrorUnknown)));
}

void EmbeddedWorkerTestHelper::OnPushEvent(int embedded_worker_id,
                                           int request_id,
                                           const PushEventPayload& payload) {
  SimulateSend(new ServiceWorkerHostMsg_PushEventFinished(
      embedded_worker_id, request_id,
      blink::WebServiceWorkerEventResultCompleted));
}

void EmbeddedWorkerTestHelper::SimulateWorkerReadyForInspection(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerReadyForInspection(worker->process_id(),
                                         embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptCached(
    int embedded_worker_id) {
  int64_t version_id =
      embedded_worker_id_service_worker_version_id_map_[embedded_worker_id];
  ServiceWorkerVersion* version = context()->GetLiveVersion(version_id);
  if (!version || version->script_cache_map()->size())
    return;
  std::vector<ServiceWorkerDatabase::ResourceRecord> records;
  // Add a dummy ResourceRecord for the main script to the script cache map of
  // the ServiceWorkerVersion. We use embedded_worker_id for resource_id to
  // avoid ID collision.
  records.push_back(ServiceWorkerDatabase::ResourceRecord(
      embedded_worker_id, version->script_url(), 100));
  version->script_cache_map()->SetResources(records);
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptLoaded(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerScriptLoaded(worker->process_id(), embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerThreadStarted(
    int thread_id,
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerThreadStarted(worker->process_id(), thread_id,
                                    embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptEvaluated(
    int embedded_worker_id,
    bool success) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerScriptEvaluated(worker->process_id(), embedded_worker_id,
                                      success);
}

void EmbeddedWorkerTestHelper::SimulateWorkerStarted(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerStarted(
      worker->process_id(),
      embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerStopped(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  if (worker != NULL)
    registry()->OnWorkerStopped(worker->process_id(), embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateSend(
    IPC::Message* message) {
  registry()->OnMessageReceived(*message, mock_render_process_id_);
  delete message;
}

void EmbeddedWorkerTestHelper::OnStartWorkerStub(
    const EmbeddedWorkerMsg_StartWorker_Params& params) {
  EmbeddedWorkerInstance* worker =
      registry()->GetWorker(params.embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, worker->status());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnStartWorker,
                 weak_factory_.GetWeakPtr(), params.embedded_worker_id,
                 params.service_worker_version_id, params.scope,
                 params.script_url));
}

void EmbeddedWorkerTestHelper::OnStopWorkerStub(int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnStopWorker,
                 weak_factory_.GetWeakPtr(),
                 embedded_worker_id));
}

void EmbeddedWorkerTestHelper::OnMessageToWorkerStub(
    int thread_id,
    int embedded_worker_id,
    const IPC::Message& message) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  EXPECT_EQ(worker->thread_id(), thread_id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&EmbeddedWorkerTestHelper::OnMessageToWorker),
          weak_factory_.GetWeakPtr(),
          thread_id,
          embedded_worker_id,
          message));
}

void EmbeddedWorkerTestHelper::OnActivateEventStub(int request_id) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnActivateEvent,
                 weak_factory_.GetWeakPtr(),
                 current_embedded_worker_id_,
                 request_id));
}

void EmbeddedWorkerTestHelper::OnInstallEventStub(int request_id) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnInstallEvent,
                 weak_factory_.GetWeakPtr(),
                 current_embedded_worker_id_,
                 request_id));
}

void EmbeddedWorkerTestHelper::OnFetchEventStub(
    int request_id,
    const ServiceWorkerFetchRequest& request) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnFetchEvent,
                 weak_factory_.GetWeakPtr(),
                 current_embedded_worker_id_,
                 request_id,
                 request));
}

void EmbeddedWorkerTestHelper::OnPushEventStub(
    int request_id,
    const PushEventPayload& payload) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnPushEvent,
                            weak_factory_.GetWeakPtr(),
                            current_embedded_worker_id_, request_id, payload));
}

void EmbeddedWorkerTestHelper::OnSetupMojoStub(
    int thread_id,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services) {
  scoped_ptr<ServiceRegistryImpl> new_registry(new ServiceRegistryImpl);
  new_registry->Bind(std::move(services));
  new_registry->BindRemoteServiceProvider(std::move(exposed_services));
  OnSetupMojo(new_registry.get());
  thread_id_service_registry_map_.add(thread_id, std::move(new_registry));
}

EmbeddedWorkerRegistry* EmbeddedWorkerTestHelper::registry() {
  DCHECK(context());
  return context()->embedded_worker_registry();
}

MessagePortMessageFilter*
EmbeddedWorkerTestHelper::NewMessagePortMessageFilter() {
  scoped_refptr<MessagePortMessageFilter> filter(
      new MockMessagePortMessageFilter);
  message_port_message_filters_.push_back(filter);
  return filter.get();
}

}  // namespace content
