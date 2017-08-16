// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_process_manager.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/service_worker/embedded_worker_settings.h"
#include "content/public/common/child_process_host.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerProcessManagerTest : public testing::Test {
 public:
  ServiceWorkerProcessManagerTest() {}

  void SetUp() override {
    browser_context_.reset(new TestBrowserContext);
    process_manager_.reset(
        new ServiceWorkerProcessManager(browser_context_.get()));
    pattern_ = GURL("http://www.example.com/");
    script_url_ = GURL("http://www.example.com/sw.js");
    render_process_host_factory_.reset(new MockRenderProcessHostFactory());
    RenderProcessHostImpl::set_render_process_host_factory(
        render_process_host_factory_.get());
  }

  void TearDown() override {
    process_manager_->Shutdown();
    process_manager_.reset();
    RenderProcessHostImpl::set_render_process_host_factory(nullptr);
    render_process_host_factory_.reset();
  }

  std::unique_ptr<MockRenderProcessHost> CreateRenderProcessHost() {
    return base::MakeUnique<MockRenderProcessHost>(browser_context_.get());
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<ServiceWorkerProcessManager> process_manager_;
  GURL pattern_;
  GURL script_url_;

 private:
  std::unique_ptr<MockRenderProcessHostFactory> render_process_host_factory_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProcessManagerTest);
};

TEST_F(ServiceWorkerProcessManagerTest, SortProcess) {
  // Process 1 has 2 refs, 2 has 3 refs and 3 has 1 ref.
  process_manager_->AddProcessReferenceToPattern(pattern_, 1);
  process_manager_->AddProcessReferenceToPattern(pattern_, 1);
  process_manager_->AddProcessReferenceToPattern(pattern_, 2);
  process_manager_->AddProcessReferenceToPattern(pattern_, 2);
  process_manager_->AddProcessReferenceToPattern(pattern_, 2);
  process_manager_->AddProcessReferenceToPattern(pattern_, 3);

  // Process 2 has the biggest # of references and it should be chosen.
  EXPECT_THAT(process_manager_->SortProcessesForPattern(pattern_),
              testing::ElementsAre(2, 1, 3));

  process_manager_->RemoveProcessReferenceFromPattern(pattern_, 1);
  process_manager_->RemoveProcessReferenceFromPattern(pattern_, 1);
  // Scores for each process: 2 : 3, 3 : 1, process 1 is removed.
  EXPECT_THAT(process_manager_->SortProcessesForPattern(pattern_),
              testing::ElementsAre(2, 3));
}

TEST_F(ServiceWorkerProcessManagerTest, FindAvailableProcess) {
  std::unique_ptr<MockRenderProcessHost> host1(CreateRenderProcessHost());
  std::unique_ptr<MockRenderProcessHost> host2(CreateRenderProcessHost());
  std::unique_ptr<MockRenderProcessHost> host3(CreateRenderProcessHost());

  // Process 1 has 2 refs, 2 has 3 refs and 3 has 1 ref.
  process_manager_->AddProcessReferenceToPattern(pattern_, host1->GetID());
  process_manager_->AddProcessReferenceToPattern(pattern_, host1->GetID());
  process_manager_->AddProcessReferenceToPattern(pattern_, host2->GetID());
  process_manager_->AddProcessReferenceToPattern(pattern_, host2->GetID());
  process_manager_->AddProcessReferenceToPattern(pattern_, host2->GetID());
  process_manager_->AddProcessReferenceToPattern(pattern_, host3->GetID());

  // When all processes are in foreground, process 2 that has the highest
  // refcount should be chosen.
  EXPECT_EQ(host2->GetID(), process_manager_->FindAvailableProcess(pattern_));

  // Backgrounded process 2 should be deprioritized.
  host2->set_is_process_backgrounded(true);
  EXPECT_EQ(host1->GetID(), process_manager_->FindAvailableProcess(pattern_));

  // When all processes are in background, process 2 that has the highest
  // refcount should be chosen.
  host1->set_is_process_backgrounded(true);
  host3->set_is_process_backgrounded(true);
  EXPECT_EQ(host2->GetID(), process_manager_->FindAvailableProcess(pattern_));

  // Process 3 should be chosen because it is the only foreground process.
  host3->set_is_process_backgrounded(false);
  EXPECT_EQ(host3->GetID(), process_manager_->FindAvailableProcess(pattern_));
}

