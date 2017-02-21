// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_test_helper.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/embedded_worker_start_params.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/push_event_payload.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockServiceWorkerDispatcherHost : public ServiceWorkerDispatcherHost {
 public:
  MockServiceWorkerDispatcherHost(int process_id,
                                  ResourceContext* resource_context,
                                  IPC::Sender* sender)
      : ServiceWorkerDispatcherHost(process_id, resource_context),
        sender_(sender) {}

  bool Send(IPC::Message* message) override { return sender_->Send(message); }

 protected:
  ~MockServiceWorkerDispatcherHost() override {}

 private:
  IPC::Sender* sender_;

  DISALLOW_COPY_AND_ASSIGN(MockServiceWorkerDispatcherHost);
};

}  // namespace

EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::
    MockEmbeddedWorkerInstanceClient(
        base::WeakPtr<EmbeddedWorkerTestHelper> helper)
    : helper_(helper), binding_(this) {}

EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::
    ~MockEmbeddedWorkerInstanceClient() {}

void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::StartWorker(
    const EmbeddedWorkerStartParams& params,
    mojom::ServiceWorkerEventDispatcherRequest dispatcher_request) {
  if (!helper_)
    return;

  embedded_worker_id_ = params.embedded_worker_id;

  EmbeddedWorkerInstance* worker =
      helper_->registry()->GetWorker(params.embedded_worker_id);
  ASSERT_TRUE(worker);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());

  helper_->OnStartWorkerStub(params, std::move(dispatcher_request));
}

void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::StopWorker(
    const StopWorkerCallback& callback) {
  if (!helper_)
    return;

  ASSERT_TRUE(embedded_worker_id_);
  EmbeddedWorkerInstance* worker =
      helper_->registry()->GetWorker(embedded_worker_id_.value());
  // |worker| is possible to be null when corresponding EmbeddedWorkerInstance
  // is removed right after sending StopWorker.
  if (worker)
    EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, worker->status());
  helper_->OnStopWorkerStub(callback);
}

void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::
    ResumeAfterDownload() {
  helper_->OnResumeAfterDownloadStub(embedded_worker_id_.value());
}

void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::
    AddMessageToConsole(blink::WebConsoleMessage::Level level,
                        const std::string& message) {
  // TODO(shimazu): Pass these arguments to the test helper when a test is
  // necessary to check them individually.
}

// static
void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::Bind(
    const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
    mojom::EmbeddedWorkerInstanceClientRequest request) {
  std::vector<std::unique_ptr<MockEmbeddedWorkerInstanceClient>>* clients =
      helper->mock_instance_clients();
  size_t next_client_index = helper->mock_instance_clients_next_index_;

  ASSERT_GE(clients->size(), next_client_index);
  if (clients->size() == next_client_index) {
    clients->push_back(
        base::MakeUnique<MockEmbeddedWorkerInstanceClient>(helper));
  }

  std::unique_ptr<MockEmbeddedWorkerInstanceClient>& client =
      clients->at(next_client_index);
  helper->mock_instance_clients_next_index_ = next_client_index + 1;
  if (client)
    client->binding_.Bind(std::move(request));
}

