// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/interfaces/tray_action.mojom.h"
#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/lock_screen_apps/app_manager.h"
#include "chrome/browser/chromeos/lock_screen_apps/focus_cycler_delegate.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_observer.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_session.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_item_storage.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_contents.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/test/device_data_manager_test_api.h"

using ash::mojom::TrayActionState;
using extensions::DictionaryBuilder;
using extensions::ListBuilder;
using extensions::lock_screen_data::LockScreenItemStorage;

namespace {

// App IDs used for test apps.
const char kTestAppId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kSecondaryTestAppId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

// The primary tesing profile.
const char kPrimaryProfileName[] = "primary_profile";

// Key for pref containing lock screen data crypto key.
constexpr char kDataCryptoKeyPref[] = "lockScreenAppDataCryptoKey";

std::unique_ptr<arc::ArcSession> ArcSessionFactory() {
  ADD_FAILURE() << "Attempt to create arc session.";
  return nullptr;
}

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

class TestFocusCyclerDelegate : public lock_screen_apps::FocusCyclerDelegate {
 public:
  TestFocusCyclerDelegate() = default;
  ~TestFocusCyclerDelegate() override = default;

  void RegisterLockScreenAppFocusHandler(
      const LockScreenAppFocusCallback& handler) override {
    focus_handler_ = handler;
    lock_screen_app_focused_ = true;
  }

  void UnregisterLockScreenAppFocusHandler() override {
    ASSERT_FALSE(focus_handler_.is_null());
    focus_handler_.Reset();
  }

  void HandleLockScreenAppFocusOut(bool reverse) override {
    ASSERT_FALSE(focus_handler_.is_null());
    lock_screen_app_focused_ = false;
  }

  void RequestAppFocus(bool reverse) {
    ASSERT_FALSE(focus_handler_.is_null());
    lock_screen_app_focused_ = true;
    focus_handler_.Run(reverse);
  }

  bool HasHandler() const { return !focus_handler_.is_null(); }

  bool lock_screen_app_focused() const { return lock_screen_app_focused_; }

 private:
  bool lock_screen_app_focused_ = false;
  LockScreenAppFocusCallback focus_handler_;

  DISALLOW_COPY_AND_ASSIGN(TestFocusCyclerDelegate);
};

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
  void ResetLaunchCount() { launch_count_ = 0; }

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
    command_line_->GetProcessCommandLine()->InitFromArgv(
        {"", "--disable-lock-screen-apps"});
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
    command_line_->GetProcessCommandLine()->InitFromArgv({""});

    ASSERT_TRUE(profile_manager_.SetUp());

    auto power_client = base::MakeUnique<chromeos::FakePowerManagerClient>();
    power_manager_client_ = power_client.get();
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    dbus_setter->SetPowerManagerClient(std::move(power_client));

    BrowserWithTestWindowTest::SetUp();

