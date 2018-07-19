// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_host.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/shared_worker/mock_shared_worker.h"
#include "content/browser/shared_worker/shared_worker_connector_impl.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_storage_partition.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/message_port/message_port_channel.h"
#include "url/origin.h"

using blink::MessagePortChannel;

namespace content {

class SharedWorkerHostTest : public testing::Test {
 public:
  SharedWorkerHostTest()
      : mock_render_process_host_(&browser_context_),
        service_(&storage_partition_, nullptr /* service_worker_context */) {
    storage_partition_.set_network_context(&network_context_);
  }

  base::WeakPtr<SharedWorkerHost> CreateHost() {
    GURL url("http://www.example.com/w.js");
    std::string name("name");
    url::Origin origin = url::Origin::Create(url);
    std::string content_security_policy;
    blink::WebContentSecurityPolicyType content_security_policy_type = blink::
        WebContentSecurityPolicyType::kWebContentSecurityPolicyTypeReport;
    blink::mojom::IPAddressSpace creation_address_space =
        blink::mojom::IPAddressSpace::kPublic;
    blink::mojom::SharedWorkerCreationContextType creation_context_type =
        blink::mojom::SharedWorkerCreationContextType::kSecure;

    auto instance = std::make_unique<SharedWorkerInstance>(
        url, name, origin, content_security_policy,
        content_security_policy_type, creation_address_space,
        creation_context_type);
    auto host = std::make_unique<SharedWorkerHost>(
        &service_, std::move(instance), mock_render_process_host_.GetID());
    auto weak_host = host->AsWeakPtr();
    service_.worker_hosts_.insert(std::move(host));
    return weak_host;
  }

  void StartWorker(SharedWorkerHost* host,
                   mojom::SharedWorkerFactoryPtr factory) {
    host->Start(std::move(factory), nullptr /* service_worker_provider_info */,
                {} /* script_loader_factory_info */,
                nullptr /* factory_bundle */);
  }

  MessagePortChannel AddClient(SharedWorkerHost* host,
                               mojom::SharedWorkerClientPtr client) {
    mojo::MessagePipe message_pipe;
    MessagePortChannel local_port(std::move(message_pipe.handle0));
    MessagePortChannel remote_port(std::move(message_pipe.handle1));
    host->AddClient(std::move(client), host->process_id(),
                    22 /* dummy frame_id */, std::move(remote_port));
    return local_port;
  }

 protected:
  TestBrowserThreadBundle test_browser_thread_bundle_;
  TestStoragePartition storage_partition_;
  network::TestNetworkContext network_context_;
  TestBrowserContext browser_context_;
  MockRenderProcessHost mock_render_process_host_;

  SharedWorkerServiceImpl service_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHostTest);
};

TEST_F(SharedWorkerHostTest, Normal) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Start the worker.
  mojom::SharedWorkerFactoryPtr factory;
  MockSharedWorkerFactory factory_impl(mojo::MakeRequest(&factory));
  StartWorker(host.get(), std::move(factory));

  // Add the initiating client.
  MockSharedWorkerClient client;
  mojom::SharedWorkerClientPtr client_ptr;
  client.Bind(mojo::MakeRequest(&client_ptr));
  MessagePortChannel local_port = AddClient(host.get(), std::move(client_ptr));
  base::RunLoop().RunUntilIdle();

  // The factory should have gotten the CreateSharedWorker message.
  mojom::SharedWorkerHostPtr worker_host;
  mojom::SharedWorkerRequest worker_request;
  EXPECT_TRUE(factory_impl.CheckReceivedCreateSharedWorker(
      host->instance()->url(), host->instance()->name(),
      host->instance()->content_security_policy_type(), &worker_host,
      &worker_request));
  {
    MockSharedWorker worker(std::move(worker_request));
    base::RunLoop().RunUntilIdle();

    // The worker and client should have gotten initial messages.
    int connection_request_id;
    MessagePortChannel port;
    EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id, &port));
    EXPECT_TRUE(client.CheckReceivedOnCreated());
    // Simulate events the shared worker would send.
    worker_host->OnReadyForInspection();
    worker_host->OnScriptLoaded();
    worker_host->OnConnected(connection_request_id);
    base::RunLoop().RunUntilIdle();

    // The client should be connected.
    EXPECT_TRUE(
        client.CheckReceivedOnConnected({} /* expected_used_features */));

    // Close the client. The host should detect that there are no clients left
    // and ask the worker to terminate.
    client.Close();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(worker.CheckReceivedTerminate());

    // Simulate the worker terminating by breaking the Mojo connection when
    // |worker| goes out of scope.
  }
  base::RunLoop().RunUntilIdle();

  // The host should have self-destructed.
  EXPECT_FALSE(host);
}

