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
#include "base/test/scoped_command_line.h"
#include "chrome/browser/chromeos/lock_screen_apps/app_manager.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_contents.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::mojom::TrayActionState;
using extensions::DictionaryBuilder;
using extensions::ListBuilder;

namespace {

// App IDs used for test apps.
const char kTestAppId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kSecondaryTestAppId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

scoped_refptr<extensions::Extension> CreateTestNoteTakingApp(
    const std::string& app_id) {
  ListBuilder action_handlers;
  action_handlers.Append(DictionaryBuilder()
                             .Set("action", "new_note")
                             .SetBoolean("enabled_on_lock_screen", true)
                             .Build());
  DictionaryBuilder background;
  background.Set("scripts", ListBuilder().Append("background.js").Build());
  return extensions::ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                       .Set("name", "Test App")
                       .Set("version", "1.0")
                       .Set("manifest_version", 2)
                       .Set("app", DictionaryBuilder()
                                       .Set("background", background.Build())
                                       .Build())
                       .Set("action_handlers", action_handlers.Build())
                       .Build())
      .SetID(app_id)
      .Build();
}

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

// Wrapper around AppWindow used to manage the app window lifetime, and provide
// means to initialize/close the window,
class TestAppWindow : public content::WebContentsObserver {
 public:
  TestAppWindow(Profile* profile, extensions::AppWindow* window)
      : web_contents_(
            content::WebContentsTester::CreateTestWebContents(profile,
                                                              nullptr)),
        window_(window) {}

  ~TestAppWindow() override {
    // Make sure the window is initialized, so |window_| does not get leaked.
    if (!initialized_ && window_)
      Initialize(false /* shown */);

    Close();
  }

  void Initialize(bool shown) {
    ASSERT_FALSE(initialized_);
    ASSERT_TRUE(window_);
    initialized_ = true;

    extensions::AppWindow::CreateParams params;
    params.hidden = !shown;
    window_->Init(GURL(), new extensions::AppWindowContentsImpl(window_),
                  web_contents_->GetMainFrame(), params);
    Observe(window_->web_contents());
  }

  void Close() {
    if (!window_)
      return;

    if (!initialized_)
      return;

    content::WebContentsDestroyedWatcher destroyed_watcher(
        window_->web_contents());
    window_->GetBaseWindow()->Close();
    destroyed_watcher.Wait();

    EXPECT_FALSE(window_);
    EXPECT_TRUE(closed_);
  }

  void WebContentsDestroyed() override {
    closed_ = true;
    window_ = nullptr;
  }

  extensions::AppWindow* window() { return window_; }

  bool closed() const { return closed_; }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  extensions::AppWindow* window_;
  bool closed_ = false;
  bool initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestAppWindow);
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

class LockScreenAppStateTest : public BrowserWithTestWindowTest {
 public:
  LockScreenAppStateTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~LockScreenAppStateTest() override = default;

  void SetUp() override {
    command_line_ = base::MakeUnique<base::test::ScopedCommandLine>();
    command_line_->GetProcessCommandLine()->InitFromArgv(
        {"", "--enable-lock-screen-apps"});

    ASSERT_TRUE(profile_manager_.SetUp());

    BrowserWithTestWindowTest::SetUp();

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
    signin_profile_ = builder.BuildIncognito(
        profile_manager_.CreateTestingProfile(chrome::kInitialProfile));

    InitExtensionSystem(profile());
    InitExtensionSystem(signin_profile());

    std::unique_ptr<TestAppManager> app_manager =
        base::MakeUnique<TestAppManager>(&profile_,
                                         signin_profile_->GetOriginalProfile());
    app_manager_ = app_manager.get();
    state_controller_->SetAppManagerForTesting(std::move(app_manager));
  }

  void TearDown() override {
    extensions::ExtensionSystem::Get(profile())->Shutdown();
    extensions::ExtensionSystem::Get(signin_profile())->Shutdown();

    state_controller_->RemoveObserver(&observer_);
    state_controller_.reset();
    session_manager_.reset();
    app_manager_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  void InitExtensionSystem(Profile* profile) {
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        base::FilePath() /* install_directory */,
        false /* autoupdate_enabled */);
  }

