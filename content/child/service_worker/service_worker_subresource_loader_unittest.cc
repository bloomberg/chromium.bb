// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_subresource_loader.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/child/child_url_loader_factory_getter_impl.h"
#include "content/child/service_worker/controller_service_worker_connector.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_url_loader_client.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class FakeControllerServiceWorker : public mojom::ControllerServiceWorker {
 public:
  FakeControllerServiceWorker() = default;
  ~FakeControllerServiceWorker() override = default;

  void AddBinding(mojom::ControllerServiceWorkerRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  void CloseAllBindings() { bindings_.CloseAllBindings(); }

  // mojom::ControllerServiceWorker:
  void DispatchFetchEvent(
      const ServiceWorkerFetchRequest& request,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback) override {
    fetch_event_count_++;
    fetch_event_request_ = request;
    std::move(callback).Run(SERVICE_WORKER_OK, base::Time());
  }

  int fetch_event_count() const { return fetch_event_count_; }
  const ServiceWorkerFetchRequest& fetch_event_request() const {
    return fetch_event_request_;
  }

 private:
  int fetch_event_count_ = 0;
  ServiceWorkerFetchRequest fetch_event_request_;
  mojo::BindingSet<mojom::ControllerServiceWorker> bindings_;
  DISALLOW_COPY_AND_ASSIGN(FakeControllerServiceWorker);
};

class FakeServiceWorkerContainerHost
    : public mojom::ServiceWorkerContainerHost {
 public:
  explicit FakeServiceWorkerContainerHost(
      FakeControllerServiceWorker* fake_controller)
      : fake_controller_(fake_controller) {}

  ~FakeServiceWorkerContainerHost() override = default;

  int get_controller_service_worker_count() const {
    return get_controller_service_worker_count_;
  }

 private:
  // Implements mojom::ServiceWorkerContainerHost.
  void Register(const GURL& script_url,
                blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
                RegisterCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistration(const GURL& client_url,
                       GetRegistrationCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistrations(GetRegistrationsCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistrationForReady(
      GetRegistrationForReadyCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetControllerServiceWorker(
      mojom::ControllerServiceWorkerRequest request) override {
    get_controller_service_worker_count_++;
    fake_controller_->AddBinding(std::move(request));
  }

  int get_controller_service_worker_count_ = 0;
  FakeControllerServiceWorker* fake_controller_;
  DISALLOW_COPY_AND_ASSIGN(FakeServiceWorkerContainerHost);
};

}  // namespace

class ServiceWorkerSubresourceLoaderTest : public ::testing::Test {
 public:
  ServiceWorkerSubresourceLoaderTest()
      : fake_container_host_(&fake_controller_) {}
  ~ServiceWorkerSubresourceLoaderTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kNetworkService);

    loader_factory_getter_ =
        base::MakeRefCounted<ChildURLLoaderFactoryGetterImpl>();
    controller_connector_ =
        base::MakeRefCounted<ControllerServiceWorkerConnector>(
            &fake_container_host_);
  }

  void TearDown() override {
    controller_connector_->OnContainerHostConnectionClosed();
  }

  void TestRequest(const GURL& url, const std::string& method) {
    ResourceRequest request;
    request.url = url;
    request.method = method;

    mojom::URLLoaderPtr url_loader;
    TestURLLoaderClient url_loader_client;

    ServiceWorkerSubresourceLoaderFactory loader_factory(
        controller_connector_, loader_factory_getter_, request.url.GetOrigin(),
        base::MakeRefCounted<
            base::RefCountedData<storage::mojom::BlobRegistryPtr>>());
    loader_factory.CreateLoaderAndStart(
        mojo::MakeRequest(&url_loader), 0, 0, mojom::kURLLoadOptionNone,
        request, url_loader_client.CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<ChildURLLoaderFactoryGetter> loader_factory_getter_;

  FakeServiceWorkerContainerHost fake_container_host_;
  FakeControllerServiceWorker fake_controller_;
  scoped_refptr<ControllerServiceWorkerConnector> controller_connector_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSubresourceLoaderTest);
};

TEST_F(ServiceWorkerSubresourceLoaderTest, Basic) {
  TestRequest(GURL("https://www.example.com/foo.html"), "GET");
  EXPECT_EQ(1, fake_controller_.fetch_event_count());
  EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
}

TEST_F(ServiceWorkerSubresourceLoaderTest, DropController) {
  TestRequest(GURL("https://www.example.com/foo.html"), "GET");
  EXPECT_EQ(1, fake_controller_.fetch_event_count());
  EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  TestRequest(GURL("https://www.example.com/foo2.html"), "GET");
  EXPECT_EQ(2, fake_controller_.fetch_event_count());
  EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());

  // Drop the connection to the ControllerServiceWorker.
  fake_controller_.CloseAllBindings();
  base::RunLoop().RunUntilIdle();

  // This should re-obtain the ControllerServiceWorker.
  TestRequest(GURL("https://www.example.com/foo3.html"), "GET");
  EXPECT_EQ(3, fake_controller_.fetch_event_count());
  EXPECT_EQ(2, fake_container_host_.get_controller_service_worker_count());
}

// TODO(kinuko): Add more tests.

}  // namespace content