    session_manager_ = base::MakeUnique<session_manager::SessionManager>();
    session_manager_->SessionStarted();
    session_manager_->SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);

    // Initialize arc session manager - NoteTakingHelper expects it to be set.
    arc_session_manager_ = base::MakeUnique<arc::ArcSessionManager>(
        base::MakeUnique<arc::ArcSessionRunner>(
            base::Bind(&ArcSessionFactory)));

    chromeos::NoteTakingHelper::Initialize();

    ASSERT_TRUE(lock_screen_apps::StateController::IsEnabled());

    // Create fake lock screen app profile.
    lock_screen_profile_ = profile_manager_.CreateTestingProfile(
        chromeos::ProfileHelper::GetLockScreenAppProfileName());

    InitExtensionSystem(profile());
    InitExtensionSystem(lock_screen_profile());

    std::unique_ptr<TestAppManager> app_manager =
        base::MakeUnique<TestAppManager>(
            profile(), lock_screen_profile()->GetOriginalProfile());
    app_manager_ = app_manager.get();

    focus_cycler_delegate_ = base::MakeUnique<TestFocusCyclerDelegate>();

    state_controller_ = base::MakeUnique<lock_screen_apps::StateController>();
    state_controller_->SetTrayActionPtrForTesting(
        tray_action_.CreateInterfacePtrAndBind());
    state_controller_->SetAppManagerForTesting(std::move(app_manager));
    state_controller_->SetReadyCallbackForTesting(ready_waiter_.QuitClosure());
    state_controller_->Initialize();
    state_controller_->FlushTrayActionForTesting();
    state_controller_->SetFocusCyclerDelegate(focus_cycler_delegate_.get());

    state_controller_->AddObserver(&observer_);
  }

  void TearDown() override {
    extensions::ExtensionSystem::Get(profile())->Shutdown();
    extensions::ExtensionSystem::Get(lock_screen_profile())->Shutdown();

    state_controller_->RemoveObserver(&observer_);
    state_controller_->Shutdown();
    chromeos::NoteTakingHelper::Shutdown();

    session_manager_.reset();
    app_manager_ = nullptr;
    app_window_.reset();
    BrowserWithTestWindowTest::TearDown();
    DestroyProfile(lock_screen_profile());
    focus_cycler_delegate_.reset();
  }

  TestingProfile* CreateProfile() override {
    return profile_manager_.CreateTestingProfile(kPrimaryProfileName);
  }

  void DestroyProfile(TestingProfile* test_profile) override {
    if (test_profile == profile()) {
      profile_manager_.DeleteTestingProfile(kPrimaryProfileName);
    } else if (test_profile == lock_screen_profile()) {
      lock_screen_profile_ = nullptr;
      profile_manager_.DeleteTestingProfile(
          chromeos::ProfileHelper::GetLockScreenAppProfileName());
    } else {
      ADD_FAILURE() << "Request to destroy unknown profile.";
    }
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

  void SetPrimaryProfileAndWaitUntilReady() {
    state_controller_->SetPrimaryProfile(profile());
    ready_waiter_.Run();
  }

  // Helper method to move state controller to the specified state.
  // Should be called at the begining of tests, at most once.
  bool InitializeNoteTakingApp(TrayActionState target_state,
                               bool enable_app_launch) {
    app_ = CreateTestNoteTakingApp(kTestAppId);
    extensions::ExtensionSystem::Get(lock_screen_profile())
        ->extension_service()
        ->AddExtension(app_.get());

    app_manager_->SetInitialAppState(kTestAppId, enable_app_launch);
    SetPrimaryProfileAndWaitUntilReady();

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
    app_manager_->ResetLaunchCount();

    if (target_state == TrayActionState::kLaunching)
      return true;

    app_window_ = CreateNoteTakingWindow(lock_screen_profile(), app());
    if (!app_window_->window()) {
      ADD_FAILURE() << "Not allowed to create app window.";
      return false;
    }

    app_window()->Initialize(true /* shown */);

    ClearObservedStates();

    return state_controller()->GetLockScreenNoteState() ==
           TrayActionState::kActive;
  }

  TestingProfile* lock_screen_profile() { return lock_screen_profile_; }

  chromeos::FakePowerManagerClient* power_manager_client() {
    return power_manager_client_;
  }

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

  TestFocusCyclerDelegate* focus_cycler_delegate() {
    return focus_cycler_delegate_.get();
  }

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  TestingProfileManager profile_manager_;
  TestingProfile* lock_screen_profile_ = nullptr;

  // Run loop used to throttle test until async state controller initialization
  // is fully complete. The quit closure for this run loop will be passed to
  // |state_controller_| as the callback to be run when the state controller is
  // ready for action.
  // NOTE: Tests should call |state_controller_->SetPrimaryProfile(Profile*)|
  // before running the loop, as that is the method that starts the state
  // controller.
  base::RunLoop ready_waiter_;

  // Power manager client set by the test - the power manager client instance is
  // owned by DBusThreadManager.
  chromeos::FakePowerManagerClient* power_manager_client_ = nullptr;

  // The StateController does not really have dependency on ARC, but this is
  // needed to properly initialize NoteTakingHelper.
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;

  std::unique_ptr<session_manager::SessionManager> session_manager_;

  std::unique_ptr<lock_screen_apps::StateController> state_controller_;

  std::unique_ptr<TestFocusCyclerDelegate> focus_cycler_delegate_;

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
  SetPrimaryProfileAndWaitUntilReady();

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
  SetPrimaryProfileAndWaitUntilReady();

  ASSERT_EQ(TestAppManager::State::kStarted, app_manager()->state());

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());

  ExpectObservedStatesMatch({TrayActionState::kAvailable}, "Available on lock");
}

