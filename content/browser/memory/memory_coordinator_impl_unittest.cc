// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_coordinator_impl.h"

#include "base/memory/memory_coordinator_client_registry.h"
#include "base/memory/memory_coordinator_proxy.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "content/browser/memory/memory_monitor.h"
#include "content/browser/memory/memory_state_updater.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void RunUntilIdle() {
  base::RunLoop loop;
  loop.RunUntilIdle();
}

// A mock ChildMemoryCoordinator, for testing interaction between MC and CMC.
class MockChildMemoryCoordinator : public mojom::ChildMemoryCoordinator {
 public:
  MockChildMemoryCoordinator()
      : state_(mojom::MemoryState::NORMAL),
        on_state_change_calls_(0),
        purge_memory_calls_(0) {}

  ~MockChildMemoryCoordinator() override {}

  void OnStateChange(mojom::MemoryState state) override {
    state_ = state;
    ++on_state_change_calls_;
  }

  void PurgeMemory() override { ++purge_memory_calls_; }

  mojom::MemoryState state() const { return state_; }
  int on_state_change_calls() const { return on_state_change_calls_; }
  int purge_memory_calls() const { return purge_memory_calls_; }

 private:
  mojom::MemoryState state_;
  int on_state_change_calls_;
  int purge_memory_calls_;
};

// A mock MemoryCoordinatorClient, for testing interaction between MC and
// clients.
class MockMemoryCoordinatorClient : public base::MemoryCoordinatorClient {
 public:
  void OnMemoryStateChange(base::MemoryState state) override {
    did_state_changed_ = true;
    state_ = state;
  }

  bool did_state_changed() const { return did_state_changed_; }
  base::MemoryState state() const { return state_; }

 private:
  bool did_state_changed_ = false;
  base::MemoryState state_ = base::MemoryState::NORMAL;
};

class MockMemoryMonitor : public MemoryMonitor {
 public:
  MockMemoryMonitor() {}
  ~MockMemoryMonitor() override {}

  void SetFreeMemoryUntilCriticalMB(int free_memory) {
    free_memory_ = free_memory;
  }

  // MemoryMonitor implementation
  int GetFreeMemoryUntilCriticalMB() override { return free_memory_; }

 private:
  int free_memory_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MockMemoryMonitor);
};

class TestMemoryCoordinatorDelegate : public MemoryCoordinatorDelegate {
 public:
  TestMemoryCoordinatorDelegate() {}
  ~TestMemoryCoordinatorDelegate() override {}

  bool CanSuspendBackgroundedRenderer(int render_process_id) override {
    return true;
  }

  void DiscardTab() override { discard_tab_called_ = true; }

  bool discard_tab_called() const { return discard_tab_called_; }

 private:
  bool discard_tab_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryCoordinatorDelegate);
};

// A MemoryCoordinatorImpl that can be directly constructed.
class TestMemoryCoordinatorImpl : public MemoryCoordinatorImpl {
 public:
  // Mojo machinery for wrapping a mock ChildMemoryCoordinator.
  struct Child {
    Child(mojom::ChildMemoryCoordinatorPtr* cmc_ptr) : cmc_binding(&cmc) {
      cmc_binding.Bind(mojo::MakeRequest(cmc_ptr));
      RunUntilIdle();
    }

    MockChildMemoryCoordinator cmc;
    mojo::Binding<mojom::ChildMemoryCoordinator> cmc_binding;
  };

  TestMemoryCoordinatorImpl(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner)
      : MemoryCoordinatorImpl(task_runner,
                              base::MakeUnique<MockMemoryMonitor>()) {
    SetDelegateForTesting(base::MakeUnique<TestMemoryCoordinatorDelegate>());
  }

  ~TestMemoryCoordinatorImpl() override {}

  using MemoryCoordinatorImpl::OnConnectionError;
  using MemoryCoordinatorImpl::children;

  MockChildMemoryCoordinator* CreateChildMemoryCoordinator(
      int process_id) {
    mojom::ChildMemoryCoordinatorPtr cmc_ptr;
    children_.push_back(std::unique_ptr<Child>(new Child(&cmc_ptr)));
    AddChildForTesting(process_id, std::move(cmc_ptr));
    render_process_hosts_[process_id] =
        base::MakeUnique<MockRenderProcessHost>(&browser_context_);
    return &children_.back()->cmc;
  }

  RenderProcessHost* GetRenderProcessHost(int render_process_id) override {
    return GetMockRenderProcessHost(render_process_id);
  }

