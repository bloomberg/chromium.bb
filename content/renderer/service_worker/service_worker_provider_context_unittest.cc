// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_provider_context.h"

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/content_features.h"
#include "content/public/common/resource_type.h"
#include "content/public/renderer/child_url_loader_factory_getter.h"
#include "content/public/test/test_url_loader_client.h"
#include "content/renderer/loader/child_url_loader_factory_getter_impl.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "content/renderer/service_worker/service_worker_dispatcher.h"
#include "content/renderer/service_worker/service_worker_handle_reference.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/web_service_worker_impl.h"
#include "content/renderer/service_worker/web_service_worker_registration_impl.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/common/service_worker/service_worker_error_type.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_registration.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"
#include "third_party/WebKit/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/WebKit/public/platform/web_feature.mojom.h"

namespace content {
namespace service_worker_provider_context_unittest {

class MockServiceWorkerRegistrationObjectHost
    : public blink::mojom::ServiceWorkerRegistrationObjectHost {
 public:
  MockServiceWorkerRegistrationObjectHost() {
    bindings_.set_connection_error_handler(
        base::Bind(&MockServiceWorkerRegistrationObjectHost::OnConnectionError,
                   base::Unretained(this)));
  }
  ~MockServiceWorkerRegistrationObjectHost() override = default;

  void AddBinding(
      blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedRequest
          request) {
    bindings_.AddBinding(this, std::move(request));
  }

  blink::mojom::ServiceWorkerRegistrationObjectAssociatedRequest
  CreateRegistrationObjectRequest() {
    if (!remote_registration_)
      return mojo::MakeRequest(&remote_registration_);
    return nullptr;
  }

  int GetBindingCount() const { return bindings_.size(); }

 private:
  // Implements blink::mojom::ServiceWorkerRegistrationObjectHost.
  void Update(UpdateCallback callback) override {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                            base::nullopt);
  }
  void Unregister(UnregisterCallback callback) override {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                            base::nullopt);
  }
  void EnableNavigationPreload(
      bool enable,
      EnableNavigationPreloadCallback callback) override {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                            base::nullopt);
  }
  void GetNavigationPreloadState(
      GetNavigationPreloadStateCallback callback) override {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                            base::nullopt, nullptr);
  }
  void SetNavigationPreloadHeader(
      const std::string& value,
      SetNavigationPreloadHeaderCallback callback) override {
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                            base::nullopt);
  }

  void OnConnectionError() {
    // If there are still bindings, |this| is still being used.
    if (!bindings_.empty())
      return;
    // Will destroy corresponding remote WebServiceWorkerRegistrationImpl
    // instance.
    remote_registration_.reset();
  }

  mojo::AssociatedBindingSet<blink::mojom::ServiceWorkerRegistrationObjectHost>
      bindings_;
  blink::mojom::ServiceWorkerRegistrationObjectAssociatedPtr
      remote_registration_;
};

class MockWebServiceWorkerProviderClientImpl
    : public blink::WebServiceWorkerProviderClient {
 public:
  MockWebServiceWorkerProviderClientImpl() {}

  ~MockWebServiceWorkerProviderClientImpl() override {}

  void SetController(std::unique_ptr<blink::WebServiceWorker::Handle> handle,
                     bool should_notify_controller_change) override {
    was_set_controller_called_ = true;
  }

  void DispatchMessageEvent(
      std::unique_ptr<blink::WebServiceWorker::Handle> handle,
      const blink::WebString& message,
      blink::WebVector<blink::MessagePortChannel> ports) override {
    was_dispatch_message_event_called_ = true;
  }

  void CountFeature(blink::mojom::WebFeature feature) override {
    used_features_.insert(feature);
  }

  bool was_set_controller_called() const { return was_set_controller_called_; }

  bool was_dispatch_message_event_called() const {
    return was_dispatch_message_event_called_;
  }

  const std::set<blink::mojom::WebFeature>& used_features() const {
    return used_features_;
  }

 private:
  bool was_set_controller_called_ = false;
  bool was_dispatch_message_event_called_ = false;
  std::set<blink::mojom::WebFeature> used_features_;
};

