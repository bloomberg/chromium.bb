// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/interfaces/tray_action.mojom.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/chromeos/lock_screen_apps/app_manager.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_observer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::mojom::TrayActionState;

namespace {

class TestAppManager : public lock_screen_apps::AppManager {
 public:
  enum class State {
    kNotInitialized,
    kStarted,
    kStopped,
  };

  TestAppManager(Profile* expected_primary_profile,
                 Profile* expected_lock_screen_profile)
      : expected_primary_profile_(expected_primary_profile),
        expected_lock_screen_profile_(expected_lock_screen_profile) {}

  ~TestAppManager() override = default;

  void Initialize(Profile* primary_profile,
                  Profile* lock_screen_profile) override {
    ASSERT_EQ(State::kNotInitialized, state_);
    ASSERT_EQ(expected_primary_profile_, primary_profile);
    ASSERT_EQ(expected_lock_screen_profile_, lock_screen_profile);

    state_ = State::kStopped;
  }

  void Start(const base::Closure& change_callback) override {
    ASSERT_TRUE(change_callback_.is_null());
    ASSERT_FALSE(change_callback.is_null());
    change_callback_ = change_callback;
    state_ = State::kStarted;
  }

  void Stop() override {
    change_callback_.Reset();
    state_ = State::kStopped;
  }

  bool LaunchNoteTaking() override {
    EXPECT_EQ(State::kStarted, state_);
    ++launch_count_;
    return app_launchable_;
  }

  bool IsNoteTakingAppAvailable() const override {
    return state_ == State::kStarted && !app_id_.empty();
  }

  std::string GetNoteTakingAppId() const override {
    if (state_ != State::kStarted)
      return std::string();
    return app_id_;
  }

  void SetInitialAppState(const std::string& app_id, bool app_launchable) {
    ASSERT_NE(State::kStarted, state_);

    app_launchable_ = app_launchable;
    app_id_ = app_id;
  }

  void UpdateApp(const std::string& app_id, bool app_launchable) {
    ASSERT_EQ(State::kStarted, state_);

    app_launchable_ = app_launchable;
    if (app_id == app_id_)
      return;
    app_id_ = app_id;

    change_callback_.Run();
  }

  State state() const { return state_; }

  int launch_count() const { return launch_count_; }

 private:
  const Profile* const expected_primary_profile_;
  const Profile* const expected_lock_screen_profile_;

  base::Closure change_callback_;

  State state_ = State::kNotInitialized;

  // Number of requested app launches.
  int launch_count_ = 0;

  // Information about the test app:
  // The app ID.
  std::string app_id_;
  // Whether app launch should succeed.
  bool app_launchable_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestAppManager);
};

class TestStateObserver : public lock_screen_apps::StateObserver {
 public:
  TestStateObserver() = default;
  ~TestStateObserver() override = default;

  void OnLockScreenNoteStateChanged(TrayActionState state) override {
    observed_states_.push_back(state);
  }

  const std::vector<TrayActionState>& observed_states() const {
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
    ash::mojom::TrayActionPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
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
  LockScreenAppStateTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~LockScreenAppStateTest() override = default;

  void SetUp() override {
    command_line_ = base::MakeUnique<base::test::ScopedCommandLine>();
    command_line_->GetProcessCommandLine()->InitFromArgv(
        {"", "--enable-lock-screen-apps"});

    ASSERT_TRUE(profile_manager_.SetUp());

    session_manager_ = base::MakeUnique<session_manager::SessionManager>();
    session_manager_->SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);

    ASSERT_TRUE(lock_screen_apps::StateController::IsEnabled());

    state_controller_ = base::MakeUnique<lock_screen_apps::StateController>();
    state_controller_->SetTrayActionPtrForTesting(
        tray_action_.CreateInterfacePtrAndBind());
    state_controller_->Initialize();
    state_controller_->FlushTrayActionForTesting();

    state_controller_->AddObserver(&observer_);

    // Create fake sign-in profile.
    TestingProfile::Builder builder;
    builder.SetPath(
        profile_manager_.profiles_dir().AppendASCII(chrome::kInitialProfile));
    TestingProfile* signin_profile = builder.BuildIncognito(
        profile_manager_.CreateTestingProfile(chrome::kInitialProfile));

    std::unique_ptr<TestAppManager> app_manager =
        base::MakeUnique<TestAppManager>(&profile_,
                                         signin_profile->GetOriginalProfile());
    app_manager_ = app_manager.get();
    state_controller_->SetAppManagerForTesting(std::move(app_manager));
  }

  void TearDown() override {
    state_controller_->RemoveObserver(&observer_);
    state_controller_.reset();
    session_manager_.reset();
    app_manager_ = nullptr;
  }