  MockRenderProcessHost* GetMockRenderProcessHost(int render_process_id) {
    auto iter = render_process_hosts_.find(render_process_id);
    if (iter == render_process_hosts_.end())
      return nullptr;
    return iter->second.get();
  }

  TestMemoryCoordinatorDelegate* GetDelegate() {
    return static_cast<TestMemoryCoordinatorDelegate*>(delegate());
  }

  // Wrapper of MemoryCoordinator::SetMemoryState that also calls RunUntilIdle.
  bool SetChildMemoryState(
      int render_process_id, MemoryState memory_state) {
    bool result = MemoryCoordinatorImpl::SetChildMemoryState(
        render_process_id, memory_state);
    RunUntilIdle();
    return result;
  }

  TestBrowserContext browser_context_;
  std::vector<std::unique_ptr<Child>> children_;
  std::map<int, std::unique_ptr<MockRenderProcessHost>> render_process_hosts_;
};

}  // namespace

class MemoryCoordinatorImplTest : public base::MultiProcessTest {
 public:
  using MemoryState = base::MemoryState;

  void SetUp() override {
    base::MultiProcessTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(features::kMemoryCoordinator);

    task_runner_ = new base::TestMockTimeTaskRunner();
    coordinator_.reset(new TestMemoryCoordinatorImpl(task_runner_));
  }

  MockMemoryMonitor* GetMockMemoryMonitor() {
    return static_cast<MockMemoryMonitor*>(coordinator_->memory_monitor());
  }

 protected:
  std::unique_ptr<TestMemoryCoordinatorImpl> coordinator_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::MessageLoop message_loop_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MemoryCoordinatorImplTest, ChildRemovedOnConnectionError) {
  coordinator_->CreateChildMemoryCoordinator(1);
  ASSERT_EQ(1u, coordinator_->children().size());
  coordinator_->OnConnectionError(1);
  EXPECT_EQ(0u, coordinator_->children().size());
}

TEST_F(MemoryCoordinatorImplTest, SetMemoryStateFailsInvalidState) {
  auto* cmc1 = coordinator_->CreateChildMemoryCoordinator(1);

  EXPECT_FALSE(
      coordinator_->SetChildMemoryState(1, MemoryState::UNKNOWN));
  EXPECT_EQ(0, cmc1->on_state_change_calls());
}

TEST_F(MemoryCoordinatorImplTest, SetMemoryStateFailsInvalidRenderer) {
  auto* cmc1 = coordinator_->CreateChildMemoryCoordinator(1);

  EXPECT_FALSE(
      coordinator_->SetChildMemoryState(2, MemoryState::THROTTLED));
  EXPECT_EQ(0, cmc1->on_state_change_calls());
}

TEST_F(MemoryCoordinatorImplTest, SetMemoryStateNotDeliveredNop) {
  auto* cmc1 = coordinator_->CreateChildMemoryCoordinator(1);

  EXPECT_FALSE(
      coordinator_->SetChildMemoryState(2, MemoryState::NORMAL));
  EXPECT_EQ(0, cmc1->on_state_change_calls());
}

TEST_F(MemoryCoordinatorImplTest, SetMemoryStateDelivered) {
  auto* cmc1 = coordinator_->CreateChildMemoryCoordinator(1);
  auto* cmc2 = coordinator_->CreateChildMemoryCoordinator(2);

  EXPECT_TRUE(
      coordinator_->SetChildMemoryState(1, MemoryState::THROTTLED));
  EXPECT_EQ(1, cmc1->on_state_change_calls());
  EXPECT_EQ(mojom::MemoryState::THROTTLED, cmc1->state());

  EXPECT_TRUE(
      coordinator_->SetChildMemoryState(2, MemoryState::SUSPENDED));
  EXPECT_EQ(1, cmc2->on_state_change_calls());
  // Child processes are considered as visible (foreground) by default,
  // and visible ones won't be suspended but throttled.
  EXPECT_EQ(mojom::MemoryState::THROTTLED, cmc2->state());
}

TEST_F(MemoryCoordinatorImplTest, PurgeMemoryChild) {
  auto* child = coordinator_->CreateChildMemoryCoordinator(1);
  EXPECT_EQ(0, child->purge_memory_calls());
  child->PurgeMemory();
  RunUntilIdle();
  EXPECT_EQ(1, child->purge_memory_calls());
}