TEST_F(ServiceWorkerProcessManagerTest,
       AllocateWorkerProcess_FindAvailableProcess) {
  const int kEmbeddedWorkerId1 = 100;
  const int kEmbeddedWorkerId2 = 200;
  const int kEmbeddedWorkerId3 = 300;
  GURL scope1("http://example.com/scope1");
  GURL scope2("http://example.com/scope2");

  // Set up mock renderer process hosts.
  std::unique_ptr<MockRenderProcessHost> host1(CreateRenderProcessHost());
  std::unique_ptr<MockRenderProcessHost> host2(CreateRenderProcessHost());
  process_manager_->AddProcessReferenceToPattern(scope1, host1->GetID());
  process_manager_->AddProcessReferenceToPattern(scope2, host2->GetID());
  ASSERT_EQ(0u, host1->GetKeepAliveRefCount());
  ASSERT_EQ(0u, host2->GetKeepAliveRefCount());

  std::map<int, ServiceWorkerProcessManager::ProcessInfo>& instance_info =
      process_manager_->instance_info_;

  // (1) Allocate a process to a worker.
  ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
  ServiceWorkerStatusCode status = process_manager_->AllocateWorkerProcess(
      kEmbeddedWorkerId1, scope1, script_url_,
      true /* can_use_existing_process */, &process_info);

  // An existing process should be allocated to the worker.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(host1->GetID(), process_info.process_id);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::EXISTING_READY_PROCESS,
            process_info.start_situation);
  EXPECT_EQ(1u, host1->GetKeepAliveRefCount());
  EXPECT_EQ(0u, host2->GetKeepAliveRefCount());
  EXPECT_EQ(1u, instance_info.size());
  std::map<int, ServiceWorkerProcessManager::ProcessInfo>::iterator found =
      instance_info.find(kEmbeddedWorkerId1);
  ASSERT_TRUE(found != instance_info.end());
  EXPECT_EQ(host1->GetID(), found->second.process_id);

  // (2) Allocate a process to another worker whose scope is the same with the
  // first worker.
  status = process_manager_->AllocateWorkerProcess(
      kEmbeddedWorkerId2, scope1, script_url_,
      true /* can_use_existing_process */, &process_info);

  // The same process should be allocated to the second worker.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(host1->GetID(), process_info.process_id);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::EXISTING_READY_PROCESS,
            process_info.start_situation);
  EXPECT_EQ(2u, host1->GetKeepAliveRefCount());
  EXPECT_EQ(0u, host2->GetKeepAliveRefCount());
  EXPECT_EQ(2u, instance_info.size());
  found = instance_info.find(kEmbeddedWorkerId2);
  ASSERT_TRUE(found != instance_info.end());
  EXPECT_EQ(host1->GetID(), found->second.process_id);

  // (3) Allocate a process to a third worker whose scope is different from
  // other workers.
  status = process_manager_->AllocateWorkerProcess(
      kEmbeddedWorkerId3, scope2, script_url_,
      true /* can_use_existing_process */, &process_info);

  // A different existing process should be allocated to the third worker.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(host2->GetID(), process_info.process_id);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::EXISTING_READY_PROCESS,
            process_info.start_situation);
  EXPECT_EQ(2u, host1->GetKeepAliveRefCount());
  EXPECT_EQ(1u, host2->GetKeepAliveRefCount());
  EXPECT_EQ(3u, instance_info.size());
  found = instance_info.find(kEmbeddedWorkerId3);
  ASSERT_TRUE(found != instance_info.end());
  EXPECT_EQ(host2->GetID(), found->second.process_id);

  // The instance map should be updated by process release.
  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId3);
  EXPECT_EQ(2u, host1->GetKeepAliveRefCount());
  EXPECT_EQ(0u, host2->GetKeepAliveRefCount());
  EXPECT_EQ(2u, instance_info.size());
  EXPECT_TRUE(base::ContainsKey(instance_info, kEmbeddedWorkerId1));
  EXPECT_TRUE(base::ContainsKey(instance_info, kEmbeddedWorkerId2));

  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId1);
  EXPECT_EQ(1u, host1->GetKeepAliveRefCount());
  EXPECT_EQ(0u, host2->GetKeepAliveRefCount());
  EXPECT_EQ(1u, instance_info.size());
  EXPECT_TRUE(base::ContainsKey(instance_info, kEmbeddedWorkerId2));

  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId2);
  EXPECT_EQ(0u, host1->GetKeepAliveRefCount());
  EXPECT_EQ(0u, host2->GetKeepAliveRefCount());
  EXPECT_TRUE(instance_info.empty());
}

