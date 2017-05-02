// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_observer.h"
#include "chrome/browser/chromeos/lock_screen_apps/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const lock_screen_apps::Action kTestAction = lock_screen_apps::Action::kNewNote;

class TestStateObserver : public lock_screen_apps::StateObserver {
 public:
  TestStateObserver() = default;
  ~TestStateObserver() override = default;

  void OnLockScreenAppsStateChanged(
      lock_screen_apps::Action action,
      lock_screen_apps::ActionState state) override {
    ASSERT_EQ(kTestAction, action);

    ++state_change_count_;
  }

  int state_change_count() const { return state_change_count_; }

 private:
  int state_change_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestStateObserver);
};

class LockScreenAppStateControllerNotSupportedTest : public testing::Test {
 public:
  LockScreenAppStateControllerNotSupportedTest() = default;

  ~LockScreenAppStateControllerNotSupportedTest() override = default;

  void SetUp() override {
    command_line_ = base::MakeUnique<base::test::ScopedCommandLine>();
    command_line_->GetProcessCommandLine()->InitFromArgv({""});
  }

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenAppStateControllerNotSupportedTest);
};

class LockScreenAppStateControllerTest : public testing::Test {
 public:
  LockScreenAppStateControllerTest() = default;
  ~LockScreenAppStateControllerTest() override = default;

  void SetUp() override {
    command_line_ = base::MakeUnique<base::test::ScopedCommandLine>();
    command_line_->GetProcessCommandLine()->InitFromArgv(
        {"", "--enable-lock-screen-apps"});

    state_controller_ = lock_screen_apps::StateController::Get();
    ASSERT_TRUE(state_controller_);

    state_controller_->AddObserver(&observer_);
  }

  void TearDown() override { state_controller_->RemoveObserver(&observer_); }

  const TestStateObserver& observer() const { return observer_; }

  lock_screen_apps::StateController* state_controller() {
    return state_controller_;
  }

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;

  lock_screen_apps::StateController* state_controller_;
  TestStateObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenAppStateControllerTest);
};

}  // namespace

TEST_F(LockScreenAppStateControllerNotSupportedTest, NoControllerInstance) {
  EXPECT_FALSE(lock_screen_apps::StateController::Get());
}

TEST_F(LockScreenAppStateControllerTest, InitialState) {
  EXPECT_EQ(lock_screen_apps::ActionState::kNotSupported,
            state_controller()->GetActionState(kTestAction));

  ASSERT_EQ(0, observer().state_change_count());

  state_controller()->MoveToBackground();

  EXPECT_EQ(lock_screen_apps::ActionState::kNotSupported,
            state_controller()->GetActionState(kTestAction));

  ASSERT_EQ(0, observer().state_change_count());
}

TEST_F(LockScreenAppStateControllerTest, HandleActionWhenNotAvaiable) {
  ASSERT_EQ(lock_screen_apps::ActionState::kNotSupported,
            state_controller()->GetActionState(kTestAction));
  EXPECT_FALSE(
      state_controller()->HandleAction(lock_screen_apps::Action::kNewNote));
}