class ServiceWorkerTestSender : public ThreadSafeSender {
 public:
  explicit ServiceWorkerTestSender(IPC::TestSink* ipc_sink)
      : ThreadSafeSender(nullptr, nullptr), ipc_sink_(ipc_sink) {}

  bool Send(IPC::Message* message) override { return ipc_sink_->Send(message); }

 private:
  ~ServiceWorkerTestSender() override {}

  IPC::TestSink* ipc_sink_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerTestSender);
};

// S13nServiceWorker: a fake URLLoaderFactory implementation that basically
// does nothing but records the requests.
class FakeURLLoaderFactory final : public network::mojom::URLLoaderFactory {
 public:
  FakeURLLoaderFactory() = default;
  ~FakeURLLoaderFactory() override = default;

  void AddBinding(network::mojom::URLLoaderFactoryRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    // Does nothing, but just record the request and hold the client (to avoid
    // connection errors).
    last_url_ = url_request.url;
    clients_.push_back(std::move(client));
  }
  void Clone(network::mojom::URLLoaderFactoryRequest factory) override {
    NOTREACHED();
  }

  size_t clients_count() const { return clients_.size(); }
  GURL last_request_url() const { return last_url_; }

 private:
  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;
  std::vector<network::mojom::URLLoaderClientPtr> clients_;
  GURL last_url_;

  DISALLOW_COPY_AND_ASSIGN(FakeURLLoaderFactory);
};

// S13nServiceWorker: a fake ControllerServiceWorker implementation that
// basically does nothing but records DispatchFetchEvent calls.
class FakeControllerServiceWorker : public mojom::ControllerServiceWorker {
 public:
  FakeControllerServiceWorker() = default;
  ~FakeControllerServiceWorker() override = default;

  // mojom::ControllerServiceWorker:
  void DispatchFetchEvent(
      const network::ResourceRequest& request,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback) override {
    fetch_event_count_++;
    fetch_event_request_ = request;
    std::move(callback).Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                            base::Time());
  }
  void Clone(mojom::ControllerServiceWorkerRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  int fetch_event_count() const { return fetch_event_count_; }
  const network::ResourceRequest& fetch_event_request() const {
    return fetch_event_request_;
  }

 private:
  int fetch_event_count_ = 0;
  network::ResourceRequest fetch_event_request_;
  base::OnceClosure fetch_event_callback_;
  mojo::BindingSet<mojom::ControllerServiceWorker> bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakeControllerServiceWorker);
};

class ServiceWorkerProviderContextTest : public testing::Test {
 public:
  ServiceWorkerProviderContextTest() = default;

  void SetUp() override {
    sender_ = new ServiceWorkerTestSender(&ipc_sink_);
    dispatcher_.reset(new ServiceWorkerDispatcher(sender_.get(), nullptr));
  }

  void EnableS13nServiceWorker() {
    scoped_feature_list_.InitAndEnableFeature(features::kNetworkService);
    network::mojom::URLLoaderFactoryPtr fake_loader_factory;
    fake_loader_factory_.AddBinding(MakeRequest(&fake_loader_factory));
    loader_factory_getter_ =
        base::MakeRefCounted<ChildURLLoaderFactoryGetterImpl>(
            std::move(fake_loader_factory), nullptr);
  }

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
  CreateServiceWorkerRegistrationObjectInfo() {
    auto info = blink::mojom::ServiceWorkerRegistrationObjectInfo::New();
    info->registration_id = 20;
    remote_registration_object_host_.AddBinding(
        mojo::MakeRequest(&info->host_ptr_info));
    info->request =
        remote_registration_object_host_.CreateRegistrationObjectRequest();

    info->active = blink::mojom::ServiceWorkerObjectInfo::New();
    info->active->handle_id = 100;
    info->active->version_id = 200;
    info->waiting = blink::mojom::ServiceWorkerObjectInfo::New();
    info->waiting->handle_id = 101;
    info->waiting->version_id = 201;
    info->installing = blink::mojom::ServiceWorkerObjectInfo::New();
    info->installing->handle_id = 102;
    info->installing->version_id = 202;
    return info;
  }

