// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/tray_action/tray_action.h"

#include <memory>
#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"

using ash::mojom::TrayActionState;

namespace ash {

namespace {

class ScopedTestStateObserver : public TrayActionObserver {
 public:
  explicit ScopedTestStateObserver(TrayAction* tray_action)
      : tray_action_(tray_action) {
    tray_action_->AddObserver(this);
  }

  ~ScopedTestStateObserver() override { tray_action_->RemoveObserver(this); }

  // TrayActionObserver:
  void OnLockScreenNoteStateChanged(TrayActionState state) override {
    observed_states_.push_back(state);
  }

  const std::vector<TrayActionState>& observed_states() const {
    return observed_states_;
  }

  void ClearObservedStates() { observed_states_.clear(); }

 private:
  TrayAction* tray_action_;

  std::vector<TrayActionState> observed_states_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTestStateObserver);
};

class TestTrayActionClient : public mojom::TrayActionClient {
 public:
  TestTrayActionClient() : binding_(this) {}

  ~TestTrayActionClient() override = default;

  mojom::TrayActionClientPtr CreateInterfacePtrAndBind() {
    mojom::TrayActionClientPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // mojom::TrayActionClient:
  void RequestNewLockScreenNote() override { action_requests_count_++; }

  int action_requests_count() const { return action_requests_count_; }

 private:
  mojo::Binding<ash::mojom::TrayActionClient> binding_;

  int action_requests_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestTrayActionClient);
};

using TrayActionTest = AshTestBase;

}  // namespace

TEST_F(TrayActionTest, NoTrayActionClient) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  ScopedTestStateObserver observer(tray_action);

  EXPECT_EQ(TrayActionState::kNotAvailable,
            tray_action->GetLockScreenNoteState());

  tray_action->UpdateLockScreenNoteState(TrayActionState::kAvailable);

  // The effective state should be |kNotAvailable| as long as an action handler
  // is not set.
  EXPECT_EQ(TrayActionState::kNotAvailable,
            tray_action->GetLockScreenNoteState());
  EXPECT_EQ(0u, observer.observed_states().size());

  std::unique_ptr<TestTrayActionClient> action_client =
      base::MakeUnique<TestTrayActionClient>();
  tray_action->SetClient(action_client->CreateInterfacePtrAndBind(),
                         TrayActionState::kLaunching);

  EXPECT_EQ(TrayActionState::kLaunching, tray_action->GetLockScreenNoteState());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kLaunching, observer.observed_states()[0]);
  observer.ClearObservedStates();

  action_client.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(TrayActionState::kNotAvailable,
            tray_action->GetLockScreenNoteState());
  EXPECT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kNotAvailable, observer.observed_states()[0]);
}

TEST_F(TrayActionTest, SettingInitialState) {
  TrayAction* tray_action = Shell::Get()->tray_action();

  ScopedTestStateObserver observer(tray_action);
  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateInterfacePtrAndBind(),
                         TrayActionState::kAvailable);

  EXPECT_EQ(TrayActionState::kAvailable, tray_action->GetLockScreenNoteState());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer.observed_states()[0]);
}

TEST_F(TrayActionTest, StateChangeNotificationOnConnectionLoss) {
  TrayAction* tray_action = Shell::Get()->tray_action();

  ScopedTestStateObserver observer(tray_action);
  std::unique_ptr<TestTrayActionClient> action_client(
      new TestTrayActionClient());
  tray_action->SetClient(action_client->CreateInterfacePtrAndBind(),
                         TrayActionState::kAvailable);

  EXPECT_EQ(TrayActionState::kAvailable, tray_action->GetLockScreenNoteState());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer.observed_states()[0]);
  observer.ClearObservedStates();

  action_client.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(TrayActionState::kNotAvailable,
            tray_action->GetLockScreenNoteState());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kNotAvailable, observer.observed_states()[0]);
}

TEST_F(TrayActionTest, NormalStateProgression) {
  TrayAction* tray_action = Shell::Get()->tray_action();

  ScopedTestStateObserver observer(tray_action);
  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateInterfacePtrAndBind(),
                         TrayActionState::kNotAvailable);

  tray_action->UpdateLockScreenNoteState(TrayActionState::kAvailable);
  EXPECT_EQ(TrayActionState::kAvailable, tray_action->GetLockScreenNoteState());
  EXPECT_FALSE(tray_action->IsLockScreenNoteActive());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer.observed_states()[0]);
  observer.ClearObservedStates();

  tray_action->UpdateLockScreenNoteState(TrayActionState::kLaunching);
  EXPECT_EQ(TrayActionState::kLaunching, tray_action->GetLockScreenNoteState());
  EXPECT_FALSE(tray_action->IsLockScreenNoteActive());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kLaunching, observer.observed_states()[0]);
  observer.ClearObservedStates();

  tray_action->UpdateLockScreenNoteState(TrayActionState::kActive);
  EXPECT_EQ(TrayActionState::kActive, tray_action->GetLockScreenNoteState());
  EXPECT_TRUE(tray_action->IsLockScreenNoteActive());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kActive, observer.observed_states()[0]);

  observer.ClearObservedStates();
  tray_action->UpdateLockScreenNoteState(TrayActionState::kBackground);
  EXPECT_EQ(TrayActionState::kBackground,
            tray_action->GetLockScreenNoteState());
  EXPECT_FALSE(tray_action->IsLockScreenNoteActive());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kBackground, observer.observed_states()[0]);
  observer.ClearObservedStates();

  tray_action->UpdateLockScreenNoteState(TrayActionState::kNotAvailable);
  EXPECT_EQ(TrayActionState::kNotAvailable,
            tray_action->GetLockScreenNoteState());
  EXPECT_FALSE(tray_action->IsLockScreenNoteActive());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kNotAvailable, observer.observed_states()[0]);
}

TEST_F(TrayActionTest, ObserversNotNotifiedOnDuplicateState) {
  TrayAction* tray_action = Shell::Get()->tray_action();

  ScopedTestStateObserver observer(tray_action);
  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateInterfacePtrAndBind(),
                         TrayActionState::kNotAvailable);

  tray_action->UpdateLockScreenNoteState(TrayActionState::kAvailable);
  EXPECT_EQ(TrayActionState::kAvailable, tray_action->GetLockScreenNoteState());
  ASSERT_EQ(1u, observer.observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer.observed_states()[0]);
  observer.ClearObservedStates();

  tray_action->UpdateLockScreenNoteState(TrayActionState::kAvailable);
  EXPECT_EQ(TrayActionState::kAvailable, tray_action->GetLockScreenNoteState());
  ASSERT_EQ(0u, observer.observed_states().size());
}

TEST_F(TrayActionTest, RequestAction) {
  TrayAction* tray_action = Shell::Get()->tray_action();

  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateInterfacePtrAndBind(),
                         TrayActionState::kNotAvailable);

  EXPECT_EQ(0, action_client.action_requests_count());
  tray_action->RequestNewLockScreenNote();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, action_client.action_requests_count());

  tray_action->UpdateLockScreenNoteState(TrayActionState::kAvailable);
  tray_action->RequestNewLockScreenNote();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, action_client.action_requests_count());
}

// Tests that there is no crash if handler is not set.
TEST_F(TrayActionTest, RequestActionWithNoHandler) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  tray_action->RequestNewLockScreenNote();
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
