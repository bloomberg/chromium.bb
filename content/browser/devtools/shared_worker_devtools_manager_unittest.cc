// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_worker_devtools_manager.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/devtools/worker_devtools_agent_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/worker_storage_partition.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestDevToolsClientHost : public DevToolsAgentHostClient {
 public:
  TestDevToolsClientHost() {}
  ~TestDevToolsClientHost() override {}
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override {}
  void AgentHostClosed(DevToolsAgentHost* agent_host) override {}

  void InspectAgentHost(DevToolsAgentHost* agent_host) {
    if (agent_host_.get())
      agent_host_->DetachClient(this);
    agent_host_ = agent_host;
    if (agent_host_.get())
      agent_host_->AttachClient(this);
  }
 private:
  scoped_refptr<DevToolsAgentHost> agent_host_;
  DISALLOW_COPY_AND_ASSIGN(TestDevToolsClientHost);
};
}  // namespace

class SharedWorkerDevToolsManagerTest : public testing::Test {
 public:
  typedef SharedWorkerDevToolsAgentHost::WorkerState WorkerState;

  SharedWorkerDevToolsManagerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()),
        partition_(new WorkerStoragePartition(
            BrowserContext::GetDefaultStoragePartition(browser_context_.get())
                ->GetURLRequestContext(),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr)),
        partition_id_(*partition_.get()) {}

 protected:
  void SetUp() override {
    manager_ = SharedWorkerDevToolsManager::GetInstance();
  }
  void TearDown() override {
    SharedWorkerDevToolsManager::GetInstance()->ResetForTesting();
  }

  void CheckWorkerState(int worker_process_id,
                        int worker_route_id,
                        WorkerState state) {
    SharedWorkerDevToolsAgentHost* host =
        GetDevToolsAgentHostForWorker(worker_process_id, worker_route_id);
    EXPECT_TRUE(!!host);
    EXPECT_EQ(state, host->state_);
  }

  void CheckWorkerNotExist(int worker_process_id, int worker_route_id) {
    EXPECT_TRUE(
        !GetDevToolsAgentHostForWorker(worker_process_id, worker_route_id));
  }

  void CheckWorkerCount(size_t size) {
    EXPECT_EQ(size, manager_->workers_.size());
  }

  SharedWorkerDevToolsAgentHost* GetDevToolsAgentHostForWorker(
      int worker_process_id,
      int worker_route_id) {
    const SharedWorkerDevToolsManager::WorkerId id(worker_process_id,
                                                   worker_route_id);
    auto it = manager_->workers_.find(id);
    return it == manager_->workers_.end() ? nullptr : it->second;
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<WorkerStoragePartition> partition_;
  const WorkerStoragePartitionId partition_id_;
  SharedWorkerDevToolsManager* manager_;
};