  void StartRequest(network::mojom::URLLoaderFactory* factory,
                    const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.resource_type = static_cast<int>(RESOURCE_TYPE_SUB_RESOURCE);
    network::mojom::URLLoaderPtr loader;
    TestURLLoaderClient loader_client;
    factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0, 0, network::mojom::kURLLoadOptionNone,
        request, loader_client.CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    // Need to run one more loop to make a Mojo call.
    base::RunLoop().RunUntilIdle();
  }

  bool ContainsRegistration(ServiceWorkerProviderContext* provider_context,
                            int64_t registration_id) {
    return provider_context->ContainsServiceWorkerRegistrationForTesting(
        registration_id);
  }

  ThreadSafeSender* thread_safe_sender() { return sender_.get(); }
  IPC::TestSink* ipc_sink() { return &ipc_sink_; }
  const MockServiceWorkerRegistrationObjectHost&
  remote_registration_object_host() const {
    return remote_registration_object_host_;
  }

 protected:
  base::MessageLoop message_loop_;
  IPC::TestSink ipc_sink_;
  std::unique_ptr<ServiceWorkerDispatcher> dispatcher_;
  scoped_refptr<ServiceWorkerTestSender> sender_;
  MockServiceWorkerRegistrationObjectHost remote_registration_object_host_;

  // S13nServiceWorker:
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeURLLoaderFactory fake_loader_factory_;
  scoped_refptr<ChildURLLoaderFactoryGetter> loader_factory_getter_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderContextTest);
};