  void ExpectObservedStatesMatch(const std::vector<TrayActionState>& states,
                                 const std::string& message) {
    state_controller_->FlushTrayActionForTesting();
    EXPECT_EQ(states, observer()->observed_states()) << message;
    EXPECT_EQ(states, tray_action()->observed_states()) << message;
  }

  // Helper method to create and register an app window for lock screen note
  // taking action with the state controller.
  // Note that app window creation may fail if the app is not allowed to create
  // the app window for the action - in that case returned |TestAppWindow| will
  // have null |window| (rather than being null itself).
  std::unique_ptr<TestAppWindow> CreateNoteTakingWindow(
      Profile* profile,
      const extensions::Extension* extension) {
    return base::MakeUnique<TestAppWindow>(
        profile, state_controller()->CreateAppWindowForLockScreenAction(
                     profile, extension,
                     extensions::api::app_runtime::ACTION_TYPE_NEW_NOTE,
                     base::MakeUnique<ChromeAppDelegate>(true)));
  }

  void ClearObservedStates() {
    state_controller_->FlushTrayActionForTesting();
    observer_.ClearObservedStates();
    tray_action_.ClearObservedStates();
  }

  // Helper method to move state controller to the specified state.
  // Should be called at the begining of tests, at most once.
  bool InitializeNoteTakingApp(TrayActionState target_state,
                               bool enable_app_launch) {
    app_ = CreateTestNoteTakingApp(kTestAppId);
    extensions::ExtensionSystem::Get(signin_profile())
        ->extension_service()
        ->AddExtension(app_.get());

    app_manager_->SetInitialAppState(kTestAppId, enable_app_launch);
    state_controller_->SetPrimaryProfile(&profile_);

    if (target_state == TrayActionState::kNotAvailable)
      return true;

    session_manager_->SetSessionState(session_manager::SessionState::LOCKED);
    if (app_manager_->state() != TestAppManager::State::kStarted) {
      ADD_FAILURE() << "Lock app manager Start not invoked.";
      return false;
    }

    ClearObservedStates();

    if (state_controller_->GetLockScreenNoteState() !=
        TrayActionState::kAvailable) {
      ADD_FAILURE() << "Unable to move to available state.";
      return false;
    }
    if (target_state == TrayActionState::kAvailable)
      return true;

    tray_action()->SendNewNoteRequest();
    state_controller_->FlushTrayActionForTesting();

    ClearObservedStates();

    if (state_controller_->GetLockScreenNoteState() !=
        TrayActionState::kLaunching) {
      ADD_FAILURE() << "Unable to move to launching state.";
      return false;
    }
    if (target_state == TrayActionState::kLaunching)
      return true;

    app_window_ = CreateNoteTakingWindow(signin_profile(), app());
    if (!app_window_->window()) {
      ADD_FAILURE() << "Not allowed to create app window.";
      return false;
    }

    app_window()->Initialize(true /* shown */);

    ClearObservedStates();

    return state_controller()->GetLockScreenNoteState() ==
           TrayActionState::kActive;
  }

  TestingProfile* profile() { return &profile_; }
  TestingProfile* signin_profile() { return signin_profile_; }

  session_manager::SessionManager* session_manager() {
    return session_manager_.get();
  }

  TestStateObserver* observer() { return &observer_; }

  TestTrayAction* tray_action() { return &tray_action_; }

  lock_screen_apps::StateController* state_controller() {
    return state_controller_.get();
  }

  TestAppManager* app_manager() { return app_manager_; }

  TestAppWindow* app_window() { return app_window_.get(); }
  const extensions::Extension* app() { return app_.get(); }

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  TestingProfileManager profile_manager_;
  TestingProfile profile_;
  TestingProfile* signin_profile_ = nullptr;

  std::unique_ptr<session_manager::SessionManager> session_manager_;

  std::unique_ptr<lock_screen_apps::StateController> state_controller_;

  TestStateObserver observer_;
  TestTrayAction tray_action_;
  TestAppManager* app_manager_ = nullptr;