TEST_F(SharedWorkerHostTest, DestructBeforeStarting) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Add a client.
  MockSharedWorkerClient client;
  mojom::SharedWorkerClientPtr client_ptr;
  client.Bind(mojo::MakeRequest(&client_ptr));
  MessagePortChannel local_port = AddClient(host.get(), std::move(client_ptr));
  base::RunLoop().RunUntilIdle();

  // Destroy the host before starting.
  service_.DestroyHost(host.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host);

  // The client should be told startup failed.
  EXPECT_TRUE(client.CheckReceivedOnScriptLoadFailed());
}

TEST_F(SharedWorkerHostTest, TerminateBeforeStarting) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Add a client.
  MockSharedWorkerClient client;
  mojom::SharedWorkerClientPtr client_ptr;
  client.Bind(mojo::MakeRequest(&client_ptr));
  MessagePortChannel local_port = AddClient(host.get(), std::move(client_ptr));
  base::RunLoop().RunUntilIdle();

  // Request to terminate the worker before starting.
  host->TerminateWorker();
  base::RunLoop().RunUntilIdle();

  // The client should be told startup failed.
  EXPECT_TRUE(client.CheckReceivedOnScriptLoadFailed());

  // The host should be destroyed.
  EXPECT_FALSE(host);
}

TEST_F(SharedWorkerHostTest, TerminateAfterStarting) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Create the factory.
  mojom::SharedWorkerFactoryPtr factory;
  MockSharedWorkerFactory factory_impl(mojo::MakeRequest(&factory));

  // Start the worker.
  host->Start(std::move(factory), nullptr /* service_worker_provider_info */,
              {} /* script_loader_factory_info */,
              nullptr /* factory_bundle */);

  // Add a client.
  MockSharedWorkerClient client;
  mojom::SharedWorkerClientPtr client_ptr;
  client.Bind(mojo::MakeRequest(&client_ptr));
  MessagePortChannel local_port = AddClient(host.get(), std::move(client_ptr));
  base::RunLoop().RunUntilIdle();

  {
    mojom::SharedWorkerHostPtr worker_host;
    mojom::SharedWorkerRequest worker_request;
    EXPECT_TRUE(factory_impl.CheckReceivedCreateSharedWorker(
        host->instance()->url(), host->instance()->name(),
        host->instance()->content_security_policy_type(), &worker_host,
        &worker_request));
    MockSharedWorker worker(std::move(worker_request));

    // Terminate after starting.
    host->TerminateWorker();
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(worker.CheckReceivedTerminate());
    // We simulate the worker terminating by breaking the Mojo connection when
    // it goes out of scope.
  }

  // Simulate the worker in the renderer terminating.
  base::RunLoop().RunUntilIdle();

  // The client should not have been told startup failed.
  EXPECT_FALSE(client.CheckReceivedOnScriptLoadFailed());
  // The host should be destroyed.
  EXPECT_FALSE(host);
}

TEST_F(SharedWorkerHostTest, OnContextClosed) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Start the worker.
  mojom::SharedWorkerFactoryPtr factory;
  MockSharedWorkerFactory factory_impl(mojo::MakeRequest(&factory));
  StartWorker(host.get(), std::move(factory));

  // Add a client.
  MockSharedWorkerClient client;
  mojom::SharedWorkerClientPtr client_ptr;
  client.Bind(mojo::MakeRequest(&client_ptr));
  MessagePortChannel local_port = AddClient(host.get(), std::move(client_ptr));
  base::RunLoop().RunUntilIdle();

  {
    mojom::SharedWorkerHostPtr worker_host;
    mojom::SharedWorkerRequest worker_request;
    EXPECT_TRUE(factory_impl.CheckReceivedCreateSharedWorker(
        host->instance()->url(), host->instance()->name(),
        host->instance()->content_security_policy_type(), &worker_host,
        &worker_request));
    MockSharedWorker worker(std::move(worker_request));

    // Simulate the worker calling OnContextClosed().
    worker_host->OnContextClosed();
    base::RunLoop().RunUntilIdle();

    // Close the client. The host should detect that there are no clients left
    // and terminate the worker.
    client.Close();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(worker.CheckReceivedTerminate());

    // Simulate the worker terminating by breaking the Mojo connection when
    // |worker| goes out of scope.
  }
  base::RunLoop().RunUntilIdle();

  // The host should have self-destructed.
  EXPECT_FALSE(host);
}

}  // namespace content