TEST_F(ServiceWorkerProviderContextTest, CreateForController) {
  // Assume that these objects are passed from the browser process and own
  // references to browser-side registration/worker representations.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info =
      CreateServiceWorkerRegistrationObjectInfo();
  // ServiceWorkerRegistrationObjectHost Mojo connection has been added.
  ASSERT_EQ(1, remote_registration_object_host().GetBindingCount());

  // Set up ServiceWorkerProviderContext for ServiceWorkerGlobalScope.
  const int kProviderId = 10;
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, nullptr, nullptr);

  // The passed references should be adopted and owned by the provider context.
  provider_context->SetRegistrationForServiceWorkerGlobalScope(
      std::move(registration_info), thread_safe_sender());
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Destruction of the provider context should release references to the
  // associated registration and its versions.
  provider_context = nullptr;
  ASSERT_EQ(3UL, ipc_sink()->message_count());
  EXPECT_EQ(static_cast<uint32_t>(
                ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(static_cast<uint32_t>(
                ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(static_cast<uint32_t>(
                ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
            ipc_sink()->GetMessageAt(2)->type());
  // ServiceWorkerRegistrationObjectHost Mojo connection got broken.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, remote_registration_object_host().GetBindingCount());
}

TEST_F(ServiceWorkerProviderContextTest, SetController) {
  const int kProviderId = 10;

  {
    // Assume that these objects are passed from the browser process and own
    // references to browser-side registration/worker representations.
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info =
        CreateServiceWorkerRegistrationObjectInfo();

    // (1) In the case there is no WebSWProviderClient but SWProviderContext for
    // the provider, the passed reference should be adopted and owned by the
    // provider context.
    mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
    mojom::ServiceWorkerContainerAssociatedRequest container_request =
        mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);
    auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
        kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
        std::move(container_request), nullptr /* host_ptr_info */,
        nullptr /* controller_info */, nullptr /* loader_factory_getter */);

    ipc_sink()->ClearMessages();
    auto info = mojom::ControllerServiceWorkerInfo::New();
    info->object_info = std::move(registration_info->active);
    container_ptr->SetController(std::move(info),
                                 std::vector<blink::mojom::WebFeature>(), true);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(0UL, ipc_sink()->message_count());

    // Destruction of the provider context should release references to the
    // the controller.
    provider_context = nullptr;
    ASSERT_EQ(1UL, ipc_sink()->message_count());
    EXPECT_EQ(static_cast<uint32_t>(
                  ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
              ipc_sink()->GetMessageAt(0)->type());
    ipc_sink()->ClearMessages();
  }

  {
    // Assume that these objects are passed from the browser process and own
    // references to browser-side registration/worker representations.
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info =
        CreateServiceWorkerRegistrationObjectInfo();

    // (2) In the case there are both SWProviderContext and SWProviderClient for
    // the provider, the passed reference should be adopted by the provider
    // context and then be transfered ownership to the provider client, after
    // that due to limitation of the mock implementation, the reference
    // immediately gets released.
    mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
    mojom::ServiceWorkerContainerHostAssociatedRequest host_request =
        mojo::MakeRequest(&host_ptr_info);

    mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
    mojom::ServiceWorkerContainerAssociatedRequest container_request =
        mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);
    auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
        kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
        std::move(container_request), std::move(host_ptr_info),
        nullptr /* controller_info */, nullptr /* loader_factory_getter */);
    auto provider_impl = std::make_unique<WebServiceWorkerProviderImpl>(
        thread_safe_sender(), provider_context.get());
    auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();
    provider_impl->SetClient(client.get());
    ASSERT_FALSE(client->was_set_controller_called());

    ipc_sink()->ClearMessages();
    auto info = mojom::ControllerServiceWorkerInfo::New();
    info->object_info = std::move(registration_info->active);
    container_ptr->SetController(std::move(info),
                                 std::vector<blink::mojom::WebFeature>(), true);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(client->was_set_controller_called());
    ASSERT_EQ(1UL, ipc_sink()->message_count());
    EXPECT_EQ(static_cast<uint32_t>(
                  ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
              ipc_sink()->GetMessageAt(0)->type());
  }
}

// Test that clearing the controller by sending a kInvalidServiceWorkerHandle
// results in the provider context having a null controller.
TEST_F(ServiceWorkerProviderContextTest, SetController_Null) {
  const int kProviderId = 10;

  mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
  mojom::ServiceWorkerContainerHostAssociatedRequest host_request =
      mojo::MakeRequest(&host_ptr_info);

  mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
  mojom::ServiceWorkerContainerAssociatedRequest container_request =
      mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
      std::move(container_request), std::move(host_ptr_info),
      nullptr /* controller_info */, nullptr /* loader_factory_getter */);
  auto provider_impl = std::make_unique<WebServiceWorkerProviderImpl>(
      thread_safe_sender(), provider_context.get());
  auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();
  provider_impl->SetClient(client.get());

  auto info = mojom::ControllerServiceWorkerInfo::New();
  info->object_info = blink::mojom::ServiceWorkerObjectInfo::New();
  container_ptr->SetController(std::move(info),
                               std::vector<blink::mojom::WebFeature>(), true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nullptr, provider_context->TakeController());
  EXPECT_TRUE(client->was_set_controller_called());
}

// S13nServiceWorker: Test that SetController correctly sets (or resets)
// the controller service worker for clients.
TEST_F(ServiceWorkerProviderContextTest, SetControllerServiceWorker) {
  EnableS13nServiceWorker();
  const int kProviderId = 10;

  // Assume that these objects are passed from the browser process and own
  // references to browser-side registration/worker representations.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info =
      CreateServiceWorkerRegistrationObjectInfo();

  // (1) Test if setting the controller via the CTOR works.
  FakeControllerServiceWorker fake_controller1;
  auto controller_info1 = mojom::ControllerServiceWorkerInfo::New();
  mojom::ControllerServiceWorkerPtr controller_ptr1;
  fake_controller1.Clone(mojo::MakeRequest(&controller_ptr1));
  controller_info1->object_info = std::move(registration_info->active);
  controller_info1->endpoint = controller_ptr1.PassInterface();

  mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
  mojom::ServiceWorkerContainerAssociatedRequest container_request =
      mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
      std::move(container_request), nullptr /* host_ptr_info */,
      std::move(controller_info1), loader_factory_getter_);
  ipc_sink()->ClearMessages();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Subresource loader factory must be available.
  auto* subresource_loader_factory1 =
      provider_context->GetSubresourceLoaderFactory();
  ASSERT_NE(nullptr, subresource_loader_factory1);

  // Performing a request should reach the controller.
  const GURL kURL1("https://www.example.com/foo.png");
  StartRequest(subresource_loader_factory1, kURL1);
  EXPECT_EQ(kURL1, fake_controller1.fetch_event_request().url);
  EXPECT_EQ(1, fake_controller1.fetch_event_count());

  // (2) Test if resetting the controller to a new one via SetController
  // works.
  FakeControllerServiceWorker fake_controller2;
  auto controller_info2 = mojom::ControllerServiceWorkerInfo::New();
  mojom::ControllerServiceWorkerPtr controller_ptr2;
  fake_controller2.Clone(mojo::MakeRequest(&controller_ptr2));
  controller_info2->object_info = std::move(registration_info->waiting);
  controller_info2->endpoint = controller_ptr2.PassInterface();
  container_ptr->SetController(std::move(controller_info2),
                               std::vector<blink::mojom::WebFeature>(), true);

  // The controller is reset. References to the old controller must be
  // released.
  ipc_sink()->ClearMessages();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(static_cast<uint32_t>(
                ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
            ipc_sink()->GetMessageAt(0)->type());

  // Subresource loader factory must be available, and should be the same
  // one as we got before.
  auto* subresource_loader_factory2 =
      provider_context->GetSubresourceLoaderFactory();
  ASSERT_NE(nullptr, subresource_loader_factory2);
  EXPECT_EQ(subresource_loader_factory1, subresource_loader_factory2);

  // Performing a request should reach the new controller.
  const GURL kURL2("https://www.example.com/foo2.png");
  StartRequest(subresource_loader_factory2, kURL2);
  EXPECT_EQ(kURL2, fake_controller2.fetch_event_request().url);
  EXPECT_EQ(1, fake_controller2.fetch_event_count());
  // The request should not go to the previous controller.
  EXPECT_EQ(1, fake_controller1.fetch_event_count());

  // (3) Test if resetting the controller to nullptr works.
  auto controller_info3 = mojom::ControllerServiceWorkerInfo::New();
  controller_info3->object_info = blink::mojom::ServiceWorkerObjectInfo::New();
  container_ptr->SetController(std::move(controller_info3),
                               std::vector<blink::mojom::WebFeature>(), true);

  // The controller is reset. References to the old controller must be
  // released.
  ipc_sink()->ClearMessages();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(static_cast<uint32_t>(
                ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
            ipc_sink()->GetMessageAt(0)->type());

  // Subresource loader factory must not be available.
  EXPECT_EQ(nullptr, provider_context->GetSubresourceLoaderFactory());

  // Performing a request using the subresource factory obtained before
  // falls back to the network.
  const GURL kURL3("https://www.example.com/foo3.png");
  EXPECT_EQ(0UL, fake_loader_factory_.clients_count());
  StartRequest(subresource_loader_factory2, kURL3);
  EXPECT_EQ(kURL3, fake_loader_factory_.last_request_url());
  EXPECT_EQ(1UL, fake_loader_factory_.clients_count());

  // The request should not go to the previous controllers.
  EXPECT_EQ(1, fake_controller1.fetch_event_count());
  EXPECT_EQ(1, fake_controller2.fetch_event_count());

  // (4) Test if resetting the controller to yet another one via SetController
  // works.
  FakeControllerServiceWorker fake_controller4;
  auto controller_info4 = mojom::ControllerServiceWorkerInfo::New();
  mojom::ControllerServiceWorkerPtr controller_ptr4;
  fake_controller4.Clone(mojo::MakeRequest(&controller_ptr4));
  controller_info4->object_info = blink::mojom::ServiceWorkerObjectInfo::New();
  controller_info4->object_info->handle_id = 103;
  controller_info4->object_info->version_id = 203;
  controller_info4->endpoint = controller_ptr4.PassInterface();
  container_ptr->SetController(std::move(controller_info4),
                               std::vector<blink::mojom::WebFeature>(), true);
  ipc_sink()->ClearMessages();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Subresource loader factory must be available.
  auto* subresource_loader_factory4 =
      provider_context->GetSubresourceLoaderFactory();
  ASSERT_NE(nullptr, subresource_loader_factory4);

  // Performing a request should reach the new controller.
  const GURL kURL4("https://www.example.com/foo4.png");
  StartRequest(subresource_loader_factory4, kURL4);
  EXPECT_EQ(kURL4, fake_controller4.fetch_event_request().url);
  EXPECT_EQ(1, fake_controller4.fetch_event_count());

  // The request should not go to the previous controllers.
  EXPECT_EQ(1, fake_controller1.fetch_event_count());
  EXPECT_EQ(1, fake_controller2.fetch_event_count());
  // The request should not go to the network.
  EXPECT_EQ(1UL, fake_loader_factory_.clients_count());
}

TEST_F(ServiceWorkerProviderContextTest, PostMessageToClient) {
  const int kProviderId = 10;

  // Assume that these objects are passed from the browser process and own
  // references to browser-side registration/worker representations.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info =
      CreateServiceWorkerRegistrationObjectInfo();

  mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
  mojom::ServiceWorkerContainerHostAssociatedRequest host_request =
      mojo::MakeRequest(&host_ptr_info);

  mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
  mojom::ServiceWorkerContainerAssociatedRequest container_request =
      mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
      std::move(container_request), std::move(host_ptr_info),
      nullptr /* controller_info */, nullptr /* loader_factory_getter */);
  auto provider_impl = std::make_unique<WebServiceWorkerProviderImpl>(
      thread_safe_sender(), provider_context.get());
  auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();
  provider_impl->SetClient(client.get());
  ASSERT_FALSE(client->was_dispatch_message_event_called());

  ipc_sink()->ClearMessages();
  container_ptr->PostMessageToClient(
      std::move(registration_info->active), base::string16(),
      std::vector<mojo::ScopedMessagePipeHandle>());
  base::RunLoop().RunUntilIdle();

  // The passed reference should be owned by the provider client (but the
  // reference is immediately released by the mock provider client).
  EXPECT_TRUE(client->was_dispatch_message_event_called());
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(static_cast<uint32_t>(
                ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
            ipc_sink()->GetMessageAt(0)->type());
}

TEST_F(ServiceWorkerProviderContextTest, CountFeature) {
  const int kProviderId = 10;

  mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
  mojom::ServiceWorkerContainerHostAssociatedRequest host_request =
      mojo::MakeRequest(&host_ptr_info);

  mojom::ServiceWorkerContainerAssociatedPtr container_ptr;
  mojom::ServiceWorkerContainerAssociatedRequest container_request =
      mojo::MakeRequestAssociatedWithDedicatedPipe(&container_ptr);
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow,
      std::move(container_request), std::move(host_ptr_info),
      nullptr /* controller_info */, nullptr /* loader_factory_getter */);
  auto provider_impl = std::make_unique<WebServiceWorkerProviderImpl>(
      thread_safe_sender(), provider_context.get());
  auto client = std::make_unique<MockWebServiceWorkerProviderClientImpl>();

  container_ptr->CountFeature(blink::mojom::WebFeature::kWorkerStart);
  provider_impl->SetClient(client.get());
  base::RunLoop().RunUntilIdle();

  // Calling CountFeature() before client is set will save the feature usage in
  // the set, and once SetClient() is called it gets propagated to the client.
  ASSERT_EQ(1UL, client->used_features().size());
  ASSERT_EQ(blink::mojom::WebFeature::kWorkerStart,
            *(client->used_features().begin()));

  container_ptr->CountFeature(blink::mojom::WebFeature::kWindowEvent);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2UL, client->used_features().size());
  ASSERT_EQ(blink::mojom::WebFeature::kWindowEvent,
            *(++(client->used_features().begin())));
}