  std::unique_ptr<TestAppWindow> app_window_;
  scoped_refptr<const extensions::Extension> app_;

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

  app_manager()->SetInitialAppState(kTestAppId, true);
  state_controller()->SetPrimaryProfile(profile());

  ASSERT_EQ(TestAppManager::State::kStarted, app_manager()->state());

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());

  ExpectObservedStatesMatch({TrayActionState::kAvailable}, "Available on lock");
}

TEST_F(LockScreenAppStateTest, SessionLock) {
  app_manager()->SetInitialAppState(kTestAppId, true);
  state_controller()->SetPrimaryProfile(profile());
  ASSERT_EQ(TestAppManager::State::kStopped, app_manager()->state());

  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

  ASSERT_EQ(TestAppManager::State::kStarted, app_manager()->state());

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());

  ExpectObservedStatesMatch({TrayActionState::kAvailable}, "Available on lock");
  ClearObservedStates();

  // When the session is unlocked again, app manager is stopped, and tray action
  // disabled again.
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);

  EXPECT_EQ(TestAppManager::State::kStopped, app_manager()->state());

  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());

  ExpectObservedStatesMatch({TrayActionState::kNotAvailable},
                            "Not available on unlock");
  ClearObservedStates();

  // Test that subsequent session lock works as expected.
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  ASSERT_EQ(TestAppManager::State::kStarted, app_manager()->state());
  ExpectObservedStatesMatch({TrayActionState::kAvailable},
                            "Available on second lock");
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
  app_manager()->UpdateApp(kTestAppId, true);

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
  ExpectObservedStatesMatch({TrayActionState::kAvailable}, "Available on lock");
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
  app_manager()->SetInitialAppState(kTestAppId, false);

  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
  ExpectObservedStatesMatch({TrayActionState::kAvailable}, "Available on lock");
}

TEST_F(LockScreenAppStateTest, AppAvailabilityChanges) {
  state_controller()->SetPrimaryProfile(profile());
  ASSERT_EQ(TestAppManager::State::kStopped, app_manager()->state());

  app_manager()->SetInitialAppState(kTestAppId, false);
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_EQ(TestAppManager::State::kStarted, app_manager()->state());

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
  ExpectObservedStatesMatch({TrayActionState::kAvailable}, "Available on lock");
  ClearObservedStates();

  app_manager()->UpdateApp("", false);

  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());
  ExpectObservedStatesMatch({TrayActionState::kNotAvailable},
                            "Not available on app cleared");
  ClearObservedStates();

  app_manager()->UpdateApp(kSecondaryTestAppId, true);

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
  ExpectObservedStatesMatch({TrayActionState::kAvailable},
                            "Available on other app set");
}

TEST_F(LockScreenAppStateTest, MoveToBackgroundAndForeground) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kActive,
                                      true /* enable_app_launch */));

  state_controller()->MoveToBackground();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kBackground,
            state_controller()->GetLockScreenNoteState());

  ExpectObservedStatesMatch({TrayActionState::kBackground},
                            "Move to background");
  ClearObservedStates();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(app_window()->closed());

  state_controller()->MoveToForeground();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());

  ExpectObservedStatesMatch({TrayActionState::kActive}, "Move to foreground");

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(app_window()->closed());
}

TEST_F(LockScreenAppStateTest, MoveToForegroundFromNonBackgroundState) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kAvailable,
                                      true /* enable_app_launch */));

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
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kAvailable,
                                      true /* enable_app_launch */));

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  ExpectObservedStatesMatch({TrayActionState::kLaunching},
                            "Launch on new note request");
  ClearObservedStates();
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
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kAvailable,
                                      false /* enable_app_launch */));

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
  ExpectObservedStatesMatch(
      {TrayActionState::kLaunching, TrayActionState::kAvailable},
      "Failed launch on new note request");
  ClearObservedStates();

  EXPECT_EQ(1, app_manager()->launch_count());

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());

  ExpectObservedStatesMatch(
      {TrayActionState::kLaunching, TrayActionState::kAvailable},
      "Second failed launch on new note request");
  EXPECT_EQ(2, app_manager()->launch_count());
}

