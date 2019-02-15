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
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/renderer.mojom.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"

namespace content {

namespace {

void OnFetchEventCommon(
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    blink::mojom::ServiceWorker::DispatchFetchEventCallback finish_callback) {
  auto response = blink::mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->response_type = network::mojom::FetchResponseType::kDefault;
  response_callback->OnResponse(
      std::move(response), blink::mojom::ServiceWorkerFetchEventTiming::New());
  std::move(finish_callback)
      .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

}  // namespace

// A URLLoaderFactory that returns 200 OK with a simple body to any request.
class EmbeddedWorkerTestHelper::MockNetworkURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  MockNetworkURLLoaderFactory() = default;

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    std::string headers = "HTTP/1.1 200 OK\n\n";
    net::HttpResponseInfo info;
    info.headers = new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.length()));
    network::ResourceResponseHead response;
    response.headers = info.headers;
    response.headers->GetMimeType(&response.mime_type);
    client->OnReceiveResponse(response);

    std::string body = "this body came from the network";
    uint32_t bytes_written = body.size();
    mojo::DataPipe data_pipe;
    data_pipe.producer_handle->WriteData(body.data(), &bytes_written,
                                         MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

    network::URLLoaderCompletionStatus status;
    status.error_code = net::OK;
    client->OnComplete(status);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;
  DISALLOW_COPY_AND_ASSIGN(MockNetworkURLLoaderFactory);
};

class EmbeddedWorkerTestHelper::MockRendererInterface : public mojom::Renderer {
 public:
  // |helper| must outlive this.
  explicit MockRendererInterface(EmbeddedWorkerTestHelper* helper)
      : helper_(helper) {}

  void AddBinding(mojom::RendererAssociatedRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  void CreateEmbedderRendererService(
      service_manager::mojom::ServiceRequest service_request) override {
    NOTREACHED();
  }
  void CreateView(mojom::CreateViewParamsPtr) override { NOTREACHED(); }
  void CreateFrame(mojom::CreateFrameParamsPtr) override { NOTREACHED(); }
  void SetUpEmbeddedWorkerChannelForServiceWorker(
      blink::mojom::EmbeddedWorkerInstanceClientRequest client_request)
      override {
    helper_->OnInstanceClientRequest(std::move(client_request));
  }
  void CreateFrameProxy(
      int32_t routing_id,
      int32_t render_view_routing_id,
      int32_t opener_routing_id,
      int32_t parent_routing_id,
      const FrameReplicationState& replicated_state,
      const base::UnguessableToken& devtools_frame_token) override {
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
  void SetUserAgent(const std::string& user_agent) override { NOTREACHED(); }
  void SetUserAgentMetadata(const blink::UserAgentMetadata& metadata) override {
    NOTREACHED();
  }
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
  void SetProcessBackgrounded(bool backgrounded) override { NOTREACHED(); }
  void SetSchedulerKeepActive(bool keep_active) override { NOTREACHED(); }
  void ProcessPurgeAndSuspend() override { NOTREACHED(); }
  void SetIsLockedToSite(const GURL& lock_url) override { NOTREACHED(); }
  void EnableV8LowMemoryMode() override { NOTREACHED(); }

  EmbeddedWorkerTestHelper* helper_;
  mojo::AssociatedBindingSet<mojom::Renderer> bindings_;
};

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory)
    : browser_context_(std::make_unique<TestBrowserContext>()),
      render_process_host_(
          std::make_unique<MockRenderProcessHost>(browser_context_.get())),
      new_render_process_host_(
          std::make_unique<MockRenderProcessHost>(browser_context_.get())),
      wrapper_(base::MakeRefCounted<ServiceWorkerContextWrapper>(
          browser_context_.get())),
      next_thread_id_(0),
      mock_render_process_id_(render_process_host_->GetID()),
      new_mock_render_process_id_(new_render_process_host_->GetID()),
      url_loader_factory_getter_(
          base::MakeRefCounted<URLLoaderFactoryGetter>()),
      weak_factory_(this) {
  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadTaskRunnerHandle::Get();
  wrapper_->InitInternal(user_data_directory, std::move(database_task_runner),
                         nullptr, nullptr, nullptr,
                         url_loader_factory_getter_.get());
  wrapper_->process_manager()->SetProcessIdForTest(mock_render_process_id());
  wrapper_->process_manager()->SetNewProcessIdForTest(new_render_process_id());

  // Install a mocked mojom::Renderer interface to catch requests to
  // establish Mojo connection for EWInstanceClient.
  mock_renderer_interface_ = std::make_unique<MockRendererInterface>(this);

  auto renderer_interface_ptr =
      std::make_unique<mojom::RendererAssociatedPtr>();
  mock_renderer_interface_->AddBinding(
      mojo::MakeRequestAssociatedWithDedicatedPipe(
          renderer_interface_ptr.get()));
  render_process_host_->OverrideRendererInterfaceForTesting(
      std::move(renderer_interface_ptr));

  auto new_renderer_interface_ptr =
      std::make_unique<mojom::RendererAssociatedPtr>();
  mock_renderer_interface_->AddBinding(
      mojo::MakeRequestAssociatedWithDedicatedPipe(
          new_renderer_interface_ptr.get()));
  new_render_process_host_->OverrideRendererInterfaceForTesting(
      std::move(new_renderer_interface_ptr));

  if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    default_network_loader_factory_ =
        std::make_unique<MockNetworkURLLoaderFactory>();
    SetNetworkFactory(default_network_loader_factory_.get());
  }
}

