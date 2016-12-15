// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_coordinator_impl.h"

#include "base/memory/memory_coordinator_proxy.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "content/browser/memory/memory_monitor.h"
#include "content/browser/memory/memory_state_updater.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockMemoryCoordinatorClient : public base::MemoryCoordinatorClient {
 public:
  void OnMemoryStateChange(base::MemoryState state) override {
    is_called_ = true;
    state_ = state;
  }

  bool is_called() { return is_called_; }
  base::MemoryState state() { return state_; }

 private:
  bool is_called_ = false;
  base::MemoryState state_ = base::MemoryState::NORMAL;
};

}  // namespace

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

class MemoryCoordinatorImplTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kMemoryCoordinator);

    task_runner_ = new base::TestMockTimeTaskRunner();
    coordinator_.reset(new MemoryCoordinatorImpl(
        task_runner_, base::WrapUnique(new MockMemoryMonitor)));

    base::MemoryCoordinatorProxy::GetInstance()->
        SetGetCurrentMemoryStateCallback(base::Bind(
            &MemoryCoordinator::GetCurrentMemoryState,
            base::Unretained(coordinator_.get())));
    base::MemoryCoordinatorProxy::GetInstance()->
        SetSetCurrentMemoryStateForTestingCallback(base::Bind(
            &MemoryCoordinator::SetCurrentMemoryStateForTesting,
            base::Unretained(coordinator_.get())));
  }

  MockMemoryMonitor* GetMockMemoryMonitor() {
    return static_cast<MockMemoryMonitor*>(coordinator_->memory_monitor());
  }

 protected:
  std::unique_ptr<MemoryCoordinatorImpl> coordinator_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::MessageLoop message_loop_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

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
    base::RunLoop loop;
    loop.RunUntilIdle();
    EXPECT_TRUE(client.is_called());
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
    base::RunLoop loop;
    loop.RunUntilIdle();
    EXPECT_FALSE(client.is_called());
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
  base::RunLoop loop;
  loop.RunUntilIdle();
  EXPECT_TRUE(client.is_called());
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

}  // namespace content
