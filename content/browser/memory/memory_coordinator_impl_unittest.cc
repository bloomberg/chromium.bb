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
#include "content/browser/memory/memory_condition_observer.h"
#include "content/browser/memory/memory_monitor.h"
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

  void OnPurgeMemory() override { ++purge_memory_calls_; }

  bool did_state_changed() const { return did_state_changed_; }
  base::MemoryState state() const { return state_; }
  int purge_memory_calls() const { return purge_memory_calls_; }

 private:
  bool did_state_changed_ = false;
  base::MemoryState state_ = base::MemoryState::NORMAL;
  int purge_memory_calls_ = 0;
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

  void DiscardTab() override { ++discard_tab_count_; }

  int discard_tab_count() const { return discard_tab_count_; }

 private:
  int discard_tab_count_ = 0;

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
    SetTickClockForTesting(task_runner->GetMockTickClock());
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
  EXPECT_EQ(0, cmc2->on_state_change_calls());
  EXPECT_EQ(mojom::MemoryState::NORMAL, cmc2->state());
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
  EXPECT_TRUE(coordinator_->SetChildMemoryState(1, MemoryState::THROTTLED));
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
}

// TODO(bashi): Move policy specific tests into a separate file.
TEST_F(MemoryCoordinatorImplTest, OnChildVisibilityChanged) {
  auto* child = coordinator_->CreateChildMemoryCoordinator(1);

  coordinator_->memory_condition_ = MemoryCondition::NORMAL;
  coordinator_->policy_->OnChildVisibilityChanged(1, true);
  RunUntilIdle();
  EXPECT_EQ(mojom::MemoryState::NORMAL, child->state());
  coordinator_->policy_->OnChildVisibilityChanged(1, false);
  RunUntilIdle();
#if defined(OS_ANDROID)
  EXPECT_EQ(mojom::MemoryState::THROTTLED, child->state());
#else
  EXPECT_EQ(mojom::MemoryState::NORMAL, child->state());
#endif

  coordinator_->memory_condition_ = MemoryCondition::CRITICAL;
  coordinator_->policy_->OnChildVisibilityChanged(1, true);
  RunUntilIdle();
  EXPECT_EQ(mojom::MemoryState::THROTTLED, child->state());
  coordinator_->policy_->OnChildVisibilityChanged(1, false);
  RunUntilIdle();
  EXPECT_EQ(mojom::MemoryState::THROTTLED, child->state());
}

TEST_F(MemoryCoordinatorImplTest, CalculateNextCondition) {
  auto* condition_observer = coordinator_->condition_observer_.get();

  // The default condition is NORMAL.
  EXPECT_EQ(MemoryCondition::NORMAL, coordinator_->GetMemoryCondition());

  // Transitions from NORMAL
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(1000);
  EXPECT_EQ(MemoryCondition::NORMAL,
            condition_observer->CalculateNextCondition());
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(0);
  EXPECT_EQ(MemoryCondition::CRITICAL,
            condition_observer->CalculateNextCondition());

  // Transitions from CRITICAL
  coordinator_->memory_condition_ = MemoryCondition::CRITICAL;
  EXPECT_EQ(MemoryCondition::CRITICAL, coordinator_->GetMemoryCondition());
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(1000);
  EXPECT_EQ(MemoryCondition::NORMAL,
            condition_observer->CalculateNextCondition());
}