TEST_F(LockScreenAppStateTest, InitLockScreenDataLockScreenItemStorage) {
  EXPECT_EQ(TestAppManager::State::kNotInitialized, app_manager()->state());
  SetPrimaryProfileAndWaitUntilReady();

  LockScreenItemStorage* lock_screen_item_storage =
      LockScreenItemStorage::GetIfAllowed(profile());
  ASSERT_TRUE(lock_screen_item_storage);

  std::string crypto_key_in_prefs =
      profile()->GetPrefs()->GetString(kDataCryptoKeyPref);
  ASSERT_FALSE(crypto_key_in_prefs.empty());
  ASSERT_TRUE(base::Base64Decode(crypto_key_in_prefs, &crypto_key_in_prefs));

  EXPECT_EQ(crypto_key_in_prefs,
            lock_screen_item_storage->crypto_key_for_testing());

  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_FALSE(LockScreenItemStorage::GetIfAllowed(profile()));
  EXPECT_TRUE(LockScreenItemStorage::GetIfAllowed(lock_screen_profile()));
}

TEST_F(LockScreenAppStateTest,
       InitLockScreenDataLockScreenItemStorageWhileLocked) {
  EXPECT_EQ(TestAppManager::State::kNotInitialized, app_manager()->state());
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  SetPrimaryProfileAndWaitUntilReady();

  EXPECT_FALSE(LockScreenItemStorage::GetIfAllowed(profile()));

  LockScreenItemStorage* lock_screen_item_storage =
      LockScreenItemStorage::GetIfAllowed(lock_screen_profile());
  ASSERT_TRUE(lock_screen_item_storage);

  std::string crypto_key_in_prefs =
      profile()->GetPrefs()->GetString(kDataCryptoKeyPref);
  ASSERT_FALSE(crypto_key_in_prefs.empty());
  ASSERT_TRUE(base::Base64Decode(crypto_key_in_prefs, &crypto_key_in_prefs));

  EXPECT_EQ(crypto_key_in_prefs,
            lock_screen_item_storage->crypto_key_for_testing());

  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);

  EXPECT_TRUE(LockScreenItemStorage::GetIfAllowed(profile()));
  EXPECT_FALSE(LockScreenItemStorage::GetIfAllowed(lock_screen_profile()));
}

TEST_F(LockScreenAppStateTest,
       InitLockScreenDataLockScreenItemStorage_CryptoKeyExists) {
  std::string crypto_key_in_prefs = "0123456789ABCDEF0123456789ABCDEF";
  std::string crypto_key_in_prefs_encoded;
  base::Base64Encode(crypto_key_in_prefs, &crypto_key_in_prefs_encoded);

  profile()->GetPrefs()->SetString(kDataCryptoKeyPref,
                                   crypto_key_in_prefs_encoded);

  SetPrimaryProfileAndWaitUntilReady();

  LockScreenItemStorage* lock_screen_item_storage =
      LockScreenItemStorage::GetIfAllowed(profile());
  ASSERT_TRUE(lock_screen_item_storage);

  EXPECT_EQ(crypto_key_in_prefs,
            lock_screen_item_storage->crypto_key_for_testing());

  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_FALSE(LockScreenItemStorage::GetIfAllowed(profile()));
  EXPECT_TRUE(LockScreenItemStorage::GetIfAllowed(lock_screen_profile()));
}

TEST_F(LockScreenAppStateTest, SessionLock) {
  app_manager()->SetInitialAppState(kTestAppId, true);
  SetPrimaryProfileAndWaitUntilReady();
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
  SetPrimaryProfileAndWaitUntilReady();
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
  SetPrimaryProfileAndWaitUntilReady();
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
  SetPrimaryProfileAndWaitUntilReady();
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

// TODO(tbarzic): Remove or rewrite this test after refactoring
// foreground/background code.
TEST_F(LockScreenAppStateTest, DISABLED_MoveToBackgroundAndForeground) {
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

TEST_F(LockScreenAppStateTest, MoveToBackgroundWhileLaunching) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kLaunching,
                                      true /* enable_app_launch */));

  state_controller()->MoveToBackground();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());

  EXPECT_FALSE(state_controller()->CreateAppWindowForLockScreenAction(
      profile(), app(), extensions::api::app_runtime::ACTION_TYPE_NEW_NOTE,
      base::MakeUnique<ChromeAppDelegate>(true)));

  ExpectObservedStatesMatch({TrayActionState::kAvailable},
                            "Move to background cancels launch.");
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