class EmbeddedWorkerTestHelper::MockServiceWorkerEventDispatcher
    : public NON_EXPORTED_BASE(mojom::ServiceWorkerEventDispatcher) {
 public:
  static void Create(const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
                     int thread_id,
                     mojom::ServiceWorkerEventDispatcherRequest request) {
    mojo::MakeStrongBinding(
        base::MakeUnique<MockServiceWorkerEventDispatcher>(helper, thread_id),
        std::move(request));
  }

  MockServiceWorkerEventDispatcher(
      const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
      int thread_id)
      : helper_(helper), thread_id_(thread_id) {}

  ~MockServiceWorkerEventDispatcher() override {}

  void DispatchActivateEvent(
      const DispatchActivateEventCallback& callback) override {
    if (!helper_)
      return;
    helper_->OnActivateEventStub(callback);
  }

  void DispatchFetchEvent(int fetch_event_id,
                          const ServiceWorkerFetchRequest& request,
                          mojom::FetchEventPreloadHandlePtr preload_handle,
                          const DispatchFetchEventCallback& callback) override {
    if (!helper_)
      return;
    helper_->OnFetchEventStub(thread_id_, fetch_event_id, request,
                              std::move(preload_handle), callback);
  }

  void DispatchNotificationClickEvent(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      const DispatchNotificationClickEventCallback& callback) override {
    if (!helper_)
      return;
    helper_->OnNotificationClickEventStub(notification_id, notification_data,
                                          action_index, reply, callback);
  }

  void DispatchNotificationCloseEvent(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      const DispatchNotificationCloseEventCallback& callback) override {
    if (!helper_)
      return;
    helper_->OnNotificationCloseEventStub(notification_id, notification_data,
                                          callback);
  }

  void DispatchPushEvent(const PushEventPayload& payload,
                         const DispatchPushEventCallback& callback) override {
    if (!helper_)
      return;
    helper_->OnPushEventStub(payload, callback);
  }

  void DispatchSyncEvent(
      const std::string& tag,
      blink::mojom::BackgroundSyncEventLastChance last_chance,
      const DispatchSyncEventCallback& callback) override {
    NOTIMPLEMENTED();
  }

  void DispatchPaymentRequestEvent(
      payments::mojom::PaymentAppRequestPtr app_request,
      const DispatchPaymentRequestEventCallback& callback) override {
    if (!helper_)
      return;
    helper_->OnPaymentRequestEventStub(std::move(app_request), callback);
  }

  void DispatchExtendableMessageEvent(
      mojom::ExtendableMessageEventPtr event,
      const DispatchExtendableMessageEventCallback& callback) override {
    if (!helper_)
      return;
    helper_->OnExtendableMessageEventStub(std::move(event), callback);
  }

 private:
  base::WeakPtr<EmbeddedWorkerTestHelper> helper_;
  const int thread_id_;
};

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory)
    : browser_context_(new TestBrowserContext),
      render_process_host_(new MockRenderProcessHost(browser_context_.get())),
      new_render_process_host_(
          new MockRenderProcessHost(browser_context_.get())),
      wrapper_(new ServiceWorkerContextWrapper(browser_context_.get())),
      mock_instance_clients_next_index_(0),
      next_thread_id_(0),
      mock_render_process_id_(render_process_host_->GetID()),
      new_mock_render_process_id_(new_render_process_host_->GetID()),
      weak_factory_(this) {
  std::unique_ptr<MockServiceWorkerDatabaseTaskManager> database_task_manager(
      new MockServiceWorkerDatabaseTaskManager(
          base::ThreadTaskRunnerHandle::Get()));
  wrapper_->InitInternal(user_data_directory, std::move(database_task_manager),
                         base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  wrapper_->process_manager()->SetProcessIdForTest(mock_render_process_id());
  wrapper_->process_manager()->SetNewProcessIdForTest(new_render_process_id());

  scoped_refptr<ServiceWorkerDispatcherHost> dispatcher_host(
      new MockServiceWorkerDispatcherHost(
          mock_render_process_id_, browser_context_->GetResourceContext(),
          this));
  wrapper_->context()->AddDispatcherHost(mock_render_process_id_,
                                         dispatcher_host.get());
  dispatcher_hosts_[mock_render_process_id_] = std::move(dispatcher_host);

  // Setup process level interface registry.
  render_process_interface_registry_ =
      CreateInterfaceRegistry(render_process_host_.get());
  new_render_process_interface_registry_ =
      CreateInterfaceRegistry(new_render_process_host_.get());
}

EmbeddedWorkerTestHelper::~EmbeddedWorkerTestHelper() {
  if (wrapper_.get())
    wrapper_->Shutdown();
}

void EmbeddedWorkerTestHelper::SimulateAddProcessToPattern(const GURL& pattern,
                                                           int process_id) {
  if (!context()->GetDispatcherHost(process_id)) {
    scoped_refptr<ServiceWorkerDispatcherHost> dispatcher_host(
        new MockServiceWorkerDispatcherHost(
            process_id, browser_context_->GetResourceContext(), this));
    wrapper_->context()->AddDispatcherHost(process_id, dispatcher_host.get());
    dispatcher_hosts_[process_id] = std::move(dispatcher_host);
  }
  wrapper_->process_manager()->AddProcessReferenceToPattern(pattern,
                                                            process_id);
}

bool EmbeddedWorkerTestHelper::Send(IPC::Message* message) {
  OnMessageReceived(*message);
  delete message;
  return true;
}

bool EmbeddedWorkerTestHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerTestHelper, message)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerContextMsg_MessageToWorker,
                        OnMessageToWorkerStub)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // IPC::TestSink only records messages that are not handled by filters,
  // so we just forward all messages to the separate sink.
  sink_.OnMessageReceived(message);

  return handled;
}

