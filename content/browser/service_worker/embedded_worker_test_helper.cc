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
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/renderer.mojom.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/embedded_worker_start_params.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/push_event_payload.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_event_status.mojom.h"

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
    mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
    mojom::ControllerServiceWorkerRequest controller_request,
    mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info,
    mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
    mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
    blink::mojom::WorkerContentSettingsProxyPtr content_settings_proxy) {
  if (!helper_)
    return;

  embedded_worker_id_ = params.embedded_worker_id;

  EmbeddedWorkerInstance* worker =
      helper_->registry()->GetWorker(params.embedded_worker_id);
  ASSERT_TRUE(worker);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());

  helper_->OnStartWorkerStub(params, std::move(dispatcher_request),
                             std::move(controller_request),
                             std::move(instance_host), std::move(provider_info),
                             std::move(installed_scripts_info));
}

void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::StopWorker() {
  if (!helper_)
    return;

  ASSERT_TRUE(embedded_worker_id_);
  EmbeddedWorkerInstance* worker =
      helper_->registry()->GetWorker(embedded_worker_id_.value());
  // |worker| is possible to be null when corresponding EmbeddedWorkerInstance
  // is removed right after sending StopWorker.
  if (worker)
    EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, worker->status());
  helper_->OnStopWorkerStub(embedded_worker_id_.value());
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
    mojom::EmbeddedWorkerInstanceClientAssociatedRequest request) {
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
    : public mojom::ServiceWorkerEventDispatcher {
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

  void DispatchInstallEvent(
      mojom::ServiceWorkerInstallEventMethodsAssociatedPtrInfo client,
      DispatchInstallEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnInstallEventStub(std::move(client), std::move(callback));
  }

  void DispatchActivateEvent(DispatchActivateEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnActivateEventStub(std::move(callback));
  }

  void DispatchBackgroundFetchAbortEvent(
      const std::string& developer_id,
      DispatchBackgroundFetchAbortEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnBackgroundFetchAbortEventStub(developer_id, std::move(callback));
  }

  void DispatchBackgroundFetchClickEvent(
      const std::string& developer_id,
      mojom::BackgroundFetchState state,
      DispatchBackgroundFetchClickEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnBackgroundFetchClickEventStub(developer_id, state,
                                             std::move(callback));
  }

  void DispatchBackgroundFetchFailEvent(
      const std::string& developer_id,
      const std::vector<BackgroundFetchSettledFetch>& fetches,
      DispatchBackgroundFetchFailEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnBackgroundFetchFailEventStub(developer_id, fetches,
                                            std::move(callback));
  }

  void DispatchBackgroundFetchedEvent(
      const std::string& developer_id,
      const std::string& unique_id,
      const std::vector<BackgroundFetchSettledFetch>& fetches,
      DispatchBackgroundFetchedEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnBackgroundFetchedEventStub(developer_id, unique_id, fetches,
                                          std::move(callback));
  }

  void DispatchFetchEvent(
      const ServiceWorkerFetchRequest& request,
      mojom::FetchEventPreloadHandlePtr preload_handle,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnFetchEventStub(thread_id_, request, std::move(preload_handle),
                              std::move(response_callback),
                              std::move(callback));
  }

  void DispatchNotificationClickEvent(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      DispatchNotificationClickEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnNotificationClickEventStub(notification_id, notification_data,
                                          action_index, reply,
                                          std::move(callback));
  }

  void DispatchNotificationCloseEvent(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      DispatchNotificationCloseEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnNotificationCloseEventStub(notification_id, notification_data,
                                          std::move(callback));
  }

  void DispatchPushEvent(const PushEventPayload& payload,
                         DispatchPushEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnPushEventStub(payload, std::move(callback));
  }

  void DispatchSyncEvent(
      const std::string& tag,
      blink::mojom::BackgroundSyncEventLastChance last_chance,
      DispatchSyncEventCallback callback) override {
    NOTIMPLEMENTED();
  }

  void DispatchAbortPaymentEvent(
      int payment_request_id,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchAbortPaymentEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnAbortPaymentEventStub(std::move(response_callback),
                                     std::move(callback));
  }

  void DispatchCanMakePaymentEvent(
      int payment_request_id,
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchCanMakePaymentEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnCanMakePaymentEventStub(std::move(event_data),
                                       std::move(response_callback),
                                       std::move(callback));
  }

  void DispatchPaymentRequestEvent(
      int payment_request_id,
      payments::mojom::PaymentRequestEventDataPtr event_data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchPaymentRequestEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnPaymentRequestEventStub(std::move(event_data),
                                       std::move(response_callback),
                                       std::move(callback));
  }

  void DispatchExtendableMessageEvent(
      mojom::ExtendableMessageEventPtr event,
      DispatchExtendableMessageEventCallback callback) override {
    if (!helper_)
      return;
    helper_->OnExtendableMessageEventStub(std::move(event),
                                          std::move(callback));
  }

  void Ping(PingCallback callback) override { std::move(callback).Run(); }

 private:
  base::WeakPtr<EmbeddedWorkerTestHelper> helper_;
  const int thread_id_;
};

class EmbeddedWorkerTestHelper::MockRendererInterface : public mojom::Renderer {
 public:
  explicit MockRendererInterface(base::WeakPtr<EmbeddedWorkerTestHelper> helper)
      : helper_(helper) {}

  void AddBinding(mojom::RendererAssociatedRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  void CreateView(mojom::CreateViewParamsPtr) override { NOTREACHED(); }
  void CreateFrame(mojom::CreateFrameParamsPtr) override { NOTREACHED(); }
  void SetUpEmbeddedWorkerChannelForServiceWorker(
      mojom::EmbeddedWorkerInstanceClientAssociatedRequest client_request)
      override {
    MockEmbeddedWorkerInstanceClient::Bind(helper_, std::move(client_request));
  }
  void CreateFrameProxy(
      int32_t routing_id,
      int32_t render_view_routing_id,
      int32_t opener_routing_id,
      int32_t parent_routing_id,
      const FrameReplicationState& replicated_state) override {
    NOTREACHED();
  }
  void OnNetworkConnectionChanged(
      net::NetworkChangeNotifier::ConnectionType type,
      double max_bandwidth_mbps) override {
    NOTREACHED();
  }
  void OnNetworkQualityChanged(net::EffectiveConnectionType type,
                               base::TimeDelta http_rtt,
                               base::TimeDelta transport_rtt,
                               double bandwidth_kbps) override {
    NOTREACHED();
  }
  void SetWebKitSharedTimersSuspended(bool suspend) override { NOTREACHED(); }
  void UpdateScrollbarTheme(
      mojom::UpdateScrollbarThemeParamsPtr params) override {
    NOTREACHED();
  }
  void OnSystemColorsChanged(int32_t aqua_color_variant,
                             const std::string& highlight_text_color,
                             const std::string& highlight_color) override {
    NOTREACHED();
  }
  void PurgePluginListCache(bool reload_pages) override { NOTREACHED(); }

  base::WeakPtr<EmbeddedWorkerTestHelper> helper_;
  mojo::AssociatedBindingSet<mojom::Renderer> bindings_;
};

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory)
    : EmbeddedWorkerTestHelper(user_data_directory, nullptr) {}

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter)
    : browser_context_(base::MakeUnique<TestBrowserContext>()),
      render_process_host_(
          base::MakeUnique<MockRenderProcessHost>(browser_context_.get())),
      new_render_process_host_(
          base::MakeUnique<MockRenderProcessHost>(browser_context_.get())),
      wrapper_(base::MakeRefCounted<ServiceWorkerContextWrapper>(
          browser_context_.get())),
      mock_instance_clients_next_index_(0),
      next_thread_id_(0),
      mock_render_process_id_(render_process_host_->GetID()),
      new_mock_render_process_id_(new_render_process_host_->GetID()),
      url_loader_factory_getter_(std::move(url_loader_factory_getter)),
      weak_factory_(this) {
  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadTaskRunnerHandle::Get();
  wrapper_->InitInternal(user_data_directory, std::move(database_task_runner),
                         nullptr, nullptr, nullptr,
                         url_loader_factory_getter_.get());
  wrapper_->process_manager()->SetProcessIdForTest(mock_render_process_id());
  wrapper_->process_manager()->SetNewProcessIdForTest(new_render_process_id());

  scoped_refptr<ServiceWorkerDispatcherHost> dispatcher_host(
      base::MakeRefCounted<MockServiceWorkerDispatcherHost>(
          mock_render_process_id_, browser_context_->GetResourceContext(),
          this));
  dispatcher_host->Init(wrapper_.get());
  dispatcher_hosts_[mock_render_process_id_] = std::move(dispatcher_host);

  // Install a mocked mojom::Renderer interface to catch requests to
  // establish Mojo connection for EWInstanceClient.
  mock_renderer_interface_ =
      base::MakeUnique<MockRendererInterface>(AsWeakPtr());

  auto renderer_interface_ptr =
      base::MakeUnique<mojom::RendererAssociatedPtr>();
  mock_renderer_interface_->AddBinding(
      mojo::MakeIsolatedRequest(renderer_interface_ptr.get()));
  render_process_host_->OverrideRendererInterfaceForTesting(
      std::move(renderer_interface_ptr));

  auto new_renderer_interface_ptr =
      base::MakeUnique<mojom::RendererAssociatedPtr>();
  mock_renderer_interface_->AddBinding(
      mojo::MakeIsolatedRequest(new_renderer_interface_ptr.get()));
  new_render_process_host_->OverrideRendererInterfaceForTesting(
      std::move(new_renderer_interface_ptr));
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
    dispatcher_host->Init(wrapper_.get());
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

void EmbeddedWorkerTestHelper::RegisterDispatcherHost(
    int process_id,
    scoped_refptr<ServiceWorkerDispatcherHost> dispatcher_host) {
  dispatcher_hosts_[process_id] = std::move(dispatcher_host);
}

ServiceWorkerContextCore* EmbeddedWorkerTestHelper::context() {
  return wrapper_->context();
}

void EmbeddedWorkerTestHelper::ShutdownContext() {
  wrapper_->Shutdown();
  wrapper_ = NULL;
}

ServiceWorkerDispatcherHost*
EmbeddedWorkerTestHelper::GetDispatcherHostForProcess(int process_id) {
  return dispatcher_hosts_[process_id].get();
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
    mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
    mojom::ControllerServiceWorkerRequest controller_request,
    mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
    mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
    mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  MockServiceWorkerEventDispatcher::Create(AsWeakPtr(), worker->thread_id(),
                                           std::move(dispatcher_request));
  embedded_worker_id_service_worker_version_id_map_[embedded_worker_id] =
      service_worker_version_id;
  embedded_worker_id_instance_host_ptr_map_[embedded_worker_id].Bind(
      std::move(instance_host));
  embedded_worker_id_installed_scripts_info_map_[embedded_worker_id] =
      std::move(installed_scripts_info);
  ServiceWorkerRemoteProviderEndpoint* provider_endpoint =
      &embedded_worker_id_remote_provider_map_[embedded_worker_id];
  provider_endpoint->BindWithProviderInfo(std::move(provider_info));

  SimulateWorkerReadyForInspection(embedded_worker_id);
  SimulateWorkerScriptCached(
      embedded_worker_id,
      base::BindOnce(&EmbeddedWorkerTestHelper::DidSimulateWorkerScriptCached,
                     AsWeakPtr(), embedded_worker_id, pause_after_download));
}

void EmbeddedWorkerTestHelper::DidSimulateWorkerScriptCached(
    int embedded_worker_id,
    bool pause_after_download) {
  SimulateWorkerScriptLoaded(embedded_worker_id);
  if (!pause_after_download)
    OnResumeAfterDownload(embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnResumeAfterDownload(int embedded_worker_id) {
  SimulateWorkerThreadStarted(GetNextThreadId(), embedded_worker_id);
  SimulateWorkerScriptEvaluated(embedded_worker_id, true /* success */);
  SimulateWorkerStarted(embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnStopWorker(int embedded_worker_id) {
  // By default just notify the sender that the worker is stopped.
  SimulateWorkerStopped(embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnActivateEvent(
    mojom::ServiceWorkerEventDispatcher::DispatchActivateEventCallback
        callback) {
  dispatched_events()->push_back(Event::Activate);
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchAbortEvent(
    const std::string& developer_id,
    mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchAbortEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchClickEvent(
    const std::string& developer_id,
    mojom::BackgroundFetchState state,
    mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchClickEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchFailEvent(
    const std::string& developer_id,
    const std::vector<BackgroundFetchSettledFetch>& fetches,
    mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchFailEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchedEvent(
    const std::string& developer_id,
    const std::string& unique_id,
    const std::vector<BackgroundFetchSettledFetch>& fetches,
    mojom::ServiceWorkerEventDispatcher::DispatchBackgroundFetchedEventCallback
        callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnExtendableMessageEvent(
    mojom::ExtendableMessageEventPtr event,
    mojom::ServiceWorkerEventDispatcher::DispatchExtendableMessageEventCallback
        callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnInstallEvent(
    mojom::ServiceWorkerInstallEventMethodsAssociatedPtrInfo client,
    mojom::ServiceWorkerEventDispatcher::DispatchInstallEventCallback
        callback) {
  dispatched_events()->push_back(Event::Install);
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          true /* has_fetch_handler */, base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnFetchEvent(
    int /* embedded_worker_id */,
    const ServiceWorkerFetchRequest& /* request */,
    mojom::FetchEventPreloadHandlePtr /* preload_handle */,
    mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    mojom::ServiceWorkerEventDispatcher::DispatchFetchEventCallback
        finish_callback) {
  response_callback->OnResponse(
      ServiceWorkerResponse(
          base::MakeUnique<std::vector<GURL>>(), 200, "OK",
          network::mojom::FetchResponseType::kDefault,
          base::MakeUnique<ServiceWorkerHeaderMap>(), std::string(), 0,
          nullptr /* blob */, blink::kWebServiceWorkerResponseErrorUnknown,
          base::Time(), false /* is_in_cache_storage */,
          std::string() /* cache_storage_cache_name */,
          base::MakeUnique<
              ServiceWorkerHeaderList>() /* cors_exposed_header_names */),
      base::Time::Now());
  std::move(finish_callback)
      .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
           base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnPushEvent(
    const PushEventPayload& payload,
    mojom::ServiceWorkerEventDispatcher::DispatchPushEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnNotificationClickEvent(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    mojom::ServiceWorkerEventDispatcher::DispatchNotificationClickEventCallback
        callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnNotificationCloseEvent(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    mojom::ServiceWorkerEventDispatcher::DispatchNotificationCloseEventCallback
        callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnAbortPaymentEvent(
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    mojom::ServiceWorkerEventDispatcher::DispatchAbortPaymentEventCallback
        callback) {
  response_callback->OnResponseForAbortPayment(true, base::Time::Now());
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnCanMakePaymentEvent(
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    mojom::ServiceWorkerEventDispatcher::DispatchCanMakePaymentEventCallback
        callback) {
  bool can_make_payment = false;
  for (const auto& method_data : event_data->method_data) {
    for (const auto& method : method_data->supported_methods) {
      if (method == "test-method") {
        can_make_payment = true;
        break;
      }
    }
  }
  response_callback->OnResponseForCanMakePayment(can_make_payment,
                                                 base::Time::Now());
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnPaymentRequestEvent(
    payments::mojom::PaymentRequestEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    mojom::ServiceWorkerEventDispatcher::DispatchPaymentRequestEventCallback
        callback) {
  response_callback->OnResponseForPaymentRequest(
      payments::mojom::PaymentHandlerResponse::New(), base::Time::Now());
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          base::Time::Now());
}

void EmbeddedWorkerTestHelper::SimulateWorkerReadyForInspection(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  ASSERT_TRUE(embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]);
  embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]
      ->OnReadyForInspection();
  base::RunLoop().RunUntilIdle();
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptCached(
    int embedded_worker_id,
    base::OnceClosure callback) {
  int64_t version_id =
      embedded_worker_id_service_worker_version_id_map_[embedded_worker_id];
  ServiceWorkerVersion* version = context()->GetLiveVersion(version_id);
  if (!version) {
    std::move(callback).Run();
    return;
  }
  if (!version->script_cache_map()->size()) {
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    // Add a dummy ResourceRecord for the main script to the script cache map of
    // the ServiceWorkerVersion.
    records.push_back(WriteToDiskCacheAsync(
        context()->storage(), version->script_url(),
        context()->storage()->NewResourceId(), {} /* headers */, "I'm a body",
        "I'm a meta data", std::move(callback)));
    version->script_cache_map()->SetResources(records);
  }
  if (!version->GetMainScriptHttpResponseInfo())
    version->SetMainScriptHttpResponseInfo(CreateHttpResponseInfo());
  // Call |callback| if |version| already has ResourceRecords.
  if (!callback.is_null())
    std::move(callback).Run();
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptLoaded(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  ASSERT_TRUE(embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]);
  embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]
      ->OnScriptLoaded();
  base::RunLoop().RunUntilIdle();
}

void EmbeddedWorkerTestHelper::SimulateWorkerThreadStarted(
    int thread_id,
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  ASSERT_TRUE(embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]);
  embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]
      ->OnThreadStarted(thread_id);
  base::RunLoop().RunUntilIdle();
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptEvaluated(
    int embedded_worker_id,
    bool success) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  ASSERT_TRUE(embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]);
  embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]
      ->OnScriptEvaluated(success);
  base::RunLoop().RunUntilIdle();
}

void EmbeddedWorkerTestHelper::SimulateWorkerStarted(int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  ASSERT_TRUE(embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]);
  embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]->OnStarted(
      mojom::EmbeddedWorkerStartTiming::New());
  base::RunLoop().RunUntilIdle();
}

void EmbeddedWorkerTestHelper::SimulateWorkerStopped(int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  if (worker) {
    ASSERT_TRUE(embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]);
    embedded_worker_id_instance_host_ptr_map_[embedded_worker_id]->OnStopped();
    embedded_worker_id_remote_provider_map_.erase(embedded_worker_id);
    base::RunLoop().RunUntilIdle();
  }
}

void EmbeddedWorkerTestHelper::SimulateSend(IPC::Message* message) {
  registry()->OnMessageReceived(*message, mock_render_process_id_);
  delete message;
}

void EmbeddedWorkerTestHelper::OnStartWorkerStub(
    const EmbeddedWorkerStartParams& params,
    mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
    mojom::ControllerServiceWorkerRequest controller_request,
    mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
    mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
    mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info) {
  EmbeddedWorkerInstance* worker =
      registry()->GetWorker(params.embedded_worker_id);
  ASSERT_TRUE(worker);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EmbeddedWorkerTestHelper::OnStartWorker, AsWeakPtr(),
          params.embedded_worker_id, params.service_worker_version_id,
          params.scope, params.script_url, params.pause_after_download,
          std::move(dispatcher_request), std::move(controller_request),
          std::move(instance_host), std::move(provider_info),
          std::move(installed_scripts_info)));
}

void EmbeddedWorkerTestHelper::OnResumeAfterDownloadStub(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnResumeAfterDownload,
                     AsWeakPtr(), embedded_worker_id));
}

void EmbeddedWorkerTestHelper::OnStopWorkerStub(int embedded_worker_id) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedWorkerTestHelper::OnStopWorker,
                                AsWeakPtr(), embedded_worker_id));
}

void EmbeddedWorkerTestHelper::OnMessageToWorkerStub(
    int thread_id,
    int embedded_worker_id,
    const IPC::Message& message) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  EXPECT_EQ(worker->thread_id(), thread_id);
}

void EmbeddedWorkerTestHelper::OnActivateEventStub(
    mojom::ServiceWorkerEventDispatcher::DispatchActivateEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedWorkerTestHelper::OnActivateEvent,
                                AsWeakPtr(), base::Passed(&callback)));
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchAbortEventStub(
    const std::string& developer_id,
    mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchAbortEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnBackgroundFetchAbortEvent,
                     AsWeakPtr(), developer_id, base::Passed(&callback)));
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchClickEventStub(
    const std::string& developer_id,
    mojom::BackgroundFetchState state,
    mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchClickEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnBackgroundFetchClickEvent,
                     AsWeakPtr(), developer_id, state,
                     base::Passed(&callback)));
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchFailEventStub(
    const std::string& developer_id,
    const std::vector<BackgroundFetchSettledFetch>& fetches,
    mojom::ServiceWorkerEventDispatcher::
        DispatchBackgroundFetchFailEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnBackgroundFetchFailEvent,
                     AsWeakPtr(), developer_id, fetches,
                     base::Passed(&callback)));
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchedEventStub(
    const std::string& developer_id,
    const std::string& unique_id,
    const std::vector<BackgroundFetchSettledFetch>& fetches,
    mojom::ServiceWorkerEventDispatcher::DispatchBackgroundFetchedEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnBackgroundFetchedEvent,
                     AsWeakPtr(), developer_id, unique_id, fetches,
                     base::Passed(&callback)));
}

