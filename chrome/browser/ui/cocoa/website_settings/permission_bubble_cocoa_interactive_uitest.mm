// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#import "base/mac/scoped_nsobject.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/permissions/permission_request_manager_test_api.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "ui/base/test/ui_controls.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#include "ui/base/ui_base_switches.h"
#import "ui/events/test/cocoa_test_event_utils.h"

namespace {

enum class UiMode {
  VIEWS,
  COCOA,
};

std::string UiModeToString(const ::testing::TestParamInfo<UiMode>& info) {
  return info.param == UiMode::VIEWS ? "Views" : "Cocoa";
}

}  // namespace

class PermissionBubbleInteractiveUITest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<UiMode> {
 public:
  PermissionBubbleInteractiveUITest() {}

  void EnsureWindowActive(NSWindow* window, const char* message) {
    SCOPED_TRACE(message);
    EXPECT_TRUE(window);

    // Activation is asynchronous on Mac. If the window didn't become active,
    // wait for it.
    if (![window isKeyWindow]) {
      base::scoped_nsobject<WindowedNSNotificationObserver> waiter(
          [[WindowedNSNotificationObserver alloc]
              initForNotification:NSWindowDidBecomeKeyNotification
                           object:window]);
      [waiter wait];
    }
    EXPECT_TRUE([window isKeyWindow]);
  }

  // Send Cmd+keycode in the key window to NSApp.
  void SendAccelerator(ui::KeyboardCode keycode, bool shift, bool alt) {
    bool control = false;
    bool command = true;
    // Note that although this takes an NSWindow, it's just used to create the
    // NSEvent* which will be dispatched to NSApp (i.e. not NSWindow).
    ui_controls::SendKeyPress(
        [NSApp keyWindow], keycode, control, shift, alt, command);
  }

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam() == UiMode::VIEWS)
      command_line->AppendSwitch(switches::kExtendMdToSecondaryUi);
  }

  void SetUpOnMainThread() override {
    // Make the browser active (ensures the app can receive key events).
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    test_api_ =
        base::MakeUnique<test::PermissionRequestManagerTestApi>(browser());
    EXPECT_TRUE(test_api_->manager());

    test_api_->AddSimpleRequest(browser()->profile(),
                                CONTENT_SETTINGS_TYPE_GEOLOCATION);

    EXPECT_TRUE([browser()->window()->GetNativeWindow() isKeyWindow]);
    test_api_->manager()->DisplayPendingRequests();

    // The bubble should steal key focus when shown.
    EnsureWindowActive(test_api_->GetPromptWindow(), "show permission bubble");
  }

 protected:
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleInteractiveUITest);
};

// There is only one tab. Cmd+w will close it along with the browser window.
IN_PROC_BROWSER_TEST_P(PermissionBubbleInteractiveUITest, CmdWClosesWindow) {
  base::scoped_nsobject<NSWindow> browser_window(
      browser()->window()->GetNativeWindow(), base::scoped_policy::RETAIN);
  EXPECT_TRUE([browser_window isVisible]);

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, content::Source<Browser>(browser()));

  SendAccelerator(ui::VKEY_W, false, false);

  // The actual window close happens via a posted task.
  EXPECT_TRUE([browser_window isVisible]);
  observer.Wait();
  EXPECT_FALSE([browser_window isVisible]);
}

// Add a tab, ensure we can switch away and back using Cmd+Alt+Left/Right and
// curly braces.
IN_PROC_BROWSER_TEST_P(PermissionBubbleInteractiveUITest, SwitchTabs) {
  NSWindow* browser_window = browser()->window()->GetNativeWindow();

  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_TRUE(test_api_->GetPromptWindow());

  // Add a blank tab in the foreground.
  AddBlankTabAndShow(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // The bubble should hide and give focus back to the browser. However, the
  // test environment can't guarantee that macOS decides that the Browser window
  // is actually the "best" window to activate upon closing the current key
  // window. So activate it manually.
  [browser_window makeKeyAndOrderFront:nil];
  EnsureWindowActive(browser_window, "tab added");

  // Prompt is hidden while its tab is not active.
  EXPECT_FALSE(test_api_->GetPromptWindow());

  // Now a webcontents is active, it gets a first shot at processing the
  // accelerator before sending it back unhandled to the browser via IPC. That's
  // all a bit much to handle in a test, so activate the location bar.
  chrome::FocusLocationBar(browser());
  SendAccelerator(ui::VKEY_LEFT, false, true);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Note we don't need to makeKeyAndOrderFront: the permission window will take
  // focus when it is shown again.
  EnsureWindowActive(test_api_->GetPromptWindow(),
                     "switched to permission tab with arrow");
  EXPECT_TRUE(test_api_->GetPromptWindow());

  // Ensure we can switch away with the bubble active.
  SendAccelerator(ui::VKEY_RIGHT, false, true);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  [browser_window makeKeyAndOrderFront:nil];
  EnsureWindowActive(browser_window, "switch away with arrow");
  EXPECT_FALSE(test_api_->GetPromptWindow());

  // Also test switching tabs with curly braces. "VKEY_OEM_4" is
  // LeftBracket/Brace on a US keyboard, which ui::MacKeyCodeForWindowsKeyCode
  // will map to '{' when shift is passed. Also note there are only two tabs so
  // it doesn't matter which direction is taken (it wraps).
  chrome::FocusLocationBar(browser());
  SendAccelerator(ui::VKEY_OEM_4, true, false);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EnsureWindowActive(test_api_->GetPromptWindow(),
                     "switch to permission tab with curly brace");
  EXPECT_TRUE(test_api_->GetPromptWindow());

  SendAccelerator(ui::VKEY_OEM_4, true, false);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  [browser_window makeKeyAndOrderFront:nil];
  EnsureWindowActive(browser_window, "switch away with curly brace");
  EXPECT_FALSE(test_api_->GetPromptWindow());
}

INSTANTIATE_TEST_CASE_P(,
                        PermissionBubbleInteractiveUITest,
                        ::testing::Values(UiMode::VIEWS, UiMode::COCOA),
                        &UiModeToString);