  // Helper method to move state controller to available state.
  // Should be called at the begining of tests, at most once.
  bool EnableNoteTakingApp(bool enable_app_launch) {
    app_manager_->SetInitialAppState("test_app_id", enable_app_launch);
    state_controller_->SetPrimaryProfile(&profile_);
    session_manager_->SetSessionState(session_manager::SessionState::LOCKED);
    if (app_manager_->state() != TestAppManager::State::kStarted) {
      ADD_FAILURE() << "Lock app manager Start not invoked.";
      return false;
    }
    state_controller_->FlushTrayActionForTesting();

    observer_.ClearObservedStates();
    tray_action_.ClearObservedStates();

    return state_controller_->GetLockScreenNoteState() ==
           TrayActionState::kAvailable;
  }

  TestingProfile* profile() { return &profile_; }

  session_manager::SessionManager* session_manager() {
    return session_manager_.get();
  }

  TestStateObserver* observer() { return &observer_; }

  TestTrayAction* tray_action() { return &tray_action_; }

  lock_screen_apps::StateController* state_controller() {
    return state_controller_.get();
  }

  TestAppManager* app_manager() { return app_manager_; }

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  content::TestBrowserThreadBundle threads_;
  TestingProfileManager profile_manager_;
  TestingProfile profile_;

  std::unique_ptr<session_manager::SessionManager> session_manager_;

  std::unique_ptr<lock_screen_apps::StateController> state_controller_;

  TestStateObserver observer_;
  TestTrayAction tray_action_;
  TestAppManager* app_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LockScreenAppStateTest);
};

}  // namespace

TEST_F(LockScreenAppStateNotSupportedTest, NoInstance) {
  EXPECT_FALSE(lock_screen_apps::StateController::IsEnabled());
}

TEST_F(LockScreenAppStateTest, InitialState) {
  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());

  EXPECT_EQ(TestAppManager::State::kNotInitialized, app_manager()->state());
  state_controller()->MoveToBackground();

  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());

  EXPECT_EQ(0u, observer()->observed_states().size());
  EXPECT_EQ(0u, tray_action()->observed_states().size());
}

TEST_F(LockScreenAppStateTest, SetPrimaryProfile) {
  EXPECT_EQ(TestAppManager::State::kNotInitialized, app_manager()->state());
  state_controller()->SetPrimaryProfile(profile());

  EXPECT_EQ(TestAppManager::State::kStopped, app_manager()->state());
  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());
  EXPECT_EQ(0u, observer()->observed_states().size());
}

TEST_F(LockScreenAppStateTest, SetPrimaryProfileWhenSessionLocked) {
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());

  EXPECT_EQ(TestAppManager::State::kNotInitialized, app_manager()->state());

  app_manager()->SetInitialAppState("app_id", true);
  state_controller()->SetPrimaryProfile(profile());

  ASSERT_EQ(TestAppManager::State::kStarted, app_manager()->state());

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());

  state_controller()->FlushTrayActionForTesting();
  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer()->observed_states()[0]);
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, tray_action()->observed_states()[0]);
}

TEST_F(LockScreenAppStateTest, SessionLock) {
  app_manager()->SetInitialAppState("app_id", true);
  state_controller()->SetPrimaryProfile(profile());
  ASSERT_EQ(TestAppManager::State::kStopped, app_manager()->state());

  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

  ASSERT_EQ(TestAppManager::State::kStarted, app_manager()->state());

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
  state_controller()->FlushTrayActionForTesting();
  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer()->observed_states()[0]);
  observer()->ClearObservedStates();
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, tray_action()->observed_states()[0]);
  tray_action()->ClearObservedStates();

  // When the session is unlocked again, app manager is stopped, and tray action
  // disabled again.
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);

  EXPECT_EQ(TestAppManager::State::kStopped, app_manager()->state());

  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());
  state_controller()->FlushTrayActionForTesting();
  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kNotAvailable, observer()->observed_states()[0]);
  observer()->ClearObservedStates();
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kNotAvailable,
            tray_action()->observed_states()[0]);
  tray_action()->ClearObservedStates();

  // Test that subsequent session lock works as expected.
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  ASSERT_EQ(TestAppManager::State::kStarted, app_manager()->state());

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
  state_controller()->FlushTrayActionForTesting();
  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer()->observed_states()[0]);
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, tray_action()->observed_states()[0]);
}

TEST_F(LockScreenAppStateTest, SessionUnlockedWhileStartingAppManager) {
  state_controller()->SetPrimaryProfile(profile());
  ASSERT_EQ(TestAppManager::State::kStopped, app_manager()->state());

  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

  ASSERT_EQ(TestAppManager::State::kStarted, app_manager()->state());

  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(TestAppManager::State::kStopped, app_manager()->state());

  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());
  state_controller()->FlushTrayActionForTesting();
  EXPECT_EQ(0u, observer()->observed_states().size());
  EXPECT_EQ(0u, tray_action()->observed_states().size());

  // Test that subsequent session lock works as expected.
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

  ASSERT_EQ(TestAppManager::State::kStarted, app_manager()->state());
  app_manager()->UpdateApp("app_id", true);

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
  state_controller()->FlushTrayActionForTesting();
  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer()->observed_states()[0]);
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, tray_action()->observed_states()[0]);
}