TEST_F(ServiceWorkerProviderContextTest,
       SetAndTakeRegistrationForServiceWorkerGlobalScope) {
  // Set up ServiceWorkerProviderContext for ServiceWorkerGlobalScope.
  const int kProviderId = 10;
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, nullptr, nullptr);

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      CreateServiceWorkerRegistrationObjectInfo();
  int64_t registration_id = info->registration_id;
  ASSERT_EQ(1, remote_registration_object_host().GetBindingCount());

  provider_context->SetRegistrationForServiceWorkerGlobalScope(
      std::move(info), thread_safe_sender());
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Should return a newly created registration object which adopts all
  // references to the remote instances of ServiceWorkerRegistrationObjectHost
  // and ServiceWorkerHandle in the browser process.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration =
      provider_context->TakeRegistrationForServiceWorkerGlobalScope(
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  EXPECT_TRUE(registration);
  EXPECT_EQ(registration_id, registration->RegistrationId());
  EXPECT_EQ(1, remote_registration_object_host().GetBindingCount());
  ASSERT_EQ(0UL, ipc_sink()->message_count());

  ipc_sink()->ClearMessages();
  // The registration dtor decrements the refcounts.
  registration = nullptr;
  ASSERT_EQ(3UL, ipc_sink()->message_count());
  EXPECT_EQ(static_cast<uint32_t>(
                ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(static_cast<uint32_t>(
                ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(static_cast<uint32_t>(
                ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
            ipc_sink()->GetMessageAt(2)->type());
  // The Mojo connection has been dropped.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, remote_registration_object_host().GetBindingCount());
}

TEST_F(ServiceWorkerProviderContextTest, GetOrAdoptRegistration) {
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration1;
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration2;
  int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
  // Set up ServiceWorkerProviderContext for ServiceWorkerGlobalScope.
  const int kProviderId = 10;
  auto provider_context = base::MakeRefCounted<ServiceWorkerProviderContext>(
      kProviderId, blink::mojom::ServiceWorkerProviderType::kForWindow, nullptr,
      nullptr, nullptr /* controller_info */,
      nullptr /* loader_factory_getter */);

  {
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
        CreateServiceWorkerRegistrationObjectInfo();
    registration_id = info->registration_id;
    // The 1st ServiceWorkerRegistrationObjectHost Mojo connection has been
    // added.
    ASSERT_EQ(1, remote_registration_object_host().GetBindingCount());

    ASSERT_FALSE(ContainsRegistration(provider_context.get(), registration_id));
    // Should return a registration object newly created with adopting the
    // refcounts.
    registration1 =
        provider_context->GetOrCreateRegistrationForServiceWorkerClient(
            std::move(info));
    EXPECT_TRUE(registration1);
    EXPECT_TRUE(ContainsRegistration(provider_context.get(), registration_id));
    EXPECT_EQ(registration_id, registration1->RegistrationId());
    EXPECT_EQ(1, remote_registration_object_host().GetBindingCount());
    EXPECT_EQ(0UL, ipc_sink()->message_count());
  }

  ipc_sink()->ClearMessages();

  {
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
        CreateServiceWorkerRegistrationObjectInfo();
    // The 2nd Mojo connection has been added.
    ASSERT_EQ(2, remote_registration_object_host().GetBindingCount());
    // Should return the same registration object without incrementing the
    // refcounts.
    registration2 =
        provider_context->GetOrCreateRegistrationForServiceWorkerClient(
            std::move(info));
    EXPECT_TRUE(registration2);
    EXPECT_EQ(registration1, registration2);
    // The 2nd Mojo connection has been dropped.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1, remote_registration_object_host().GetBindingCount());
    ASSERT_EQ(3UL, ipc_sink()->message_count());
    EXPECT_EQ(static_cast<uint32_t>(
                  ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
              ipc_sink()->GetMessageAt(0)->type());
    EXPECT_EQ(static_cast<uint32_t>(
                  ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
              ipc_sink()->GetMessageAt(1)->type());
    EXPECT_EQ(static_cast<uint32_t>(
                  ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
              ipc_sink()->GetMessageAt(2)->type());
  }

  ipc_sink()->ClearMessages();

  // The registration dtor decrements the refcounts.
  registration1 = nullptr;
  registration2 = nullptr;
  ASSERT_EQ(3UL, ipc_sink()->message_count());
  EXPECT_EQ(static_cast<uint32_t>(
                ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(static_cast<uint32_t>(
                ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(static_cast<uint32_t>(
                ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID),
            ipc_sink()->GetMessageAt(2)->type());
  // The 1st Mojo connection has been dropped.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ContainsRegistration(provider_context.get(), registration_id));
  EXPECT_EQ(0, remote_registration_object_host().GetBindingCount());
}

}  // namespace service_worker_provider_context_unittest
}  // namespace content