TEST_F(LockScreenAppStateTest, LaunchActionWhenStylusGetsRemoved) {
  ui::test::DeviceDataManagerTestAPI devices_test_api;
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kAvailable,
                                      true /* enable_app_launch */));
  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);

  ExpectObservedStatesMatch({TrayActionState::kLaunching},
                            "Launch on new note request");
  ClearObservedStates();
  EXPECT_EQ(1, app_manager()->launch_count());

  // If the stylus is inserted and removed while launching the app, there should
  // be no new launch event.
  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::INSERTED);
  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);
  EXPECT_EQ(0u, observer()->observed_states().size());
  EXPECT_EQ(0u, tray_action()->observed_states().size());
  EXPECT_EQ(1, app_manager()->launch_count());
}

TEST_F(LockScreenAppStateTest, StylusRemovedBeforeScreenLock) {
  ui::test::DeviceDataManagerTestAPI devices_test_api;
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kNotAvailable,
                                      true /* enable_app_launch */));

  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

  // Stylus removed event should be ignored if it came before note taking on
  // lock screen was available (in this case due to session being active).
  // Screen unlock should still make lock screen note taking available.
  ExpectObservedStatesMatch({TrayActionState::kAvailable},
                            "Remove stylus, then unlock.");

  ClearObservedStates();
  EXPECT_EQ(0, app_manager()->launch_count());
}

TEST_F(LockScreenAppStateTest, StylusRemovedWhileActive) {
  ui::test::DeviceDataManagerTestAPI devices_test_api;
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kActive,
                                      true /* enable_app_launch */));

  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);

  EXPECT_EQ(0u, observer()->observed_states().size());
  EXPECT_EQ(0u, tray_action()->observed_states().size());

  ClearObservedStates();
  EXPECT_EQ(0, app_manager()->launch_count());
}

TEST_F(LockScreenAppStateTest, StylusRemovedWhileInBackground) {
  ui::test::DeviceDataManagerTestAPI devices_test_api;
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kBackground,
                                      true /* enable_app_launch */));

  devices_test_api.NotifyObserversStylusStateChanged(ui::StylusState::REMOVED);

  EXPECT_EQ(0u, observer()->observed_states().size());
  EXPECT_EQ(0u, tray_action()->observed_states().size());

  ClearObservedStates();
  EXPECT_EQ(0, app_manager()->launch_count());
}

TEST_F(LockScreenAppStateTest, AppWindowRegistration) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kAvailable,
                                      true /* enable_app_launch */));

  std::unique_ptr<TestAppWindow> app_window =
      CreateNoteTakingWindow(lock_screen_profile(), app());
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
      lock_screen_profile(), app(),
      extensions::api::app_runtime::ACTION_TYPE_NONE,
      base::MakeUnique<ChromeAppDelegate>(true)));

  app_window = CreateNoteTakingWindow(lock_screen_profile(), app());
  ASSERT_TRUE(app_window->window());

  app_window->Initialize(true /* shown */);
  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());

  // Test that second app window cannot be registered.
  std::unique_ptr<TestAppWindow> second_app_window =
      CreateNoteTakingWindow(lock_screen_profile(), app());
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
      CreateNoteTakingWindow(lock_screen_profile(), app());
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

TEST_F(LockScreenAppStateTest, CloseAppWindowOnSuspend) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kActive,
                                      true /* enable_app_launch */));

  power_manager_client()->SendSuspendImminent();
  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(app_window()->closed());
}

TEST_F(LockScreenAppStateTest, CloseAppWindowOnScreenOff) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kActive,
                                      true /* enable_app_launch */));

  power_manager_client()->SendBrightnessChanged(10 /* level */,
                                                true /* user_initiated */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(app_window()->closed());
  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());

  power_manager_client()->SendBrightnessChanged(0 /* level */,
                                                true /* user_initiated */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(app_window()->closed());
  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());

  power_manager_client()->SendBrightnessChanged(10 /* level */,
                                                false /* user_initiated */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(app_window()->closed());
  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());

  power_manager_client()->SendBrightnessChanged(0 /* level */,
                                                false /* user_initiated */);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(app_window()->closed());
  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());
}