void EmbeddedWorkerTestHelper::OnExtendableMessageEventStub(
    mojom::ExtendableMessageEventPtr event,
    mojom::ServiceWorkerEventDispatcher::DispatchExtendableMessageEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnExtendableMessageEvent,
                     AsWeakPtr(), base::Passed(&event),
                     base::Passed(&callback)));
}

void EmbeddedWorkerTestHelper::OnInstallEventStub(
    mojom::ServiceWorkerInstallEventMethodsAssociatedPtrInfo client,
    mojom::ServiceWorkerEventDispatcher::DispatchInstallEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnInstallEvent, AsWeakPtr(),
                     base::Passed(&client), base::Passed(&callback)));
}

void EmbeddedWorkerTestHelper::OnFetchEventStub(
    int thread_id,
    const ServiceWorkerFetchRequest& request,
    mojom::FetchEventPreloadHandlePtr preload_handle,
    mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    mojom::ServiceWorkerEventDispatcher::DispatchFetchEventCallback
        finish_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnFetchEvent, AsWeakPtr(),
                     thread_id_embedded_worker_id_map_[thread_id], request,
                     base::Passed(&preload_handle),
                     base::Passed(&response_callback),
                     base::Passed(&finish_callback)));
}

void EmbeddedWorkerTestHelper::OnNotificationClickEventStub(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    mojom::ServiceWorkerEventDispatcher::DispatchNotificationClickEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnNotificationClickEvent,
                     AsWeakPtr(), notification_id, notification_data,
                     action_index, reply, base::Passed(&callback)));
}