TEST_F(SharedWorkerDevToolsManagerTest, BasicTest) {
  scoped_refptr<DevToolsAgentHostImpl> agent_host;

  SharedWorkerInstance instance1(
      GURL("http://example.com/w.js"), std::string(),
      url::Origin::Create(GURL("http://example.com/")), std::string(),
      blink::kWebContentSecurityPolicyTypeReport, blink::kWebAddressSpacePublic,
      browser_context_->GetResourceContext(), partition_id_,
      blink::mojom::SharedWorkerCreationContextType::kNonsecure,
      base::UnguessableToken::Create());

  agent_host = GetDevToolsAgentHostForWorker(1, 1);
  EXPECT_FALSE(agent_host.get());

  // Created -> Started -> Destroyed
  CheckWorkerNotExist(1, 1);
  manager_->WorkerCreated(1, 1, instance1);
  CheckWorkerState(1, 1, WorkerState::WORKER_UNINSPECTED);
  manager_->WorkerReadyForInspection(1, 1);
  CheckWorkerState(1, 1, WorkerState::WORKER_UNINSPECTED);
  manager_->WorkerDestroyed(1, 1);
  CheckWorkerNotExist(1, 1);

  // Created -> GetDevToolsAgentHost -> Started -> Destroyed
  CheckWorkerNotExist(1, 2);
  manager_->WorkerCreated(1, 2, instance1);
  CheckWorkerState(1, 2, WorkerState::WORKER_UNINSPECTED);
  agent_host = GetDevToolsAgentHostForWorker(1, 2);
  EXPECT_TRUE(agent_host.get());
  CheckWorkerState(1, 2, WorkerState::WORKER_UNINSPECTED);
  EXPECT_EQ(agent_host.get(), GetDevToolsAgentHostForWorker(1, 2));
  manager_->WorkerReadyForInspection(1, 2);
  CheckWorkerState(1, 2, WorkerState::WORKER_UNINSPECTED);
  manager_->WorkerDestroyed(1, 2);
  CheckWorkerState(1, 2, WorkerState::WORKER_TERMINATED);
  agent_host = nullptr;
  CheckWorkerNotExist(1, 2);

  // Created -> Started -> GetDevToolsAgentHost -> Destroyed
  CheckWorkerNotExist(1, 3);
  manager_->WorkerCreated(1, 3, instance1);
  CheckWorkerState(1, 3, WorkerState::WORKER_UNINSPECTED);
  manager_->WorkerReadyForInspection(1, 3);
  CheckWorkerState(1, 3, WorkerState::WORKER_UNINSPECTED);
  agent_host = GetDevToolsAgentHostForWorker(1, 3);
  EXPECT_TRUE(agent_host.get());
  CheckWorkerState(1, 3, WorkerState::WORKER_UNINSPECTED);
  manager_->WorkerDestroyed(1, 3);
  CheckWorkerState(1, 3, WorkerState::WORKER_TERMINATED);
  agent_host = nullptr;
  CheckWorkerNotExist(1, 3);

  // Created -> Destroyed
  CheckWorkerNotExist(1, 4);
  manager_->WorkerCreated(1, 4, instance1);
  CheckWorkerState(1, 4, WorkerState::WORKER_UNINSPECTED);
  manager_->WorkerDestroyed(1, 4);
  CheckWorkerNotExist(1, 4);

  // Created -> GetDevToolsAgentHost -> Destroyed
  CheckWorkerNotExist(1, 5);
  manager_->WorkerCreated(1, 5, instance1);
  CheckWorkerState(1, 5, WorkerState::WORKER_UNINSPECTED);
  agent_host = GetDevToolsAgentHostForWorker(1, 5);
  EXPECT_TRUE(agent_host.get());
  CheckWorkerState(1, 5, WorkerState::WORKER_UNINSPECTED);
  manager_->WorkerDestroyed(1, 5);
  CheckWorkerState(1, 5, WorkerState::WORKER_TERMINATED);
  agent_host = nullptr;
  CheckWorkerNotExist(1, 5);

  // Created -> GetDevToolsAgentHost -> Free agent_host -> Destroyed
  CheckWorkerNotExist(1, 6);
  manager_->WorkerCreated(1, 6, instance1);
  CheckWorkerState(1, 6, WorkerState::WORKER_UNINSPECTED);
  agent_host = GetDevToolsAgentHostForWorker(1, 6);
  EXPECT_TRUE(agent_host.get());
  CheckWorkerState(1, 6, WorkerState::WORKER_UNINSPECTED);
  agent_host = nullptr;
  manager_->WorkerDestroyed(1, 6);
  CheckWorkerNotExist(1, 6);
}