TEST_F(MemoryCoordinatorImplTest, SetChildMemoryState) {
  auto* cmc = coordinator_->CreateChildMemoryCoordinator(1);
  auto iter = coordinator_->children().find(1);
  auto* render_process_host = coordinator_->GetMockRenderProcessHost(1);
  ASSERT_TRUE(iter != coordinator_->children().end());
  ASSERT_TRUE(render_process_host);

  // Foreground
  iter->second.is_visible = true;
  render_process_host->set_is_process_backgrounded(false);
  EXPECT_TRUE(coordinator_->SetChildMemoryState(1, MemoryState::NORMAL));
  EXPECT_EQ(mojom::MemoryState::NORMAL, cmc->state());
  EXPECT_TRUE(
      coordinator_->SetChildMemoryState(1, MemoryState::THROTTLED));
  EXPECT_EQ(mojom::MemoryState::THROTTLED, cmc->state());
  EXPECT_TRUE(
      coordinator_->SetChildMemoryState(1, MemoryState::SUSPENDED));
  EXPECT_EQ(mojom::MemoryState::THROTTLED, cmc->state());

  // Background
  iter->second.is_visible = false;
  render_process_host->set_is_process_backgrounded(true);
  EXPECT_TRUE(coordinator_->SetChildMemoryState(1, MemoryState::NORMAL));
#if defined(OS_ANDROID)
  EXPECT_EQ(mojom::MemoryState::THROTTLED, cmc->state());
#else
  EXPECT_EQ(mojom::MemoryState::NORMAL, cmc->state());
#endif
  EXPECT_TRUE(
      coordinator_->SetChildMemoryState(1, MemoryState::THROTTLED));
  EXPECT_EQ(mojom::MemoryState::THROTTLED, cmc->state());
  EXPECT_TRUE(
      coordinator_->SetChildMemoryState(1, MemoryState::SUSPENDED));
  EXPECT_EQ(mojom::MemoryState::SUSPENDED, cmc->state());

  // Background but there are workers
  render_process_host->IncrementServiceWorkerRefCount();
  EXPECT_TRUE(
      coordinator_->SetChildMemoryState(1, MemoryState::THROTTLED));
  EXPECT_EQ(mojom::MemoryState::THROTTLED, cmc->state());
  EXPECT_FALSE(
      coordinator_->SetChildMemoryState(1, MemoryState::SUSPENDED));
  EXPECT_EQ(mojom::MemoryState::THROTTLED, cmc->state());
  render_process_host->DecrementSharedWorkerRefCount();
}

TEST_F(MemoryCoordinatorImplTest, CalculateNextState) {
  auto* state_updater = coordinator_->state_updater_.get();
  state_updater->expected_renderer_size_ = 10;
  state_updater->new_renderers_until_throttled_ = 4;
  state_updater->new_renderers_until_suspended_ = 2;
  state_updater->new_renderers_back_to_normal_ = 5;
  state_updater->new_renderers_back_to_throttled_ = 3;
  DCHECK(state_updater->ValidateParameters());

  // The default state is NORMAL.
  EXPECT_EQ(base::MemoryState::NORMAL, coordinator_->GetCurrentMemoryState());
  EXPECT_EQ(base::MemoryState::NORMAL,
            base::MemoryCoordinatorProxy::GetInstance()->
                GetCurrentMemoryState());

  // Transitions from NORMAL
  coordinator_->current_state_ = base::MemoryState::NORMAL;
  EXPECT_EQ(base::MemoryState::NORMAL, coordinator_->GetCurrentMemoryState());
  EXPECT_EQ(base::MemoryState::NORMAL,
            base::MemoryCoordinatorProxy::GetInstance()->
                GetCurrentMemoryState());

  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(50);
  EXPECT_EQ(base::MemoryState::NORMAL, state_updater->CalculateNextState());
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(40);
  EXPECT_EQ(base::MemoryState::THROTTLED, state_updater->CalculateNextState());
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(20);
  EXPECT_EQ(base::MemoryState::SUSPENDED, state_updater->CalculateNextState());

  // Transitions from THROTTLED
  coordinator_->current_state_ = base::MemoryState::THROTTLED;
  EXPECT_EQ(base::MemoryState::THROTTLED,
            coordinator_->GetCurrentMemoryState());
  EXPECT_EQ(base::MemoryState::THROTTLED,
            base::MemoryCoordinatorProxy::GetInstance()->
                GetCurrentMemoryState());

  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(40);
  EXPECT_EQ(base::MemoryState::THROTTLED, state_updater->CalculateNextState());
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(50);
  EXPECT_EQ(base::MemoryState::NORMAL, state_updater->CalculateNextState());
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(20);
  EXPECT_EQ(base::MemoryState::SUSPENDED, state_updater->CalculateNextState());

  // Transitions from SUSPENDED
  coordinator_->current_state_ = base::MemoryState::SUSPENDED;
  // GetCurrentMemoryState() returns THROTTLED state for the browser process
  // when the global state is SUSPENDED.
  EXPECT_EQ(base::MemoryState::THROTTLED,
            coordinator_->GetCurrentMemoryState());
  EXPECT_EQ(base::MemoryState::THROTTLED,
            base::MemoryCoordinatorProxy::GetInstance()->
                GetCurrentMemoryState());

  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(20);
  EXPECT_EQ(base::MemoryState::SUSPENDED, state_updater->CalculateNextState());
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(30);
  EXPECT_EQ(base::MemoryState::THROTTLED, state_updater->CalculateNextState());
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(50);
  EXPECT_EQ(base::MemoryState::NORMAL, state_updater->CalculateNextState());
}