void EmbeddedWorkerTestHelper::RegisterMockInstanceClient(
    std::unique_ptr<MockEmbeddedWorkerInstanceClient> client) {
  mock_instance_clients_.push_back(std::move(client));
}

ServiceWorkerContextCore* EmbeddedWorkerTestHelper::context() {
  return wrapper_->context();
}

void EmbeddedWorkerTestHelper::ShutdownContext() {
  wrapper_->Shutdown();
  wrapper_ = NULL;
}

// static
net::HttpResponseInfo EmbeddedWorkerTestHelper::CreateHttpResponseInfo() {
  net::HttpResponseInfo info;
  const char data[] =
      "HTTP/1.1 200 OK\0"
      "Content-Type: application/javascript\0"
      "\0";
  info.headers =
      new net::HttpResponseHeaders(std::string(data, arraysize(data)));
  return info;
}

void EmbeddedWorkerTestHelper::OnStartWorker(
    int embedded_worker_id,
    int64_t service_worker_version_id,
    const GURL& scope,
    const GURL& script_url,
    bool pause_after_download,
    mojom::ServiceWorkerEventDispatcherRequest request) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  MockServiceWorkerEventDispatcher::Create(AsWeakPtr(), worker->thread_id(),
                                           std::move(request));

  embedded_worker_id_service_worker_version_id_map_[embedded_worker_id] =
      service_worker_version_id;
  SimulateWorkerReadyForInspection(embedded_worker_id);
  SimulateWorkerScriptCached(embedded_worker_id);
  SimulateWorkerScriptLoaded(embedded_worker_id);
  if (!pause_after_download)
    OnResumeAfterDownload(embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnResumeAfterDownload(int embedded_worker_id) {
  SimulateWorkerThreadStarted(GetNextThreadId(), embedded_worker_id);
  SimulateWorkerScriptEvaluated(embedded_worker_id, true /* success */);
  SimulateWorkerStarted(embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnStopWorker(
    const mojom::EmbeddedWorkerInstanceClient::StopWorkerCallback& callback) {
  // By default just notify the sender that the worker is stopped.
  callback.Run();
}

bool EmbeddedWorkerTestHelper::OnMessageToWorker(int thread_id,
                                                 int embedded_worker_id,
                                                 const IPC::Message& message) {
  bool handled = true;
  current_embedded_worker_id_ = embedded_worker_id;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerTestHelper, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_InstallEvent, OnInstallEventStub)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // Record all messages directed to inner script context.
  inner_sink_.OnMessageReceived(message);
  return handled;
}

void EmbeddedWorkerTestHelper::OnActivateEvent(
    const mojom::ServiceWorkerEventDispatcher::DispatchActivateEventCallback&
        callback) {
  dispatched_events()->push_back(Event::Activate);
  callback.Run(SERVICE_WORKER_OK, base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnExtendableMessageEvent(
    mojom::ExtendableMessageEventPtr event,
    const mojom::ServiceWorkerEventDispatcher::
        DispatchExtendableMessageEventCallback& callback) {
  callback.Run(SERVICE_WORKER_OK, base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnInstallEvent(int embedded_worker_id,
                                              int request_id) {
  // The installing worker may have been doomed and terminated.
  if (!registry()->GetWorker(embedded_worker_id))
    return;
  SimulateSend(new ServiceWorkerHostMsg_InstallEventFinished(
      embedded_worker_id, request_id,
      blink::WebServiceWorkerEventResultCompleted, true, base::Time::Now()));
}

void EmbeddedWorkerTestHelper::OnFetchEvent(
    int embedded_worker_id,
    int fetch_event_id,
    const ServiceWorkerFetchRequest& request,
    mojom::FetchEventPreloadHandlePtr preload_handle,
    const FetchCallback& callback) {
  SimulateSend(new ServiceWorkerHostMsg_FetchEventResponse(
      embedded_worker_id, fetch_event_id,
      SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
      ServiceWorkerResponse(
          base::MakeUnique<std::vector<GURL>>(), 200, "OK",
          blink::WebServiceWorkerResponseTypeDefault,
          base::MakeUnique<ServiceWorkerHeaderMap>(), std::string(), 0, GURL(),
          blink::WebServiceWorkerResponseErrorUnknown, base::Time(),
          false /* is_in_cache_storage */,
          std::string() /* cache_storage_cache_name */,
          base::MakeUnique<
              ServiceWorkerHeaderList>() /* cors_exposed_header_names */),
      base::Time::Now()));
  callback.Run(SERVICE_WORKER_OK, base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnPushEvent(
    const PushEventPayload& payload,
    const mojom::ServiceWorkerEventDispatcher::DispatchPushEventCallback&
        callback) {
  callback.Run(SERVICE_WORKER_OK, base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnNotificationClickEvent(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    const mojom::ServiceWorkerEventDispatcher::
        DispatchNotificationClickEventCallback& callback) {
  callback.Run(SERVICE_WORKER_OK, base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnNotificationCloseEvent(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    const mojom::ServiceWorkerEventDispatcher::
        DispatchNotificationCloseEventCallback& callback) {
  callback.Run(SERVICE_WORKER_OK, base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnPaymentRequestEvent(
    payments::mojom::PaymentAppRequestPtr app_request,
    const mojom::ServiceWorkerEventDispatcher::
        DispatchPaymentRequestEventCallback& callback) {
  callback.Run(SERVICE_WORKER_OK, base::Time::Now());
}

void EmbeddedWorkerTestHelper::SimulateWorkerReadyForInspection(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
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
  version->SetMainScriptHttpResponseInfo(CreateHttpResponseInfo());
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptLoaded(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  registry()->OnWorkerScriptLoaded(worker->process_id(), embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerThreadStarted(
    int thread_id,
    int embedded_worker_id) {
  thread_id_embedded_worker_id_map_[thread_id] = embedded_worker_id;
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  registry()->OnWorkerThreadStarted(worker->process_id(), thread_id,
                                    embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptEvaluated(
    int embedded_worker_id,
    bool success) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  registry()->OnWorkerScriptEvaluated(worker->process_id(), embedded_worker_id,
                                      success);
}

void EmbeddedWorkerTestHelper::SimulateWorkerStarted(int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  registry()->OnWorkerStarted(worker->process_id(), embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerStopped(int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  if (worker)
    registry()->OnWorkerStopped(worker->process_id(), embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateSend(IPC::Message* message) {
  registry()->OnMessageReceived(*message, mock_render_process_id_);
  delete message;
}

void EmbeddedWorkerTestHelper::OnStartWorkerStub(
    const EmbeddedWorkerStartParams& params,
    mojom::ServiceWorkerEventDispatcherRequest request) {
  EmbeddedWorkerInstance* worker =
      registry()->GetWorker(params.embedded_worker_id);
  ASSERT_TRUE(worker);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnStartWorker, AsWeakPtr(),
                 params.embedded_worker_id, params.service_worker_version_id,
                 params.scope, params.script_url, params.pause_after_download,
                 base::Passed(&request)));
}

void EmbeddedWorkerTestHelper::OnResumeAfterDownloadStub(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnResumeAfterDownload,
                            AsWeakPtr(), embedded_worker_id));
}

void EmbeddedWorkerTestHelper::OnStopWorkerStub(
    const mojom::EmbeddedWorkerInstanceClient::StopWorkerCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnStopWorker,
                            AsWeakPtr(), callback));
}

void EmbeddedWorkerTestHelper::OnMessageToWorkerStub(
    int thread_id,
    int embedded_worker_id,
    const IPC::Message& message) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  EXPECT_EQ(worker->thread_id(), thread_id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&EmbeddedWorkerTestHelper::OnMessageToWorker),
          AsWeakPtr(), thread_id, embedded_worker_id, message));
}

void EmbeddedWorkerTestHelper::OnActivateEventStub(
    const mojom::ServiceWorkerEventDispatcher::DispatchActivateEventCallback&
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnActivateEvent,
                            AsWeakPtr(), callback));
}

void EmbeddedWorkerTestHelper::OnExtendableMessageEventStub(
    mojom::ExtendableMessageEventPtr event,
    const mojom::ServiceWorkerEventDispatcher::
        DispatchExtendableMessageEventCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnExtendableMessageEvent,
                            AsWeakPtr(), base::Passed(&event), callback));
}

void EmbeddedWorkerTestHelper::OnInstallEventStub(int request_id) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnInstallEvent, AsWeakPtr(),
                 current_embedded_worker_id_, request_id));
}

void EmbeddedWorkerTestHelper::OnFetchEventStub(
    int thread_id,
    int fetch_event_id,
    const ServiceWorkerFetchRequest& request,
    mojom::FetchEventPreloadHandlePtr preload_handle,
    const FetchCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnFetchEvent, AsWeakPtr(),
                 thread_id_embedded_worker_id_map_[thread_id], fetch_event_id,
                 request, base::Passed(&preload_handle), callback));
}

void EmbeddedWorkerTestHelper::OnNotificationClickEventStub(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    const mojom::ServiceWorkerEventDispatcher::
        DispatchNotificationClickEventCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnNotificationClickEvent,
                            AsWeakPtr(), notification_id, notification_data,
                            action_index, reply, callback));
}

void EmbeddedWorkerTestHelper::OnNotificationCloseEventStub(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    const mojom::ServiceWorkerEventDispatcher::
        DispatchNotificationCloseEventCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnNotificationCloseEvent,
                 AsWeakPtr(), notification_id, notification_data, callback));
}