void EmbeddedWorkerTestHelper::OnNotificationCloseEventStub(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    mojom::ServiceWorkerEventDispatcher::DispatchNotificationCloseEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnNotificationCloseEvent,
                     AsWeakPtr(), notification_id, notification_data,
                     base::Passed(&callback)));
}

void EmbeddedWorkerTestHelper::OnPushEventStub(
    const PushEventPayload& payload,
    mojom::ServiceWorkerEventDispatcher::DispatchPushEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedWorkerTestHelper::OnPushEvent,
                                AsWeakPtr(), payload, base::Passed(&callback)));
}

void EmbeddedWorkerTestHelper::OnAbortPaymentEventStub(
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    mojom::ServiceWorkerEventDispatcher::DispatchAbortPaymentEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedWorkerTestHelper::OnAbortPaymentEvent,
                                AsWeakPtr(), base::Passed(&response_callback),
                                base::Passed(&callback)));
}

void EmbeddedWorkerTestHelper::OnCanMakePaymentEventStub(
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    mojom::ServiceWorkerEventDispatcher::DispatchCanMakePaymentEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnCanMakePaymentEvent,
                     AsWeakPtr(), base::Passed(&event_data),
                     base::Passed(&response_callback),
                     base::Passed(&callback)));
}

void EmbeddedWorkerTestHelper::OnPaymentRequestEventStub(
    payments::mojom::PaymentRequestEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    mojom::ServiceWorkerEventDispatcher::DispatchPaymentRequestEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnPaymentRequestEvent,
                     AsWeakPtr(), base::Passed(&event_data),
                     base::Passed(&response_callback),
                     base::Passed(&callback)));
}

EmbeddedWorkerRegistry* EmbeddedWorkerTestHelper::registry() {
  DCHECK(context());
  return context()->embedded_worker_registry();
}

}  // namespace content