TEST_F(MemoryCoordinatorImplTest, UpdateState) {
  auto* state_updater = coordinator_->state_updater_.get();
  state_updater->expected_renderer_size_ = 10;
  state_updater->new_renderers_until_throttled_ = 4;
  state_updater->new_renderers_until_suspended_ = 2;
  state_updater->new_renderers_back_to_normal_ = 5;
  state_updater->new_renderers_back_to_throttled_ = 3;
  DCHECK(state_updater->ValidateParameters());

  {
    // Transition happens (NORMAL -> THROTTLED).
    MockMemoryCoordinatorClient client;
    base::MemoryCoordinatorClientRegistry::GetInstance()->Register(&client);
    coordinator_->current_state_ = base::MemoryState::NORMAL;
    GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(40);
    state_updater->UpdateState();
    RunUntilIdle();
    EXPECT_TRUE(client.did_state_changed());
    EXPECT_EQ(base::MemoryState::THROTTLED, client.state());
    base::MemoryCoordinatorClientRegistry::GetInstance()->Unregister(&client);
  }

  {
    // No transtion (NORMAL -> NORMAL). OnStateChange shouldn't be called.
    MockMemoryCoordinatorClient client;
    base::MemoryCoordinatorClientRegistry::GetInstance()->Register(&client);
    coordinator_->current_state_ = base::MemoryState::NORMAL;
    GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(50);
    state_updater->UpdateState();
    RunUntilIdle();
    EXPECT_FALSE(client.did_state_changed());
    EXPECT_EQ(base::MemoryState::NORMAL, client.state());
    base::MemoryCoordinatorClientRegistry::GetInstance()->Unregister(&client);
  }
}

TEST_F(MemoryCoordinatorImplTest, SetMemoryStateForTesting) {
  auto* state_updater = coordinator_->state_updater_.get();
  state_updater->expected_renderer_size_ = 10;
  state_updater->new_renderers_until_throttled_ = 4;
  state_updater->new_renderers_until_suspended_ = 2;
  state_updater->new_renderers_back_to_normal_ = 5;
  state_updater->new_renderers_back_to_throttled_ = 3;
  DCHECK(state_updater->ValidateParameters());

  MockMemoryCoordinatorClient client;
  base::MemoryCoordinatorClientRegistry::GetInstance()->Register(&client);
  EXPECT_EQ(base::MemoryState::NORMAL, coordinator_->GetCurrentMemoryState());
  EXPECT_EQ(base::MemoryState::NORMAL,
            base::MemoryCoordinatorProxy::GetInstance()->
                GetCurrentMemoryState());
  EXPECT_EQ(base::MemoryState::NORMAL, client.state());

  base::MemoryCoordinatorProxy::GetInstance()->SetCurrentMemoryStateForTesting(
      base::MemoryState::SUSPENDED);
  // GetCurrentMemoryState() returns THROTTLED state for the browser process
  // when the global state is SUSPENDED.
  EXPECT_EQ(base::MemoryState::THROTTLED,
            coordinator_->GetCurrentMemoryState());
  EXPECT_EQ(base::MemoryState::THROTTLED,
            base::MemoryCoordinatorProxy::GetInstance()->
                GetCurrentMemoryState());

  base::MemoryCoordinatorProxy::GetInstance()->SetCurrentMemoryStateForTesting(
      base::MemoryState::THROTTLED);
  EXPECT_EQ(base::MemoryState::THROTTLED,
            coordinator_->GetCurrentMemoryState());
  EXPECT_EQ(base::MemoryState::THROTTLED,
            base::MemoryCoordinatorProxy::GetInstance()->
            GetCurrentMemoryState());
  RunUntilIdle();
  EXPECT_TRUE(client.did_state_changed());
  EXPECT_EQ(base::MemoryState::THROTTLED, client.state());
  base::MemoryCoordinatorClientRegistry::GetInstance()->Unregister(&client);
}