TEST_F(SharedWorkerDevToolsManagerTest, AttachTest) {
  scoped_refptr<DevToolsAgentHostImpl> agent_host1;
  scoped_refptr<DevToolsAgentHostImpl> agent_host2;

  SharedWorkerInstance instance1(
      GURL("http://example.com/w1.js"), std::string(),
      url::Origin::Create(GURL("http://example.com/")), std::string(),
      blink::kWebContentSecurityPolicyTypeReport, blink::kWebAddressSpacePublic,
      browser_context_->GetResourceContext(), partition_id_,
      blink::mojom::SharedWorkerCreationContextType::kNonsecure,
      base::UnguessableToken::Create());
  SharedWorkerInstance instance2(
      GURL("http://example.com/w2.js"), std::string(),
      url::Origin::Create(GURL("http://example.com/")), std::string(),
      blink::kWebContentSecurityPolicyTypeReport, blink::kWebAddressSpacePublic,
      browser_context_->GetResourceContext(), partition_id_,
      blink::mojom::SharedWorkerCreationContextType::kNonsecure,
      base::UnguessableToken::Create());

  // Created -> GetDevToolsAgentHost -> Register -> Started -> Destroyed
  std::unique_ptr<TestDevToolsClientHost> client_host1(
      new TestDevToolsClientHost());
  CheckWorkerNotExist(2, 1);
  manager_->WorkerCreated(2, 1, instance1);
  CheckWorkerState(2, 1, WorkerState::WORKER_UNINSPECTED);
  agent_host1 = GetDevToolsAgentHostForWorker(2, 1);
  EXPECT_TRUE(agent_host1.get());
  CheckWorkerState(2, 1, WorkerState::WORKER_UNINSPECTED);
  EXPECT_EQ(agent_host1.get(), GetDevToolsAgentHostForWorker(2, 1));
  client_host1->InspectAgentHost(agent_host1.get());
  CheckWorkerState(2, 1, WorkerState::WORKER_INSPECTED);
  manager_->WorkerReadyForInspection(2, 1);
  CheckWorkerState(2, 1, WorkerState::WORKER_INSPECTED);
  manager_->WorkerDestroyed(2, 1);
  CheckWorkerState(2, 1, WorkerState::WORKER_TERMINATED);
  EXPECT_EQ(agent_host1.get(), GetDevToolsAgentHostForWorker(2, 1));

  // Created -> Started -> GetDevToolsAgentHost -> Register -> Destroyed
  std::unique_ptr<TestDevToolsClientHost> client_host2(
      new TestDevToolsClientHost());
  manager_->WorkerCreated(2, 2, instance2);
  CheckWorkerState(2, 2, WorkerState::WORKER_UNINSPECTED);
  manager_->WorkerReadyForInspection(2, 2);
  CheckWorkerState(2, 2, WorkerState::WORKER_UNINSPECTED);
  agent_host2 = GetDevToolsAgentHostForWorker(2, 2);
  EXPECT_TRUE(agent_host2.get());
  EXPECT_NE(agent_host1.get(), agent_host2.get());
  EXPECT_EQ(agent_host2.get(), GetDevToolsAgentHostForWorker(2, 2));
  CheckWorkerState(2, 2, WorkerState::WORKER_UNINSPECTED);
  client_host2->InspectAgentHost(agent_host2.get());
  CheckWorkerState(2, 2, WorkerState::WORKER_INSPECTED);
  manager_->WorkerDestroyed(2, 2);
  CheckWorkerState(2, 2, WorkerState::WORKER_TERMINATED);
  EXPECT_EQ(agent_host2.get(), GetDevToolsAgentHostForWorker(2, 2));

  // Re-created -> Started -> ClientHostClosing -> Destroyed
  CheckWorkerState(2, 1, WorkerState::WORKER_TERMINATED);
  manager_->WorkerCreated(2, 3, instance1);
  CheckWorkerNotExist(2, 1);
  CheckWorkerState(2, 3, WorkerState::WORKER_PAUSED_FOR_REATTACH);
  EXPECT_EQ(agent_host1.get(), GetDevToolsAgentHostForWorker(2, 3));
  manager_->WorkerReadyForInspection(2, 3);
  CheckWorkerState(2, 3, WorkerState::WORKER_INSPECTED);
  client_host1->InspectAgentHost(nullptr);
  manager_->WorkerDestroyed(2, 3);
  CheckWorkerState(2, 3, WorkerState::WORKER_TERMINATED);
  agent_host1 = nullptr;
  CheckWorkerNotExist(2, 3);

  // Re-created -> Destroyed
  CheckWorkerState(2, 2, WorkerState::WORKER_TERMINATED);
  manager_->WorkerCreated(2, 4, instance2);
  CheckWorkerNotExist(2, 2);
  CheckWorkerState(2, 4, WorkerState::WORKER_PAUSED_FOR_REATTACH);
  EXPECT_EQ(agent_host2.get(), GetDevToolsAgentHostForWorker(2, 4));
  manager_->WorkerDestroyed(2, 4);
  CheckWorkerNotExist(2, 2);
  CheckWorkerState(2, 4, WorkerState::WORKER_TERMINATED);

  // Re-created -> ClientHostClosing -> Destroyed
  manager_->WorkerCreated(2, 5, instance2);
  CheckWorkerNotExist(2, 2);
  CheckWorkerState(2, 5, WorkerState::WORKER_PAUSED_FOR_REATTACH);
  EXPECT_EQ(agent_host2.get(), GetDevToolsAgentHostForWorker(2, 5));
  client_host2->InspectAgentHost(nullptr);
  CheckWorkerCount(1);
  agent_host2 = nullptr;
  CheckWorkerCount(1);
  manager_->WorkerDestroyed(2, 5);
  CheckWorkerCount(0);
}

