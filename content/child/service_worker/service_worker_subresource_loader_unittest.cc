// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_subresource_loader.h"

#include "base/run_loop.h"
#include "content/child/child_url_loader_factory_getter_impl.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_url_loader_client.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerSubresourceLoaderTest : public ::testing::Test {
 public:
  class MockServiceWorkerEventDispatcher
      : public mojom::ServiceWorkerEventDispatcher {
   public:
    MockServiceWorkerEventDispatcher() = default;
    ~MockServiceWorkerEventDispatcher() override = default;

    void AddBinding(mojom::ServiceWorkerEventDispatcherRequest request) {
      bindings_.AddBinding(this, std::move(request));
    }

    // mojom::ServiceWorkerEventDispatcher:
    void DispatchInstallEvent(
        mojom::ServiceWorkerInstallEventMethodsAssociatedPtrInfo client,
        DispatchInstallEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchActivateEvent(
        DispatchActivateEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchBackgroundFetchAbortEvent(
        const std::string& tag,
        DispatchBackgroundFetchAbortEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchBackgroundFetchClickEvent(
        const std::string& tag,
        mojom::BackgroundFetchState state,
        DispatchBackgroundFetchClickEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchBackgroundFetchFailEvent(
        const std::string& tag,
        const std::vector<BackgroundFetchSettledFetch>& fetches,
        DispatchBackgroundFetchFailEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchBackgroundFetchedEvent(
        const std::string& tag,
        const std::vector<BackgroundFetchSettledFetch>& fetches,
        DispatchBackgroundFetchedEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchExtendableMessageEvent(
        mojom::ExtendableMessageEventPtr event,
        DispatchExtendableMessageEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchFetchEvent(
        int fetch_event_id,
        const ServiceWorkerFetchRequest& request,
        mojom::FetchEventPreloadHandlePtr preload_handle,
        mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
        DispatchFetchEventCallback callback) override {
      fetch_event_count_++;
      fetch_event_request_ = request;
      std::move(callback).Run(SERVICE_WORKER_OK, base::Time());
    }
    void DispatchNotificationClickEvent(
        const std::string& notification_id,
        const PlatformNotificationData& notification_data,
        int action_index,
        const base::Optional<base::string16>& reply,
        DispatchNotificationClickEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchNotificationCloseEvent(
        const std::string& notification_id,
        const PlatformNotificationData& notification_data,
        DispatchNotificationCloseEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchPushEvent(const PushEventPayload& payload,
                           DispatchPushEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchSyncEvent(
        const std::string& tag,
        blink::mojom::BackgroundSyncEventLastChance last_chance,
        DispatchSyncEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchAbortPaymentEvent(
        int payment_request_id,
        payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
        DispatchAbortPaymentEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchCanMakePaymentEvent(
        int payment_request_id,
        payments::mojom::CanMakePaymentEventDataPtr event_data,
        payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
        DispatchCanMakePaymentEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void DispatchPaymentRequestEvent(
        int payment_request_id,
        payments::mojom::PaymentRequestEventDataPtr event_data,
        payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
        DispatchPaymentRequestEventCallback callback) override {
      ASSERT_TRUE(false);
    }
    void Ping(PingCallback callback) override { ASSERT_TRUE(false); }

    int fetch_event_count() const { return fetch_event_count_; }
    const ServiceWorkerFetchRequest& fetch_event_request() const {
      return fetch_event_request_;
    }

   private:
    int fetch_event_count_ = 0;
    ServiceWorkerFetchRequest fetch_event_request_;
    mojo::BindingSet<mojom::ServiceWorkerEventDispatcher> bindings_;
    DISALLOW_COPY_AND_ASSIGN(MockServiceWorkerEventDispatcher);
  };

  ServiceWorkerSubresourceLoaderTest() = default;
  ~ServiceWorkerSubresourceLoaderTest() override = default;

  void TestRequest(const GURL& url, const std::string& method) {
    ResourceRequest request;
    request.url = url;
    request.method = method;

    auto loader_factory_getter =
        base::MakeRefCounted<ChildURLLoaderFactoryGetterImpl>();

    mojom::URLLoaderPtr url_loader;
    TestURLLoaderClient url_loader_client;

    EXPECT_EQ(0, event_dispatcher_.fetch_event_count());

    mojom::ServiceWorkerEventDispatcherPtr event_dispatcher_ptr;
    event_dispatcher_.AddBinding(mojo::MakeRequest(&event_dispatcher_ptr));
    auto shared_event_dispatcher = base::MakeRefCounted<
        base::RefCountedData<mojom::ServiceWorkerEventDispatcherPtr>>();
    shared_event_dispatcher->data = std::move(event_dispatcher_ptr);

    ServiceWorkerSubresourceLoaderFactory loader_factory(
        std::move(shared_event_dispatcher), loader_factory_getter,
        request.url.GetOrigin());
    loader_factory.CreateLoaderAndStart(
        mojo::MakeRequest(&url_loader), 0, 0, mojom::kURLLoadOptionNone,
        request, url_loader_client.CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();

    EXPECT_EQ(1, event_dispatcher_.fetch_event_count());
    EXPECT_EQ(request.url, event_dispatcher_.fetch_event_request().url);
    EXPECT_EQ(request.method, event_dispatcher_.fetch_event_request().method);
  }

  TestBrowserThreadBundle thread_bundle_;
  MockServiceWorkerEventDispatcher event_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSubresourceLoaderTest);
};

TEST_F(ServiceWorkerSubresourceLoaderTest, Basic) {
  TestRequest(GURL("https://www.example.com/foo.html"), "GET");
}

// TODO(kinuko): Add more tests.

}  // namespace content