TEST_F(LockScreenAppStateTest, AppWindowRegistration) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kAvailable,
                                      true /* enable_app_launch */));

  std::unique_ptr<TestAppWindow> app_window =
      CreateNoteTakingWindow(signin_profile(), app());
  EXPECT_FALSE(app_window->window());

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kLaunching,
            state_controller()->GetLockScreenNoteState());
  observer()->ClearObservedStates();
  tray_action()->ClearObservedStates();

  std::unique_ptr<TestAppWindow> non_eligible_app_window =
      CreateNoteTakingWindow(profile(), app());
  EXPECT_FALSE(non_eligible_app_window->window());

  EXPECT_FALSE(state_controller()->CreateAppWindowForLockScreenAction(
      signin_profile(), app(), extensions::api::app_runtime::ACTION_TYPE_NONE,
      base::MakeUnique<ChromeAppDelegate>(true)));

  app_window = CreateNoteTakingWindow(signin_profile(), app());
  ASSERT_TRUE(app_window->window());

  app_window->Initialize(true /* shown */);
  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());

  // Test that second app window cannot be registered.
  std::unique_ptr<TestAppWindow> second_app_window =
      CreateNoteTakingWindow(signin_profile(), app());
  EXPECT_FALSE(second_app_window->window());

  // Test the app window does not get closed by itself.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(app_window->closed());

  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());

  // Closing the second app window, will not change the state.
  second_app_window->Close();
  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());

  app_window->Close();
  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
}

TEST_F(LockScreenAppStateTest, AppWindowClosedBeforeBeingShown) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kLaunching,
                                      true /* enable_app_launch */));

  std::unique_ptr<TestAppWindow> app_window =
      CreateNoteTakingWindow(signin_profile(), app());
  ASSERT_TRUE(app_window->window());
  app_window->Initialize(false /* shown */);

  app_window->Close();
  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
}

TEST_F(LockScreenAppStateTest, AppWindowClosedOnSessionUnlock) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kActive,
                                      true /* enable_app_launch */));

  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(app_window()->closed());
}

TEST_F(LockScreenAppStateTest, AppWindowClosedOnAppUnload) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kActive,
                                      true /* enable_app_launch */));

  extensions::ExtensionSystem::Get(signin_profile())
      ->extension_service()
      ->UnloadExtension(app()->id(),
                        extensions::UnloadedExtensionReason::UNINSTALL);
  app_manager()->UpdateApp("", false);

  EXPECT_EQ(TrayActionState::kNotAvailable,
            state_controller()->GetLockScreenNoteState());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(app_window()->closed());
}

TEST_F(LockScreenAppStateTest, AppWindowClosedOnNoteTakingAppChange) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kActive,
                                      true /* enable_app_launch */));

  scoped_refptr<extensions::Extension> secondary_app =
      CreateTestNoteTakingApp(kSecondaryTestAppId);
  extensions::ExtensionSystem::Get(signin_profile())
      ->extension_service()
      ->AddExtension(secondary_app.get());

  app_manager()->UpdateApp(secondary_app->id(), true);

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(app_window()->closed());

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  std::unique_ptr<TestAppWindow> app_window =
      CreateNoteTakingWindow(signin_profile(), app());
  EXPECT_FALSE(app_window->window());
  ASSERT_EQ(TrayActionState::kLaunching,
            state_controller()->GetLockScreenNoteState());

  std::unique_ptr<TestAppWindow> secondary_app_window =
      CreateNoteTakingWindow(signin_profile(), secondary_app.get());
  ASSERT_TRUE(secondary_app_window->window());

  secondary_app_window->Initialize(true /* shown*/);
  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(secondary_app_window->closed());

  // Uninstall the app and test the secondary app window is closed.
  extensions::ExtensionSystem::Get(signin_profile())
      ->extension_service()
      ->UnloadExtension(secondary_app->id(),
                        extensions::UnloadedExtensionReason::UNINSTALL);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(secondary_app_window->closed());
}
