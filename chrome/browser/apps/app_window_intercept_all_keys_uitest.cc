// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest-spi.h"

using extensions::NativeAppWindow;

class AppWindowInterceptAllKeysTest
    : public extensions::PlatformAppBrowserTest {
 public:
  // Send key to window but does not wait till the key is actually in the input
  // queue (like ui_test_utils::SendKeyPressToWindowSync()) as key will not be
  // sent to hook when keyboard is intercepted.
  bool SimulateKeyPress(ui::KeyboardCode key,
                        bool control,
                        bool shift,
                        bool alt,
                        bool command) {
    DVLOG(1) << "Sending: " << key << " control = " << control
             << " shift = " << shift << " alt = " << alt
             << " command = " << command;
    if (!ui_controls::SendKeyPressNotifyWhenDone(
            GetFirstAppWindow()->GetNativeWindow(), key, control, shift, alt,
            command, null_callback_)) {
      LOG(WARNING) << "Failed to send key to app";
      failure_message_ = "Failed to send key to app.";
      return false;
    }

    base::RunLoop().RunUntilIdle();
    return !testing::Test::HasFatalFailure();
  }

  bool SimulateKeyPress(ui::KeyboardCode key) {
    return SimulateKeyPress(key, false, false, false, false);
  }

  bool WaitForKeyEvent(ui::KeyboardCode code) {
    std::string key_event = base::StringPrintf("KeyReceived: %d", code);
    ExtensionTestMessageListener key_listener(key_event, false);

    if (!SimulateKeyPress(code)) {
      failure_message_ = "Failed to send key to app";
      return false;
    }

    key_listener.WaitUntilSatisfied();

    DVLOG(1) << "Application ACK-ed keypress";
    return true;
  }

  bool LoadApplication(const char* app_path) {
    DVLOG(1) << "Launching app = " << app_path;
    LoadAndLaunchPlatformApp(app_path, "Launched");

    DVLOG(1) << "Validating that application is in focus";
    // We start by making sure the window is actually focused.
    if (!ui_test_utils::ShowAndFocusNativeWindow(
            GetFirstAppWindow()->GetNativeWindow())) {
      failure_message_ = "App did not get focus.";
      return false;
    }

    DVLOG(1) << "Launched application";
    return true;
  }

  void SendTaskSwitchKeys() {
    // Send switch sequence (currently just for windows - will have to update as
    // more platform support is added).
    SimulateKeyPress(ui::VKEY_TAB, false, false, true, false);
  }

  void ValidateCannotInterceptKeys(const char* app_path,
                                   bool change_intercept,
                                   bool enable_intercept) {
    ExtensionTestMessageListener command_listener("readyForCommand", true);
    ASSERT_TRUE(LoadApplication(app_path)) << failure_message_;

    const char* message = "";
    if (change_intercept) {
      message = enable_intercept ? "enable" : "disable";
    }
    ASSERT_TRUE(command_listener.WaitUntilSatisfied());
    command_listener.Reply(message);
    command_listener.Reset();

    ASSERT_TRUE(WaitForKeyEvent(ui::VKEY_Z)) << failure_message_;

    SendTaskSwitchKeys();

    // Send key and check if it is received.
    ASSERT_FALSE(SimulateKeyPress(ui::VKEY_Z)) << failure_message_;
  }

  void ValidateInterceptKeys(bool disable_after_enabling) {
    ExtensionTestMessageListener command_listener("readyForCommand", true);
    ASSERT_TRUE(LoadApplication(app_with_permission_)) << failure_message_;

    // setInterceptAllKeys() is asynchronous so wait for response and receiving
    // a key back.
    ASSERT_TRUE(command_listener.WaitUntilSatisfied());
    command_listener.Reply("enable");
    command_listener.Reset();

    ASSERT_TRUE(WaitForKeyEvent(ui::VKEY_Z)) << failure_message_;

    SendTaskSwitchKeys();

    // Send key and check if it is received.
    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_Z)) << failure_message_;

    ASSERT_TRUE(WaitForKeyEvent(ui::VKEY_Z)) << failure_message_;

    if (disable_after_enabling) {
      ASSERT_TRUE(command_listener.WaitUntilSatisfied());
      command_listener.Reply("disable");
      command_listener.Reset();
      ASSERT_TRUE(command_listener.WaitUntilSatisfied());
    }
  }

 protected:
  std::string failure_message_;
  base::Callback<void(void)> null_callback_;
  const char* app_with_permission_ =
      "window_api_intercept_all_keys/has_permission";
  const char* app_without_permission_ =
      "window_api_intercept_all_keys/no_permission";
};

