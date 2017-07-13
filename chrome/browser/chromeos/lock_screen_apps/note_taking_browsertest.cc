// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/launcher.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace {

const char kTestAppId[] = "cadfeochfldmbdgoccgbeianhamecbae";

class LockScreenNoteTakingTest : public ExtensionBrowserTest {
 public:
  LockScreenNoteTakingTest() { set_chromeos_user_ = true; }
  ~LockScreenNoteTakingTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd_line) override {
    cmd_line->AppendSwitchASCII(extensions::switches::kWhitelistedExtensionID,
                                kTestAppId);
    cmd_line->AppendSwitch(chromeos::switches::kEnableLockScreenApps);

    ExtensionBrowserTest::SetUpCommandLine(cmd_line);
  }

  bool EnableLockScreenAppLaunch(const std::string& app_id) {
    chromeos::NoteTakingHelper::Get()->SetPreferredApp(profile(), app_id);
    profile()->GetPrefs()->SetBoolean(prefs::kNoteTakingAppEnabledOnLockScreen,
                                      true);
    session_manager::SessionManager::Get()->SetSessionState(
        session_manager::SessionState::LOCKED);

    return lock_screen_apps::StateController::Get()->GetLockScreenNoteState() ==
           ash::mojom::TrayActionState::kAvailable;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LockScreenNoteTakingTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(LockScreenNoteTakingTest, Launch) {
  ASSERT_TRUE(lock_screen_apps::StateController::IsEnabled());

  scoped_refptr<const extensions::Extension> app =
      LoadExtension(test_data_dir_.AppendASCII("lock_screen_apps/app_launch"));
  ASSERT_TRUE(app);
  ASSERT_TRUE(EnableLockScreenAppLaunch(app->id()));

  // The test app will send "readyToClose" message from the app window created
  // as part of the test. The message will be sent after the tests in the app
  // window context have been run and the window is ready to be closed.
  // The test should reply to this message in order for the app window to close
  // itself.
  ExtensionTestMessageListener ready_to_close("readyToClose",
                                              true /* will_reply */);

  extensions::ResultCatcher catcher;
  lock_screen_apps::StateController::Get()->RequestNewLockScreenNote();

  ASSERT_EQ(ash::mojom::TrayActionState::kLaunching,
            lock_screen_apps::StateController::Get()->GetLockScreenNoteState());

  // The test will run two sets of tests:
  // *  in the window that gets created as the response to the new_note action
  //    launch
  // *  in the app background page - the test will launch an app window and wait
  //    for it to be closed
  // Test runner should wait for both of those to finish (test result message
  // will be sent for each set of tests).
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ASSERT_EQ(ash::mojom::TrayActionState::kActive,
            lock_screen_apps::StateController::Get()->GetLockScreenNoteState());

  ASSERT_TRUE(ready_to_close.WaitUntilSatisfied());
  // Close the app window created by the API test.
  ready_to_close.Reply("close");

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  EXPECT_EQ(ash::mojom::TrayActionState::kAvailable,
            lock_screen_apps::StateController::Get()->GetLockScreenNoteState());
}

// Tests that lock screen app window creation fails if not requested from the
// lock screen context - the test app runs tests as a response to a launch event
// in the user's profile (rather than the lock screen profile).
IN_PROC_BROWSER_TEST_F(LockScreenNoteTakingTest, LaunchInNonLockScreenContext) {
  ASSERT_TRUE(lock_screen_apps::StateController::IsEnabled());

  scoped_refptr<const extensions::Extension> app = LoadExtension(
      test_data_dir_.AppendASCII("lock_screen_apps/non_lock_screen_context"));
  ASSERT_TRUE(app);
  ASSERT_TRUE(EnableLockScreenAppLaunch(app->id()));

  extensions::ResultCatcher catcher;

  // Get the lock screen apps state controller to the state where lock screen
  // enabled app window creation is allowed (provided the window is created
  // from a lock screen context).
  // NOTE: This is not mandatory for the test to pass, but without it, app
  //     window creation would fail regardless of the context from which
  //     chrome.app.window.create is called.
  lock_screen_apps::StateController::Get()->RequestNewLockScreenNote();
  ASSERT_EQ(ash::mojom::TrayActionState::kLaunching,
            lock_screen_apps::StateController::Get()->GetLockScreenNoteState());

  // Launch note taking in regular, non lock screen context. The test will
  // verify the app cannot create lock screen enabled app windows in this case.
  auto action_data =
      base::MakeUnique<extensions::api::app_runtime::ActionData>();
  action_data->action_type =
      extensions::api::app_runtime::ActionType::ACTION_TYPE_NEW_NOTE;
  apps::LaunchPlatformAppWithAction(profile(), app.get(),
                                    std::move(action_data), base::FilePath());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}