TEST_F(SharedWorkerDevToolsManagerTest, ReattachTest) {
  SharedWorkerInstance instance(
      GURL("http://example.com/w3.js"), std::string(),
      url::Origin::Create(GURL("http://example.com/")), std::string(),
      blink::kWebContentSecurityPolicyTypeReport, blink::kWebAddressSpacePublic,
      browser_context_->GetResourceContext(), partition_id_,
      blink::mojom::SharedWorkerCreationContextType::kNonsecure,
      base::UnguessableToken::Create());
  std::unique_ptr<TestDevToolsClientHost> client_host(
      new TestDevToolsClientHost());
  // Created -> GetDevToolsAgentHost -> Register -> Destroyed
  manager_->WorkerCreated(3, 1, instance);
  CheckWorkerState(3, 1, WorkerState::WORKER_UNINSPECTED);
  scoped_refptr<DevToolsAgentHost> agent_host(
      GetDevToolsAgentHostForWorker(3, 1));
  EXPECT_TRUE(agent_host.get());
  CheckWorkerState(3, 1, WorkerState::WORKER_UNINSPECTED);
  client_host->InspectAgentHost(agent_host.get());
  CheckWorkerState(3, 1, WorkerState::WORKER_INSPECTED);
  manager_->WorkerDestroyed(3, 1);
  CheckWorkerState(3, 1, WorkerState::WORKER_TERMINATED);
  // ClientHostClosing -> Re-created -> release agent_host -> Destroyed
  client_host->InspectAgentHost(nullptr);
  CheckWorkerState(3, 1, WorkerState::WORKER_TERMINATED);
  manager_->WorkerCreated(3, 2, instance);
  CheckWorkerState(3, 2, WorkerState::WORKER_UNINSPECTED);
  agent_host = nullptr;
  CheckWorkerState(3, 2, WorkerState::WORKER_UNINSPECTED);
  manager_->WorkerDestroyed(3, 2);
  CheckWorkerNotExist(3, 2);
  CheckWorkerCount(0);
}

TEST_F(SharedWorkerDevToolsManagerTest, PauseOnStartTest) {
  SharedWorkerInstance instance(
      GURL("http://example.com/w3.js"), std::string(),
      url::Origin::Create(GURL("http://example.com/")), std::string(),
      blink::kWebContentSecurityPolicyTypeReport, blink::kWebAddressSpacePublic,
      browser_context_->GetResourceContext(), partition_id_,
      blink::mojom::SharedWorkerCreationContextType::kNonsecure,
      base::UnguessableToken::Create());
  std::unique_ptr<TestDevToolsClientHost> client_host(
      new TestDevToolsClientHost());
  manager_->WorkerCreated(3, 1, instance);
  CheckWorkerState(3, 1, WorkerState::WORKER_UNINSPECTED);
  scoped_refptr<SharedWorkerDevToolsAgentHost> agent_host(
      GetDevToolsAgentHostForWorker(3, 1));
  EXPECT_TRUE(agent_host.get());
  CheckWorkerState(3, 1, WorkerState::WORKER_UNINSPECTED);
  agent_host->PauseForDebugOnStart();
  CheckWorkerState(3, 1, WorkerState::WORKER_PAUSED_FOR_DEBUG_ON_START);
  EXPECT_FALSE(agent_host->IsReadyForInspection());
  manager_->WorkerReadyForInspection(3, 1);
  CheckWorkerState(3, 1, WorkerState::WORKER_READY_FOR_DEBUG_ON_START);
  client_host->InspectAgentHost(agent_host.get());
  CheckWorkerState(3, 1, WorkerState::WORKER_INSPECTED);
  client_host->InspectAgentHost(nullptr);
  agent_host = nullptr;
  CheckWorkerState(3, 1, WorkerState::WORKER_UNINSPECTED);
  manager_->WorkerDestroyed(3, 1);
  CheckWorkerNotExist(3, 1);
  CheckWorkerCount(0);
}

}  // namespace content