TEST_F(MemoryCoordinatorImplTest, UpdateCondition) {
  auto* condition_observer = coordinator_->condition_observer_.get();

  auto* foreground_child = coordinator_->CreateChildMemoryCoordinator(1);
  auto* background_child = coordinator_->CreateChildMemoryCoordinator(2);
  auto iter = coordinator_->children().find(2);
  iter->second.is_visible = false;

  {
    // Transition happens (NORMAL -> CRITICAL).
    // All processes should be in THROTTLED memory state.
    MockMemoryCoordinatorClient client;
    base::MemoryCoordinatorClientRegistry::GetInstance()->Register(&client);
    GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(0);
    condition_observer->UpdateCondition();
    task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(40));
    RunUntilIdle();
    EXPECT_TRUE(client.did_state_changed());
    EXPECT_EQ(base::MemoryState::THROTTLED, client.state());
    EXPECT_EQ(mojom::MemoryState::THROTTLED, foreground_child->state());
    EXPECT_EQ(mojom::MemoryState::THROTTLED, background_child->state());
    base::MemoryCoordinatorClientRegistry::GetInstance()->Unregister(&client);
  }

  {
    // No transtion (NORMAL -> NORMAL). OnStateChange shouldn't be called.
    MockMemoryCoordinatorClient client;
    base::MemoryCoordinatorClientRegistry::GetInstance()->Register(&client);
    coordinator_->memory_condition_ = MemoryCondition::NORMAL;
    GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(1000);
    condition_observer->UpdateCondition();
    RunUntilIdle();
    EXPECT_FALSE(client.did_state_changed());
    EXPECT_EQ(base::MemoryState::NORMAL, client.state());
    base::MemoryCoordinatorClientRegistry::GetInstance()->Unregister(&client);
  }

  {
    // Transition happends but browser state change is delayed
    // (CRITICAL -> NORMAL).
    base::TimeDelta transition_interval =
        coordinator_->minimum_state_transition_period_ +
        base::TimeDelta::FromSeconds(1);
    coordinator_->memory_condition_ = MemoryCondition::NORMAL;
    coordinator_->last_state_change_ = task_runner_->NowTicks();
    GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(0);
    condition_observer->UpdateCondition();
    EXPECT_EQ(base::MemoryState::THROTTLED,
              coordinator_->GetCurrentMemoryState());

    // Back to NORMAL condition before |minimum_state_transition_period_| is
    // passed. At this point the browser's state shouldn't be changed.
    GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(1000);
    condition_observer->UpdateCondition();
    task_runner_->FastForwardBy(transition_interval / 2);
    EXPECT_EQ(MemoryCondition::NORMAL, coordinator_->GetMemoryCondition());
    EXPECT_EQ(base::MemoryState::THROTTLED,
              coordinator_->GetCurrentMemoryState());

    // |minimum_state_transition_period_| is passed. State should be changed.
    task_runner_->FastForwardBy(transition_interval / 2);
    EXPECT_EQ(base::MemoryState::NORMAL, coordinator_->GetCurrentMemoryState());
  }
}

// TODO(bashi): Move ForceSetMemoryCondition?
TEST_F(MemoryCoordinatorImplTest, ForceSetMemoryCondition) {
  auto* condition_observer = coordinator_->condition_observer_.get();
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(1000);

  base::TimeDelta interval = base::TimeDelta::FromSeconds(5);
  condition_observer->monitoring_interval_ = interval;

  // Starts updating memory condition. The initial condition should be NORMAL
  // with above configuration.
  coordinator_->Start();
  task_runner_->RunUntilIdle();
  EXPECT_EQ(MemoryCondition::NORMAL, coordinator_->GetMemoryCondition());

  base::TimeDelta force_set_duration = interval * 3;
  coordinator_->ForceSetMemoryCondition(MemoryCondition::CRITICAL,
                                        force_set_duration);
  EXPECT_EQ(MemoryCondition::CRITICAL, coordinator_->GetMemoryCondition());

  // The condition should remain CRITICAL even after some monitoring period are
  // passed.
  task_runner_->FastForwardBy(interval * 2);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(MemoryCondition::CRITICAL, coordinator_->GetMemoryCondition());

  // The condition should be updated after |force_set_duration| is passed.
  task_runner_->FastForwardBy(force_set_duration);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(MemoryCondition::NORMAL, coordinator_->GetMemoryCondition());

  // Also make sure that the condition is updated based on free avaiable memory.
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(0);
  task_runner_->FastForwardBy(interval * 2);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(MemoryCondition::CRITICAL, coordinator_->GetMemoryCondition());
}