void EmbeddedWorkerTestHelper::SetNetworkFactory(
    network::mojom::URLLoaderFactory* factory) {
  if (!factory)
    factory = default_network_loader_factory_.get();

  // Reset factory in URLLoaderFactoryGetter so that we don't hit DCHECK()
  // there.
  url_loader_factory_getter_->SetNetworkFactoryForTesting(nullptr);
  url_loader_factory_getter_->SetNetworkFactoryForTesting(factory);

  render_process_host_->OverrideURLLoaderFactory(factory);
  new_render_process_host_->OverrideURLLoaderFactory(factory);
}

void EmbeddedWorkerTestHelper::AddPendingInstanceClient(
    std::unique_ptr<FakeEmbeddedWorkerInstanceClient> client) {
  pending_embedded_worker_instance_clients_.push(std::move(client));
}

void EmbeddedWorkerTestHelper::AddPendingServiceWorker(
    std::unique_ptr<FakeServiceWorker> service_worker) {
  pending_service_workers_.push(std::move(service_worker));
}

void EmbeddedWorkerTestHelper::OnInstanceClientRequest(
    blink::mojom::EmbeddedWorkerInstanceClientRequest request) {
  std::unique_ptr<FakeEmbeddedWorkerInstanceClient> client;
  if (!pending_embedded_worker_instance_clients_.empty()) {
    // Use the instance client that was registered for this message.
    client = std::move(pending_embedded_worker_instance_clients_.front());
    pending_embedded_worker_instance_clients_.pop();
    if (!client) {
      // Some tests provide a nullptr to drop the request.
      return;
    }
  } else {
    client = CreateInstanceClient();
  }

  client->Bind(std::move(request));
  instance_clients_.insert(std::move(client));
}

void EmbeddedWorkerTestHelper::OnServiceWorkerRequest(
    blink::mojom::ServiceWorkerRequest request) {
  std::unique_ptr<FakeServiceWorker> service_worker;
  if (!pending_service_workers_.empty()) {
    // Use the service worker that was registered for this message.
    service_worker = std::move(pending_service_workers_.front());
    pending_service_workers_.pop();
    if (!service_worker) {
      // Some tests provide a nullptr to drop the request.
      return;
    }
  } else {
    service_worker = CreateServiceWorker();
  }

  service_worker->Bind(std::move(request));
  service_workers_.insert(std::move(service_worker));
}

void EmbeddedWorkerTestHelper::RemoveInstanceClient(
    FakeEmbeddedWorkerInstanceClient* instance_client) {
  auto it = instance_clients_.find(instance_client);
  instance_clients_.erase(it);
}

void EmbeddedWorkerTestHelper::RemoveServiceWorker(
    FakeServiceWorker* service_worker) {
  auto it = service_workers_.find(service_worker);
  service_workers_.erase(it);
}