// Currently this is implemented only for Windows.
// Disabled test http://crbug.com/438209
#if defined(OS_WIN)
#define MAYBE_GetKeysAfterSwitchSequence DISABLED_GetKeysAfterSwitchSequence
#else
#define MAYBE_GetKeysAfterSwitchSequence DISABLED_GetKeysAfterSwitchSequence
#endif

// Tests a window continues to keep focus even after application switch key
// sequence is sent when setInterceptAllKeys() is enabled.
IN_PROC_BROWSER_TEST_F(AppWindowInterceptAllKeysTest,
                       MAYBE_GetKeysAfterSwitchSequence) {
  ValidateInterceptKeys(false);
}

// Test to make sure that keys not received after disable.
// Disabled test http://crbug.com/438209
#if defined(OS_WIN)
#define MAYBE_NoKeysAfterDisableIsCalled DISABLED_NoKeysAfterDisableIsCalled
#else
#define MAYBE_NoKeysAfterDisableIsCalled DISABLED_NoKeysAfterDisableIsCalled
#endif

IN_PROC_BROWSER_TEST_F(AppWindowInterceptAllKeysTest,
                       MAYBE_NoKeysAfterDisableIsCalled) {
  ValidateInterceptKeys(true);

  ASSERT_TRUE(WaitForKeyEvent(ui::VKEY_Z)) << failure_message_;

  SendTaskSwitchKeys();

  // Send key and check if it is received.
  ASSERT_FALSE(SimulateKeyPress(ui::VKEY_Z)) << failure_message_;
}

// Test that calling just disable has no effect in retaining keyboard intercept.
// Currently this is implemented only for Windows.
// Disabled test http://crbug.com/438209
#if defined(OS_WIN)
#define MAYBE_NoopCallingDisableInterceptAllKeys \
  DISABLED_NoopCallingDisableInterceptAllKeys
#else
#define MAYBE_NoopCallingDisableInterceptAllKeys \
  DISABLED_NoopCallingDisableInterceptAllKeys
#endif

IN_PROC_BROWSER_TEST_F(AppWindowInterceptAllKeysTest,
                       MAYBE_NoopCallingDisableInterceptAllKeys) {
  ValidateCannotInterceptKeys(app_with_permission_, true, false);
}

// Test no effect when called without permissions
// Currently this is implemented only for Windows.
#if defined(OS_WIN)
#define MAYBE_NoopCallingEnableWithoutPermission \
  DISABLED_NoopCallingEnableWithoutPermission
#else
#define MAYBE_NoopCallingEnableWithoutPermission \
  DISABLED_NoopCallingEnableWithoutPermission
#endif

IN_PROC_BROWSER_TEST_F(AppWindowInterceptAllKeysTest,
                       MAYBE_NoopCallingEnableWithoutPermission) {
  ValidateCannotInterceptKeys(app_without_permission_, true, true);
}

// Test that intercept is disabled by default
#if defined(OS_WIN)
// Disabled test http://crbug.com/438209
#define MAYBE_InterceptDisabledByDefault DISABLED_InterceptDisabledByDefault
#else
#define MAYBE_InterceptDisabledByDefault DISABLED_InterceptDisabledByDefault
#endif

IN_PROC_BROWSER_TEST_F(AppWindowInterceptAllKeysTest,
                       MAYBE_InterceptDisabledByDefault) {
  ValidateCannotInterceptKeys(app_with_permission_, false, false);
}

// Tests that the application cannot be loaded in stable.
IN_PROC_BROWSER_TEST_F(AppWindowInterceptAllKeysTest, CannotLoadOtherThanDev) {
  chrome::VersionInfo::Channel version_info[] = {
      chrome::VersionInfo::CHANNEL_BETA, chrome::VersionInfo::CHANNEL_STABLE};
  for (unsigned int index = 0; index < arraysize(version_info); index++) {
    extensions::ScopedCurrentChannel channel(version_info[index]);
    const extensions::Extension* extension = nullptr;
    EXPECT_NONFATAL_FAILURE(
        extension = LoadExtension(test_data_dir_.AppendASCII("platform_apps")
                                      .AppendASCII(app_with_permission_)),
        "");

    DVLOG(1) << "Finished loading extension";

    ASSERT_TRUE(extension == nullptr) << "Application loaded in"
                                      << version_info[index]
                                      << " while permission does not exist";
  }
}