TEST_F(MemoryCoordinatorImplTest, DiscardTab) {
  coordinator_->DiscardTab();
  EXPECT_EQ(1, coordinator_->GetDelegate()->discard_tab_count());
}

TEST_F(MemoryCoordinatorImplTest, DiscardTabUnderCritical) {
  auto* condition_observer = coordinator_->condition_observer_.get();
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(1000);

  base::TimeDelta interval = base::TimeDelta::FromSeconds(5);
  condition_observer->monitoring_interval_ = interval;

  auto* delegate = coordinator_->GetDelegate();

  coordinator_->Start();
  task_runner_->RunUntilIdle();
  EXPECT_EQ(MemoryCondition::NORMAL, coordinator_->GetMemoryCondition());
  EXPECT_EQ(0, delegate->discard_tab_count());

  // Enter CRITICAL condition. Tab discarding should start.
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(0);
  task_runner_->FastForwardBy(interval);
  EXPECT_EQ(1, delegate->discard_tab_count());
  task_runner_->FastForwardBy(interval);
  EXPECT_EQ(2, delegate->discard_tab_count());

  // Back to NORMAL. Tab discarding should stop.
  GetMockMemoryMonitor()->SetFreeMemoryUntilCriticalMB(1000);
  task_runner_->FastForwardBy(interval);
  EXPECT_EQ(2, delegate->discard_tab_count());
  task_runner_->FastForwardBy(interval);
  EXPECT_EQ(2, delegate->discard_tab_count());
}

// TODO(bashi): Move policy specific tests into a separate file.
TEST_F(MemoryCoordinatorImplTest, OnCriticalCondition) {
  MockMemoryCoordinatorClient client;
  base::MemoryCoordinatorClientRegistry::GetInstance()->Register(&client);
  auto* child1 = coordinator_->CreateChildMemoryCoordinator(1);
  auto* child2 = coordinator_->CreateChildMemoryCoordinator(2);
  auto* delegate = coordinator_->GetDelegate();
  base::TimeDelta interval = base::TimeDelta::FromSeconds(31);

  // child1: Foreground, child2: Background
  coordinator_->policy_->OnChildVisibilityChanged(1, true);
  coordinator_->policy_->OnChildVisibilityChanged(2, false);

  // Purge memory from all children regardless of their visibility.
  task_runner_->FastForwardBy(interval);
  coordinator_->policy_->OnCriticalCondition();
  RunUntilIdle();
  task_runner_->FastForwardBy(interval);
  coordinator_->policy_->OnCriticalCondition();
  RunUntilIdle();
  EXPECT_EQ(2, delegate->discard_tab_count());
  EXPECT_EQ(0, client.purge_memory_calls());
  EXPECT_EQ(1, child1->purge_memory_calls());
  EXPECT_EQ(1, child2->purge_memory_calls());

  // Purge memory from browser process only after we asked all children to
  // purge memory.
  task_runner_->FastForwardBy(interval);
  coordinator_->policy_->OnCriticalCondition();
  RunUntilIdle();
  EXPECT_EQ(3, delegate->discard_tab_count());
  EXPECT_EQ(1, client.purge_memory_calls());
  EXPECT_EQ(1, child1->purge_memory_calls());
  EXPECT_EQ(1, child2->purge_memory_calls());

  // Don't request purging for a certain period of time if we already requested.
  task_runner_->FastForwardBy(interval);
  coordinator_->policy_->OnCriticalCondition();
  RunUntilIdle();
  EXPECT_EQ(4, delegate->discard_tab_count());
  EXPECT_EQ(1, client.purge_memory_calls());
  EXPECT_EQ(1, child1->purge_memory_calls());
  EXPECT_EQ(1, child2->purge_memory_calls());

  base::MemoryCoordinatorClientRegistry::GetInstance()->Unregister(&client);
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

  base::SpawnChildResult spawn_result1 = SpawnChild("process1");
  base::Process& process1 = spawn_result1.process;
  base::SpawnChildResult spawn_result2 = SpawnChild("process2");
  base::Process& process2 = spawn_result2.process;

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