TEST_F(ServiceWorkerProcessManagerTest,
       AllocateWorkerProcess_WithProcessReuse) {
  const int kEmbeddedWorkerId = 100;
  const GURL kSiteUrl = GURL("http://example.com");

  // Create a process that is hosting a frame with URL |pattern_|.
  std::unique_ptr<MockRenderProcessHost> host(CreateRenderProcessHost());
  host->Init();
  RenderProcessHostImpl::AddFrameWithSite(browser_context_.get(), host.get(),
                                          kSiteUrl);

  std::map<int, ServiceWorkerProcessManager::ProcessInfo>& instance_info =
      process_manager_->instance_info_;
  EXPECT_TRUE(instance_info.empty());

  // Allocate a process to a worker, when process reuse is authorized.
  ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
  ServiceWorkerStatusCode status = process_manager_->AllocateWorkerProcess(
      kEmbeddedWorkerId, pattern_, script_url_,
      true /* can_use_existing_process */, &process_info);

  // An existing process should be allocated to the worker.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(host->GetID(), process_info.process_id);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::EXISTING_UNREADY_PROCESS,
            process_info.start_situation);
  EXPECT_EQ(1u, host->GetKeepAliveRefCount());
  EXPECT_EQ(1u, instance_info.size());
  std::map<int, ServiceWorkerProcessManager::ProcessInfo>::iterator found =
      instance_info.find(kEmbeddedWorkerId);
  ASSERT_TRUE(found != instance_info.end());
  EXPECT_EQ(host->GetID(), found->second.process_id);

  // Release the process.
  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId);
  EXPECT_EQ(0u, host->GetKeepAliveRefCount());
  EXPECT_TRUE(instance_info.empty());

  RenderProcessHostImpl::RemoveFrameWithSite(browser_context_.get(), host.get(),
                                             kSiteUrl);
}

TEST_F(ServiceWorkerProcessManagerTest,
       AllocateWorkerProcess_WithoutProcessReuse) {
  const int kEmbeddedWorkerId = 100;
  const GURL kSiteUrl = GURL("http://example.com");

  // Create a process that is hosting a frame with URL |pattern_|.
  std::unique_ptr<MockRenderProcessHost> host(CreateRenderProcessHost());
  RenderProcessHostImpl::AddFrameWithSite(browser_context_.get(), host.get(),
                                          kSiteUrl);

  std::map<int, ServiceWorkerProcessManager::ProcessInfo>& instance_info =
      process_manager_->instance_info_;
  EXPECT_TRUE(instance_info.empty());

  // Allocate a process to a worker, when process reuse is disallowed.
  ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
  ServiceWorkerStatusCode status = process_manager_->AllocateWorkerProcess(
      kEmbeddedWorkerId, pattern_, script_url_,
      false /* can_use_existing_process */, &process_info);

  // A new process should be allocated to the worker.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_NE(host->GetID(), process_info.process_id);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::NEW_PROCESS,
            process_info.start_situation);
  EXPECT_EQ(0u, host->GetKeepAliveRefCount());
  EXPECT_EQ(1u, instance_info.size());
  std::map<int, ServiceWorkerProcessManager::ProcessInfo>::iterator found =
      instance_info.find(kEmbeddedWorkerId);
  ASSERT_TRUE(found != instance_info.end());
  EXPECT_NE(host->GetID(), found->second.process_id);

  // Release the process.
  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId);
  EXPECT_TRUE(instance_info.empty());

  RenderProcessHostImpl::RemoveFrameWithSite(browser_context_.get(), host.get(),
                                             kSiteUrl);
}

TEST_F(ServiceWorkerProcessManagerTest, AllocateWorkerProcess_InShutdown) {
  process_manager_->Shutdown();
  ASSERT_TRUE(process_manager_->IsShutdown());

  ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
  ServiceWorkerStatusCode status = process_manager_->AllocateWorkerProcess(
      1, pattern_, script_url_, true /* can_use_existing_process */,
      &process_info);

  // Allocating a process in shutdown should abort.
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT, status);
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, process_info.process_id);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::UNKNOWN,
            process_info.start_situation);
  EXPECT_TRUE(process_manager_->instance_info_.empty());
}

}  // namespace content