EmbeddedWorkerTestHelper::~EmbeddedWorkerTestHelper() {
  if (wrapper_.get())
    wrapper_->Shutdown();
}

ServiceWorkerContextCore* EmbeddedWorkerTestHelper::context() {
  return wrapper_->context();
}

void EmbeddedWorkerTestHelper::ShutdownContext() {
  wrapper_->Shutdown();
  wrapper_ = nullptr;
}

// static
net::HttpResponseInfo EmbeddedWorkerTestHelper::CreateHttpResponseInfo() {
  net::HttpResponseInfo info;
  const char data[] =
      "HTTP/1.1 200 OK\0"
      "Content-Type: application/javascript\0"
      "\0";
  info.headers =
      new net::HttpResponseHeaders(std::string(data, base::size(data)));
  return info;
}

void EmbeddedWorkerTestHelper::PopulateScriptCacheMap(
    int64_t version_id,
    base::OnceClosure callback) {
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

void EmbeddedWorkerTestHelper::OnActivateEvent(
    blink::mojom::ServiceWorker::DispatchActivateEventCallback callback) {
  dispatched_events()->push_back(Event::Activate);
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchAbortEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    blink::mojom::ServiceWorker::DispatchBackgroundFetchAbortEventCallback
        callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchClickEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    blink::mojom::ServiceWorker::DispatchBackgroundFetchClickEventCallback
        callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchFailEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    blink::mojom::ServiceWorker::DispatchBackgroundFetchFailEventCallback
        callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchSuccessEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    blink::mojom::ServiceWorker::DispatchBackgroundFetchSuccessEventCallback
        callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnCookieChangeEvent(
    const net::CanonicalCookie& cookie,
    ::network::mojom::CookieChangeCause cause,
    blink::mojom::ServiceWorker::DispatchCookieChangeEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnExtendableMessageEvent(
    blink::mojom::ExtendableMessageEventPtr event,
    blink::mojom::ServiceWorker::DispatchExtendableMessageEventCallback
        callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnInstallEvent(
    blink::mojom::ServiceWorker::DispatchInstallEventCallback callback) {
  dispatched_events()->push_back(Event::Install);
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                          true /* has_fetch_handler */);
}

void EmbeddedWorkerTestHelper::OnFetchEvent(
    int /* embedded_worker_id */,
    blink::mojom::FetchAPIRequestPtr /* request */,
    blink::mojom::FetchEventPreloadHandlePtr /* preload_handle */,
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    blink::mojom::ServiceWorker::DispatchFetchEventCallback finish_callback) {
  // TODO(falken): In-line common into here.
  OnFetchEventCommon(std::move(response_callback), std::move(finish_callback));
}

void EmbeddedWorkerTestHelper::OnPushEvent(
    base::Optional<std::string> payload,
    blink::mojom::ServiceWorker::DispatchPushEventCallback callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnNotificationClickEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    blink::mojom::ServiceWorker::DispatchNotificationClickEventCallback
        callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnNotificationCloseEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    blink::mojom::ServiceWorker::DispatchNotificationCloseEventCallback
        callback) {
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnAbortPaymentEvent(
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    blink::mojom::ServiceWorker::DispatchAbortPaymentEventCallback callback) {
  response_callback->OnResponseForAbortPayment(true);
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnCanMakePaymentEvent(
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    blink::mojom::ServiceWorker::DispatchCanMakePaymentEventCallback callback) {
  bool can_make_payment = false;
  for (const auto& method_data : event_data->method_data) {
    if (method_data->supported_method == "test-method") {
      can_make_payment = true;
      break;
    }
  }
  response_callback->OnResponseForCanMakePayment(can_make_payment);
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnPaymentRequestEvent(
    payments::mojom::PaymentRequestEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    blink::mojom::ServiceWorker::DispatchPaymentRequestEventCallback callback) {
  response_callback->OnResponseForPaymentRequest(
      payments::mojom::PaymentHandlerResponse::New());
  std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
}

void EmbeddedWorkerTestHelper::OnSetIdleTimerDelayToZero(
    int embedded_worker_id) {
  // Subclasses may implement this method.
}

std::unique_ptr<FakeEmbeddedWorkerInstanceClient>
EmbeddedWorkerTestHelper::CreateInstanceClient() {
  return std::make_unique<FakeEmbeddedWorkerInstanceClient>(this);
}

std::unique_ptr<FakeServiceWorker>
EmbeddedWorkerTestHelper::CreateServiceWorker() {
  return std::make_unique<FakeServiceWorker>(this);
}

void EmbeddedWorkerTestHelper::OnActivateEventStub(
    blink::mojom::ServiceWorker::DispatchActivateEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedWorkerTestHelper::OnActivateEvent,
                                AsWeakPtr(), std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchAbortEventStub(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    blink::mojom::ServiceWorker::DispatchBackgroundFetchAbortEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnBackgroundFetchAbortEvent,
                     AsWeakPtr(), std::move(registration),
                     std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchClickEventStub(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    blink::mojom::ServiceWorker::DispatchBackgroundFetchClickEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnBackgroundFetchClickEvent,
                     AsWeakPtr(), std::move(registration),
                     std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchFailEventStub(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    blink::mojom::ServiceWorker::DispatchBackgroundFetchFailEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnBackgroundFetchFailEvent,
                     AsWeakPtr(), std::move(registration),
                     std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnBackgroundFetchSuccessEventStub(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    blink::mojom::ServiceWorker::DispatchBackgroundFetchSuccessEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnBackgroundFetchSuccessEvent,
                     AsWeakPtr(), std::move(registration),
                     std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnCookieChangeEventStub(
    const net::CanonicalCookie& cookie,
    ::network::mojom::CookieChangeCause cause,
    blink::mojom::ServiceWorker::DispatchCookieChangeEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnCookieChangeEvent,
                     AsWeakPtr(), cookie, cause, std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnExtendableMessageEventStub(
    blink::mojom::ExtendableMessageEventPtr event,
    blink::mojom::ServiceWorker::DispatchExtendableMessageEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnExtendableMessageEvent,
                     AsWeakPtr(), std::move(event), std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnInstallEventStub(
    blink::mojom::ServiceWorker::DispatchInstallEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedWorkerTestHelper::OnInstallEvent,
                                AsWeakPtr(), std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnFetchEventStub(
    int embedded_worker_id,
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::FetchEventPreloadHandlePtr preload_handle,
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    blink::mojom::ServiceWorker::DispatchFetchEventCallback finish_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnFetchEvent, AsWeakPtr(),
                     embedded_worker_id, std::move(request),
                     std::move(preload_handle), std::move(response_callback),
                     std::move(finish_callback)));
}

void EmbeddedWorkerTestHelper::OnNotificationClickEventStub(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    blink::mojom::ServiceWorker::DispatchNotificationClickEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnNotificationClickEvent,
                     AsWeakPtr(), notification_id, notification_data,
                     action_index, reply, std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnNotificationCloseEventStub(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    blink::mojom::ServiceWorker::DispatchNotificationCloseEventCallback
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnNotificationCloseEvent,
                     AsWeakPtr(), notification_id, notification_data,
                     std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnPushEventStub(
    base::Optional<std::string> payload,
    blink::mojom::ServiceWorker::DispatchPushEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnPushEvent, AsWeakPtr(),
                     std::move(payload), std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnAbortPaymentEventStub(
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    blink::mojom::ServiceWorker::DispatchAbortPaymentEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedWorkerTestHelper::OnAbortPaymentEvent,
                                AsWeakPtr(), std::move(response_callback),
                                std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnCanMakePaymentEventStub(
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    blink::mojom::ServiceWorker::DispatchCanMakePaymentEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnCanMakePaymentEvent,
                     AsWeakPtr(), std::move(event_data),
                     std::move(response_callback), std::move(callback)));
}

void EmbeddedWorkerTestHelper::OnPaymentRequestEventStub(
    payments::mojom::PaymentRequestEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    blink::mojom::ServiceWorker::DispatchPaymentRequestEventCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerTestHelper::OnPaymentRequestEvent,
                     AsWeakPtr(), std::move(event_data),
                     std::move(response_callback), std::move(callback)));
}

EmbeddedWorkerRegistry* EmbeddedWorkerTestHelper::registry() {
  DCHECK(context());
  return context()->embedded_worker_registry();
}

}  // namespace content