TEST_F(LockScreenAppStateTest, AppWindowClosedOnAppUnload) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kActive,
                                      true /* enable_app_launch */));

  extensions::ExtensionSystem::Get(lock_screen_profile())
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
  extensions::ExtensionSystem::Get(lock_screen_profile())
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
      CreateNoteTakingWindow(lock_screen_profile(), app());
  EXPECT_FALSE(app_window->window());
  ASSERT_EQ(TrayActionState::kLaunching,
            state_controller()->GetLockScreenNoteState());

  std::unique_ptr<TestAppWindow> secondary_app_window =
      CreateNoteTakingWindow(lock_screen_profile(), secondary_app.get());
  ASSERT_TRUE(secondary_app_window->window());

  secondary_app_window->Initialize(true /* shown*/);
  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(secondary_app_window->closed());

  // Uninstall the app and test the secondary app window is closed.
  extensions::ExtensionSystem::Get(lock_screen_profile())
      ->extension_service()
      ->UnloadExtension(secondary_app->id(),
                        extensions::UnloadedExtensionReason::UNINSTALL);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(secondary_app_window->closed());
}

// Goes through different states with no focus cycler set; mainly to check
// there are no crashes.
TEST_F(LockScreenAppStateTest, NoFocusCyclerDelegate) {
  lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(nullptr);

  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kActive,
                                      true /* enable_app_launch */));

  state_controller()->MoveToBackground();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());

  state_controller()->MoveToForeground();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_EQ(TrayActionState::kAvailable,
            state_controller()->GetLockScreenNoteState());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(app_window()->closed());
}

TEST_F(LockScreenAppStateTest, ResetFocusCyclerDelegateWhileActive) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kActive,
                                      true /* enable_app_launch */));

  lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(nullptr);
  ASSERT_FALSE(focus_cycler_delegate()->HasHandler());

  lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(
      focus_cycler_delegate());
  EXPECT_TRUE(focus_cycler_delegate()->HasHandler());
}

TEST_F(LockScreenAppStateTest, FocusCyclerDelegateGetsSetOnAppWindowCreation) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kAvailable,
                                      true /* enable_app_launch */));

  tray_action()->SendNewNoteRequest();
  state_controller()->FlushTrayActionForTesting();

  EXPECT_FALSE(focus_cycler_delegate()->HasHandler());

  std::unique_ptr<TestAppWindow> app_window =
      CreateNoteTakingWindow(lock_screen_profile(), app());
  app_window->Initialize(true /* shown */);

  EXPECT_TRUE(focus_cycler_delegate()->HasHandler());

  state_controller()->MoveToBackground();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(focus_cycler_delegate()->HasHandler());
  EXPECT_TRUE(app_window->closed());
}

TEST_F(LockScreenAppStateTest, TakeFocus) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kActive,
                                      true /* enable_app_launch */));

  auto regular_app_window = base::MakeUnique<TestAppWindow>(
      profile(),
      new extensions::AppWindow(profile(), new ChromeAppDelegate(true), app()));
  EXPECT_FALSE(state_controller()->HandleTakeFocus(
      regular_app_window->window()->web_contents(), true));
  EXPECT_TRUE(focus_cycler_delegate()->lock_screen_app_focused());

  ASSERT_TRUE(state_controller()->HandleTakeFocus(
      app_window()->window()->web_contents(), true));
  EXPECT_FALSE(focus_cycler_delegate()->lock_screen_app_focused());

  focus_cycler_delegate()->RequestAppFocus(true);
  EXPECT_TRUE(focus_cycler_delegate()->lock_screen_app_focused());
}

// TODO(tbarzic): Remove or rewrite this test after refactoring
// foreground/background code.
TEST_F(LockScreenAppStateTest,
       DISABLED_RequestFocusFromBackgroundMovesAppToForeground) {
  ASSERT_TRUE(InitializeNoteTakingApp(TrayActionState::kActive,
                                      true /* enable_app_launch */));

  ASSERT_TRUE(state_controller()->HandleTakeFocus(
      app_window()->window()->web_contents(), true));
  EXPECT_FALSE(focus_cycler_delegate()->lock_screen_app_focused());

  state_controller()->MoveToBackground();

  EXPECT_EQ(TrayActionState::kBackground,
            state_controller()->GetLockScreenNoteState());

  focus_cycler_delegate()->RequestAppFocus(true);
  EXPECT_TRUE(focus_cycler_delegate()->lock_screen_app_focused());

  EXPECT_EQ(TrayActionState::kActive,
            state_controller()->GetLockScreenNoteState());
}