TEST_F(LockScreenAppStateTest, AppManagerNoApp) {
  state_controller()->SetPrimaryProfile(profile());
  ASSERT_EQ(TestAppManager::State::kStopped, app_manager()->state());

  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_EQ(TestAppManager::State::kStarted, app_manager()->state());

  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());
  state_controller()->FlushTrayActionForTesting();
  EXPECT_EQ(0u, observer()->observed_states().size());
  EXPECT_EQ(0u, tray_action()->observed_states().size());

  tray_action()->SendNewNoteRequest();

  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());
  state_controller()->FlushTrayActionForTesting();
  EXPECT_EQ(0u, observer()->observed_states().size());
  EXPECT_EQ(0u, tray_action()->observed_states().size());

  // App manager should be started on next session lock.
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(TestAppManager::State::kStopped, app_manager()->state());
  app_manager()->SetInitialAppState("app_id", false);

  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
  state_controller()->FlushTrayActionForTesting();
  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer()->observed_states()[0]);
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, tray_action()->observed_states()[0]);
}

TEST_F(LockScreenAppStateTest, AppAvailabilityChanges) {
  state_controller()->SetPrimaryProfile(profile());
  ASSERT_EQ(TestAppManager::State::kStopped, app_manager()->state());

  app_manager()->SetInitialAppState("app_id", false);
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_EQ(TestAppManager::State::kStarted, app_manager()->state());

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
  state_controller()->FlushTrayActionForTesting();
  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer()->observed_states()[0]);
  observer()->ClearObservedStates();
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, tray_action()->observed_states()[0]);
  tray_action()->ClearObservedStates();

  app_manager()->UpdateApp("", false);

  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());
  state_controller()->FlushTrayActionForTesting();
  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kNotAvailable, observer()->observed_states()[0]);
  observer()->ClearObservedStates();
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kNotAvailable,
            tray_action()->observed_states()[0]);
  tray_action()->ClearObservedStates();

  app_manager()->UpdateApp("app_id_1", true);

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
  state_controller()->FlushTrayActionForTesting();
  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, observer()->observed_states()[0]);
  observer()->ClearObservedStates();
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kAvailable, tray_action()->observed_states()[0]);
  tray_action()->ClearObservedStates();
}

TEST_F(LockScreenAppStateTest, MoveToBackgroundFromActive) {
  ASSERT_TRUE(EnableNoteTakingApp(true /* enable_app_launch */));

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

TEST_F(LockScreenAppStateTest, MoveToForeground) {
  ASSERT_TRUE(EnableNoteTakingApp(true /* enable_app_launch */));

  state_controller()->SetLockScreenNoteStateForTesting(
      TrayActionState::kBackground);

  state_controller()->MoveToForeground();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());

  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kActive, observer()->observed_states()[0]);
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kActive, tray_action()->observed_states()[0]);
}

TEST_F(LockScreenAppStateTest, MoveToForegroundFromNonBackgroundState) {
  ASSERT_TRUE(EnableNoteTakingApp(true /* enable_app_launch */));

  state_controller()->MoveToForeground();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());

  EXPECT_EQ(0u, observer()->observed_states().size());
  EXPECT_EQ(0u, tray_action()->observed_states().size());
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
  ASSERT_TRUE(EnableNoteTakingApp(true /* enable_app_launch */));

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kLaunching,
            state_controller()->GetLockScreenNoteState());
  ASSERT_EQ(1u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kLaunching, observer()->observed_states()[0]);
  observer()->ClearObservedStates();
  ASSERT_EQ(1u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kLaunching, tray_action()->observed_states()[0]);
  tray_action()->ClearObservedStates();

  EXPECT_EQ(1, app_manager()->launch_count());

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  // There should be no state change - the state_controller was already in
  // launching state when the request was received.
  EXPECT_EQ(0u, observer()->observed_states().size());
  EXPECT_EQ(0u, tray_action()->observed_states().size());
  EXPECT_EQ(1, app_manager()->launch_count());
}

TEST_F(LockScreenAppStateTest, HandleActionWithLaunchFailure) {
  ASSERT_TRUE(EnableNoteTakingApp(false /* enable_app_launch */));

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
  ASSERT_EQ(2u, observer()->observed_states().size());
  EXPECT_EQ(TrayActionState::kLaunching, observer()->observed_states()[0]);
  EXPECT_EQ(TrayActionState::kAvailable, observer()->observed_states()[1]);
  observer()->ClearObservedStates();
  ASSERT_EQ(2u, tray_action()->observed_states().size());
  EXPECT_EQ(TrayActionState::kLaunching, tray_action()->observed_states()[0]);
  EXPECT_EQ(TrayActionState::kAvailable, tray_action()->observed_states()[1]);
  tray_action()->ClearObservedStates();

  EXPECT_EQ(1, app_manager()->launch_count());

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());

  // There should be no state change - the state_controller was already in
  // launching state when the request was received.
  EXPECT_EQ(2u, observer()->observed_states().size());
  EXPECT_EQ(2u, tray_action()->observed_states().size());
  EXPECT_EQ(2, app_manager()->launch_count());
}
