// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_dispatcher.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/web_service_worker_impl.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_error_type.mojom.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_object.mojom.h"

namespace content {

namespace {

class MockServiceWorkerObjectHost
    : public blink::mojom::ServiceWorkerObjectHost {
 public:
  MockServiceWorkerObjectHost(int32_t handle_id, int64_t version_id)
      : handle_id_(handle_id), version_id_(version_id) {}
  ~MockServiceWorkerObjectHost() override = default;

  blink::mojom::ServiceWorkerObjectInfoPtr CreateObjectInfo() {
    auto info = blink::mojom::ServiceWorkerObjectInfo::New();
    info->handle_id = handle_id_;
    info->version_id = version_id_;
    bindings_.AddBinding(this, mojo::MakeRequest(&info->host_ptr_info));
    return info;
  }

  int GetBindingCount() const { return bindings_.size(); }

 private:
  // Implements blink::mojom::ServiceWorkerObjectHost.
  void PostMessage(::blink::TransferableMessage message,
                   const url::Origin& source_origin) override {
    NOTREACHED();
  }
  void TerminateForTesting(TerminateForTestingCallback callback) override {
    NOTREACHED();
  }

  int32_t handle_id_;
  int64_t version_id_;
  mojo::AssociatedBindingSet<blink::mojom::ServiceWorkerObjectHost> bindings_;
};

}  // namespace

class ServiceWorkerDispatcherTest : public testing::Test {
 public:
  ServiceWorkerDispatcherTest() {}

  void SetUp() override {
    dispatcher_ = std::make_unique<ServiceWorkerDispatcher>(
        nullptr /* thread_safe_sender */);
  }

  bool ContainsServiceWorker(int handle_id) {
    return ContainsKey(dispatcher_->service_workers_, handle_id);
  }

  ServiceWorkerDispatcher* dispatcher() { return dispatcher_.get(); }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<ServiceWorkerDispatcher> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcherTest);
};

TEST_F(ServiceWorkerDispatcherTest, GetServiceWorker) {
  scoped_refptr<WebServiceWorkerImpl> worker1;
  scoped_refptr<WebServiceWorkerImpl> worker2;
  auto mock_service_worker_object_host =
      std::make_unique<MockServiceWorkerObjectHost>(100 /* handle_id */,
                                                    200 /* version_id */);
  ASSERT_EQ(0, mock_service_worker_object_host->GetBindingCount());

  // Should return a worker object newly created with the 1st given |info|.
  {
    blink::mojom::ServiceWorkerObjectInfoPtr info =
        mock_service_worker_object_host->CreateObjectInfo();
    EXPECT_EQ(1, mock_service_worker_object_host->GetBindingCount());
    int handle_id = info->handle_id;
    worker1 = dispatcher()->GetOrCreateServiceWorker(std::move(info));
    EXPECT_TRUE(worker1);
    EXPECT_TRUE(ContainsServiceWorker(handle_id));
    // |worker1| is holding the 1st blink::mojom::ServiceWorkerObjectHost Mojo
    // connection to |mock_service_worker_object_host|.
    EXPECT_EQ(1, mock_service_worker_object_host->GetBindingCount());
  }

  // Should return the same worker object and release the 2nd given |info|.
  {
    blink::mojom::ServiceWorkerObjectInfoPtr info =
        mock_service_worker_object_host->CreateObjectInfo();
    EXPECT_EQ(2, mock_service_worker_object_host->GetBindingCount());
    worker2 = dispatcher()->GetOrCreateServiceWorker(std::move(info));
    EXPECT_EQ(worker1, worker2);
    base::RunLoop().RunUntilIdle();
    // The Mojo connection kept by |info| should be released.
    EXPECT_EQ(1, mock_service_worker_object_host->GetBindingCount());
  }

  // Should return nullptr when given nullptr.
  scoped_refptr<WebServiceWorkerImpl> invalid_worker =
      dispatcher()->GetOrCreateServiceWorker(nullptr);
  EXPECT_FALSE(invalid_worker);
}

}  // namespace content