void EmbeddedWorkerTestHelper::OnPushEventStub(
    const PushEventPayload& payload,
    const mojom::ServiceWorkerEventDispatcher::DispatchPushEventCallback&
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnPushEvent, AsWeakPtr(),
                            payload, callback));
}

void EmbeddedWorkerTestHelper::OnPaymentRequestEventStub(
    payments::mojom::PaymentAppRequestPtr app_request,
    const mojom::ServiceWorkerEventDispatcher::
        DispatchPaymentRequestEventCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnPaymentRequestEvent, AsWeakPtr(),
                 base::Passed(std::move(app_request)), callback));
}

EmbeddedWorkerRegistry* EmbeddedWorkerTestHelper::registry() {
  DCHECK(context());
  return context()->embedded_worker_registry();
}

std::unique_ptr<service_manager::InterfaceRegistry>
EmbeddedWorkerTestHelper::CreateInterfaceRegistry(MockRenderProcessHost* rph) {
  auto registry =
      base::MakeUnique<service_manager::InterfaceRegistry>(std::string());
  registry->AddInterface(
      base::Bind(&MockEmbeddedWorkerInstanceClient::Bind, AsWeakPtr()));

  service_manager::mojom::InterfaceProviderPtr interfaces;
  registry->Bind(mojo::MakeRequest(&interfaces), service_manager::Identity(),
                 service_manager::InterfaceProviderSpec(),
                 service_manager::Identity(),
                 service_manager::InterfaceProviderSpec());

  std::unique_ptr<service_manager::InterfaceProvider> remote_interfaces(
      new service_manager::InterfaceProvider);
  remote_interfaces->Bind(std::move(interfaces));
  rph->SetRemoteInterfaces(std::move(remote_interfaces));
  return registry;
}

}  // namespace content