TEST_F(MemoryCoordinatorImplTest, ForceSetGlobalState) {
  auto* state_updater = coordinator_->state_updater_.get();
  state_updater->expected_renderer_size_ = 10;
  state_updater->new_renderers_until_throttled_ = 4;
  state_updater->new_renderers_until_suspended_ = 2;
  state_updater->new_renderers_back_to_normal_ = 5;
  state_updater->new_renderers_back_to_throttled_ = 3;
  DCHECK(state_updater->ValidateParameters());
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(50);

  base::TimeDelta interval = base::TimeDelta::FromSeconds(5);
  base::TimeDelta minimum_transition = base::TimeDelta::FromSeconds(30);
  state_updater->monitoring_interval_ = interval;
  state_updater->minimum_transition_period_ = minimum_transition;

  // Starts updating states. The initial state should be NORMAL with above
  // configuration.
  coordinator_->Start();
  task_runner_->RunUntilIdle();
  EXPECT_EQ(base::MemoryState::NORMAL, coordinator_->GetGlobalMemoryState());

  base::TimeDelta force_set_duration = interval * 3;
  coordinator_->ForceSetGlobalState(base::MemoryState::SUSPENDED,
                                    force_set_duration);
  EXPECT_EQ(base::MemoryState::SUSPENDED, coordinator_->GetGlobalMemoryState());

  // The state should remain SUSPENDED even after some monitoring period are
  // passed.
  task_runner_->FastForwardBy(interval * 2);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(base::MemoryState::SUSPENDED, coordinator_->GetGlobalMemoryState());

  // The state should be updated after |force_set_duration| is passed.
  task_runner_->FastForwardBy(force_set_duration);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(base::MemoryState::NORMAL, coordinator_->GetGlobalMemoryState());

  // Also make sure that the state is updated based on free avaiable memory.
  // Since the global state has changed in the previous task, we have to wait
  // for |minimum_transition|.
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(40);
  task_runner_->FastForwardBy(minimum_transition);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(base::MemoryState::THROTTLED, coordinator_->GetGlobalMemoryState());
}

TEST_F(MemoryCoordinatorImplTest, DiscardTab) {
  coordinator_->DiscardTab();
  EXPECT_TRUE(coordinator_->GetDelegate()->discard_tab_called());
}

#if defined(OS_ANDROID)
// TODO(jcivelli): Broken on Android. http://crbug.com/678665
#define MAYBE_GetStateForProcess DISABLED_GetStateForProcess
#else
#define MAYBE_GetStateForProcess GetStateForProcess
#endif
TEST_F(MemoryCoordinatorImplTest, MAYBE_GetStateForProcess) {
  EXPECT_EQ(base::MemoryState::UNKNOWN,
            coordinator_->GetStateForProcess(base::kNullProcessHandle));
  EXPECT_EQ(base::MemoryState::NORMAL,
            coordinator_->GetStateForProcess(base::GetCurrentProcessHandle()));

  coordinator_->CreateChildMemoryCoordinator(1);
  coordinator_->CreateChildMemoryCoordinator(2);
  base::Process process1 = SpawnChild("process1");
  base::Process process2 = SpawnChild("process2");
  coordinator_->GetMockRenderProcessHost(1)->SetProcessHandle(
      base::MakeUnique<base::ProcessHandle>(process1.Handle()));
  coordinator_->GetMockRenderProcessHost(2)->SetProcessHandle(
      base::MakeUnique<base::ProcessHandle>(process2.Handle()));

  EXPECT_EQ(base::MemoryState::NORMAL,
            coordinator_->GetStateForProcess(process1.Handle()));
  EXPECT_EQ(base::MemoryState::NORMAL,
            coordinator_->GetStateForProcess(process2.Handle()));

  EXPECT_TRUE(
      coordinator_->SetChildMemoryState(1, MemoryState::THROTTLED));
  EXPECT_EQ(base::MemoryState::THROTTLED,
            coordinator_->GetStateForProcess(process1.Handle()));
  EXPECT_EQ(base::MemoryState::NORMAL,
            coordinator_->GetStateForProcess(process2.Handle()));
}

}  // namespace content
