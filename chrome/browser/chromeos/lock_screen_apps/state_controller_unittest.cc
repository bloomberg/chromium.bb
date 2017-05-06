// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/interfaces/tray_action.mojom.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_observer.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::mojom::TrayActionState;

namespace {

class TestStateObserver : public lock_screen_apps::StateObserver {
 public:
  TestStateObserver() = default;
  ~TestStateObserver() override = default;

  void OnLockScreenNoteStateChanged(TrayActionState state) override {
    observed_states_.push_back(state);
  }

  const std::vector<TrayActionState> observed_states() const {
    return observed_states_;
  }

  void ClearObservedStates() { observed_states_.clear(); }

 private:
  std::vector<TrayActionState> observed_states_;

  DISALLOW_COPY_AND_ASSIGN(TestStateObserver);
};

class TestTrayAction : public ash::mojom::TrayAction {
 public:
  TestTrayAction() : binding_(this) {}

  ~TestTrayAction() override = default;

  ash::mojom::TrayActionPtr CreateInterfacePtrAndBind() {
    return binding_.CreateInterfacePtrAndBind();
  }

  void SetClient(ash::mojom::TrayActionClientPtr client,
                 TrayActionState state) override {
    client_ = std::move(client);
    EXPECT_EQ(TrayActionState::kNotAvailable, state);
  }

  void UpdateLockScreenNoteState(TrayActionState state) override {
    observed_states_.push_back(state);
  }

  void SendNewNoteRequest() {
    ASSERT_TRUE(client_);
    client_->RequestNewLockScreenNote();
  }

  const std::vector<TrayActionState>& observed_states() const {
    return observed_states_;
  }

  void ClearObservedStates() { observed_states_.clear(); }

 private:
  mojo::Binding<ash::mojom::TrayAction> binding_;
  ash::mojom::TrayActionClientPtr client_;

  std::vector<TrayActionState> observed_states_;

  DISALLOW_COPY_AND_ASSIGN(TestTrayAction);
};

class LockScreenAppStateNotSupportedTest : public testing::Test {
 public:
  LockScreenAppStateNotSupportedTest() = default;

  ~LockScreenAppStateNotSupportedTest() override = default;

  void SetUp() override {
    command_line_ = base::MakeUnique<base::test::ScopedCommandLine>();
    command_line_->GetProcessCommandLine()->InitFromArgv({""});
  }

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenAppStateNotSupportedTest);
};

class LockScreenAppStateTest : public testing::Test {
 public:
  LockScreenAppStateTest() = default;
  ~LockScreenAppStateTest() override = default;

  void SetUp() override {
    command_line_ = base::MakeUnique<base::test::ScopedCommandLine>();
    command_line_->GetProcessCommandLine()->InitFromArgv(
        {"", "--enable-lock-screen-apps"});

    ASSERT_TRUE(lock_screen_apps::StateController::IsEnabled());

    state_controller_ = base::MakeUnique<lock_screen_apps::StateController>();
    state_controller_->SetTrayActionPtrForTesting(
        tray_action_.CreateInterfacePtrAndBind());
    state_controller_->Initialize();
    state_controller_->FlushTrayActionForTesting();

    state_controller_->AddObserver(&observer_);
  }

  void TearDown() override {
    state_controller_->RemoveObserver(&observer_);
    state_controller_.reset();
  }

  TestStateObserver* observer() { return &observer_; }

  TestTrayAction* tray_action() { return &tray_action_; }

  lock_screen_apps::StateController* state_controller() {
    return state_controller_.get();
  }

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  content::TestBrowserThreadBundle threads_;

  std::unique_ptr<lock_screen_apps::StateController> state_controller_;
  TestStateObserver observer_;
  TestTrayAction tray_action_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenAppStateTest);
};

}  // namespace

TEST_F(LockScreenAppStateNotSupportedTest, NoInstance) {
  EXPECT_FALSE(lock_screen_apps::StateController::IsEnabled());
}

TEST_F(LockScreenAppStateTest, InitialState) {
  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());

  state_controller()->MoveToBackground();

  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());

  EXPECT_EQ(0u, observer()->observed_states().size());
  EXPECT_EQ(0u, tray_action()->observed_states().size());
}

TEST_F(LockScreenAppStateTest, MoveToBackgroundFromActive) {
  state_controller()->SetLockScreenNoteStateForTesting(
      TrayActionState::kActive);

  state_controller()->MoveToBackground();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kBackground,
            state_controller()->GetLockScreenNoteState());

  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kBackground, observer()->observed_states()[0]);
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kBackground, tray_action()->observed_states()[0]);
}

TEST_F(LockScreenAppStateTest, HandleActionWhenNotAvaiable) {
  ASSERT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(0u, observer()->observed_states().size());
  EXPECT_EQ(0u, tray_action()->observed_states().size());
}

TEST_F(LockScreenAppStateTest, HandleAction) {
  state_controller()->SetLockScreenNoteStateForTesting(
      TrayActionState::kAvailable);

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());
  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kActive, observer()->observed_states()[0]);
  observer()->ClearObservedStates();
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kActive, tray_action()->observed_states()[0]);
  tray_action()->ClearObservedStates();

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  // There should be no state change - the state_controller was already in
  // active state when the request was received.
  EXPECT_EQ(0u, observer()->observed_states().size());
  EXPECT_EQ(0u, tray_action()->observed_states().size());
}