// Inject different keyboard combos and make sure that the app get them all.
// Disabled test http://crbug.com/438209
#if defined(OS_WIN)
#define MAYBE_ValidateKeyEvent DISABLED_ValidateKeyEvent
#else
#define MAYBE_ValidateKeyEvent DISABLED_ValidateKeyEvent
#endif

namespace {
// Maximum lenght of the result array in KeyEventTestData structure.
const size_t kMaxResultLength = 10;

// A structure holding test data of a keyboard event.
// Each keyboard event may generate multiple result strings representing
// the result of keydown, keypress, keyup and textInput events.
// For keydown, keypress and keyup events, the format of the result string is:
// <type> <keyCode> <charCode> <ctrlKey> <shiftKey> <altKey> <commandKey>
// where <type> may be 'D' (keydown), 'P' (keypress) or 'U' (keyup).
// <ctrlKey>, <shiftKey> <altKey> and <commandKey> are boolean value indicating
// the state of corresponding modifier key.
struct KeyEventTestData {
  ui::KeyboardCode key;
  bool control;
  bool shift;
  bool alt;
  bool command;

  int result_length;
  const char* const result[kMaxResultLength];
};
}  // namespace

IN_PROC_BROWSER_TEST_F(AppWindowInterceptAllKeysTest, MAYBE_ValidateKeyEvent) {
  // Launch the app
  ValidateInterceptKeys(false);

  static const KeyEventTestData kValidateKeyEvents[] = {
      // a
      {ui::VKEY_A,
       false,
       false,
       false,
       false,
       3,
       {"D 65 0 false false false false",
        "P 97 97 false false false false",
        "U 65 0 false false false false"}},
      // shift+a
      {ui::VKEY_A,
       false,
       true,
       false,
       false,
       5,
       {"D 16 0 false true false false",
        "D 65 0 false true false false",
        "P 65 65 false true false false",
        "U 65 0 false true false false",
        "U 16 0 false true false false"}},
      // ctrl+f which has accelerator binding should also result in all keys
      // being
      // sent.
      {ui::VKEY_F,
       true,
       false,
       false,
       false,
       5,
       {"D 17 0 true false false false",
        "D 70 0 true false false false",
        "P 6 6 true false false false",
        "U 70 0 true false false false",
        "U 17 0 true false false false"}},
      // ctrl+z
      {ui::VKEY_Z,
       true,
       false,
       false,
       false,
       5,
       {"D 17 0 true false false false",
        "D 90 0 true false false false",
        "P 26 26 true false false false",
        "U 90 0 true false false false",
        "U 17 0 true false false false"}},
      // alt+f
      {ui::VKEY_F,
       false,
       false,
       true,
       false,
       4,
       {"D 18 0 false false true false",
        "D 70 0 false false true false",
        "U 70 0 false false true false",
        "U 18 0 false false true false"}},
      // make sure both left and right shift makes it across
      {ui::VKEY_RSHIFT,
       false,
       false,
       false,
       false,
       2,
       {"D 16 0 false true false false", "U 16 0 false true false false"}},
  };

  DVLOG(1) << "Starting keyboard input test";

  for (unsigned int index = 0; index < arraysize(kValidateKeyEvents); index++) {
    // create all the event listeners needed
    const KeyEventTestData* current_event = &kValidateKeyEvents[index];
    scoped_ptr<ExtensionTestMessageListener> listeners[kMaxResultLength];
    for (int i = 0; i < current_event->result_length; i++) {
      listeners[i].reset(
          new ExtensionTestMessageListener(current_event->result[i], false));
    }
    ASSERT_TRUE(SimulateKeyPress(current_event->key, current_event->control,
                                 current_event->shift, current_event->alt,
                                 current_event->command));
    for (int i = 0; i < current_event->result_length; i++) {
      EXPECT_TRUE(listeners[i]->WaitUntilSatisfied());
    }
  }
}
