// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_controls.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/keycodes/keyboard_codes.h"

class WebViewInteractiveTest
    : public extensions::PlatformAppBrowserTest {
 public:
  WebViewInteractiveTest()
      : corner_(gfx::Point()),
        mouse_click_result_(false),
        first_click_(true) {}

  void MoveMouseInsideWindowWithListener(gfx::Point point,
                                         const std::string& message) {
    ExtensionTestMessageListener move_listener(message, false);
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
        gfx::Point(corner_.x() + point.x(), corner_.y() + point.y())));
    ASSERT_TRUE(move_listener.WaitUntilSatisfied());
  }

  void SendMouseClickWithListener(ui_controls::MouseButton button,
                                  const std::string& message) {
    ExtensionTestMessageListener listener(message, false);
    SendMouseClick(button);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  void SendMouseClick(ui_controls::MouseButton button) {
    SendMouseEvent(button, ui_controls::DOWN);
    SendMouseEvent(button, ui_controls::UP);
  }

  gfx::NativeWindow GetPlatformAppWindow() {
    extensions::ShellWindowRegistry::ShellWindowSet shell_windows =
        extensions::ShellWindowRegistry::Get(
            browser()->profile())->shell_windows();
    return (*shell_windows.begin())->GetNativeWindow();
  }

  void SendKeyPressToPlatformApp(ui::KeyboardCode key) {
    ASSERT_EQ(1U, GetShellWindowCount());
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), key, false, false, false, false));
  }

  void SendMouseEvent(ui_controls::MouseButton button,
                      ui_controls::MouseButtonState state) {
   if (first_click_) {
     mouse_click_result_ = ui_test_utils::SendMouseEventsSync(button,
                                                              state);
     first_click_ = false;
   } else {
     ASSERT_EQ(mouse_click_result_, ui_test_utils::SendMouseEventsSync(
         button, state));
   }
  }

  void SetupTest(const std::string& app_name,
                 const std::string& guest_url_spec) {
    ASSERT_TRUE(StartTestServer());
    std::string host_str("localhost");  // Must stay in scope with replace_host.
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host_str);

    GURL guest_url = test_server()->GetURL(guest_url_spec);
    guest_url = guest_url.ReplaceComponents(replace_host);

    ui_test_utils::UrlLoadObserver guest_observer(
        guest_url, content::NotificationService::AllSources());

    ExtensionTestMessageListener guest_connected_listener("connected", false);
    LoadAndLaunchPlatformApp(app_name.c_str());

    guest_observer.Wait();

    // Wait until the guest process reports that it has established a message
    // channel with the app.
    ASSERT_TRUE(guest_connected_listener.WaitUntilSatisfied());
    content::Source<content::NavigationController> source =
        guest_observer.source();
    EXPECT_TRUE(source->GetWebContents()->GetRenderProcessHost()->IsGuest());

    guest_web_contents_ = source->GetWebContents();
    embedder_web_contents_ = guest_web_contents_->GetEmbedderWebContents();

    gfx::Rect offset;
    embedder_web_contents_->GetView()->GetContainerBounds(&offset);
    corner_ = gfx::Point(offset.x(), offset.y());
  }

  content::WebContents* guest_web_contents() {
    return guest_web_contents_;
  }

  content::WebContents* embedder_web_contents() {
    return embedder_web_contents_;
  }

  gfx::Point corner() {
    return corner_;
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableBrowserPluginPointerLock);
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  content::WebContents* guest_web_contents_;
  content::WebContents* embedder_web_contents_;
  gfx::Point corner_;
  bool mouse_click_result_;
  bool first_click_;
};

// ui_test_utils::SendMouseMoveSync doesn't seem to work on OS_MACOSX, and
// likely won't work on many other platforms as well, so for now this test
// is for Windows and Linux only.
#if (defined(OS_WIN) || defined(OS_LINUX))

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, PointerLock) {
  SetupTest("web_view/pointer_lock",
            "files/extensions/platform_apps/web_view/pointer_lock/guest.html");

  ExtensionTestMessageListener guest_clicked_listener("clicked", false);
  // We need to simulate a mouse click in the window or else SendMouseMoveSync
  // will cause the browser to crash (not sure why exactly...)
  SimulateMouseClickAt(guest_web_contents(), 0,
      WebKit::WebMouseEvent::ButtonLeft, gfx::Point(75, 75));
  ASSERT_TRUE(guest_clicked_listener.WaitUntilSatisfied());

  // Move the mouse over the Lock Pointer button.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
      gfx::Point(corner().x() + 75, corner().y() + 25)));

  // Click the Lock Pointer button, locking the mouse to lockTarget1.
  SendMouseClickWithListener(ui_controls::LEFT, "locked");

  // Attempt to move the mouse off of the lock target, and onto lockTarget2,
  // (which would trigger a test failure).
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
      gfx::Point(corner().x() + 74, corner().y() + 74)));
  MoveMouseInsideWindowWithListener(gfx::Point(75, 75), "mouse-move");

#if (defined(OS_WIN) && defined(USE_AURA))
  // When the mouse is unlocked on win aura, sending a test mouse click clicks
  // where the mouse moved to while locked. I was unable to figure out why, and
  // since the issue only occurs with the test mouse events, just fix it with
  // a simple workaround - moving the mouse back to where it should be.
  // TODO(mthiesse): Fix Win Aura simulated mouse events while mouse locked.
  MoveMouseInsideWindowWithListener(gfx::Point(75, 25), "mouse-move");
#endif

  ExtensionTestMessageListener unlocked_listener("unlocked", false);
  // Send a key press to unlock the mouse.
  SendKeyPressToPlatformApp(ui::VKEY_ESCAPE);

  // Wait for page to receive (successful) mouse unlock response.
  ASSERT_TRUE(unlocked_listener.WaitUntilSatisfied());

  // After the second lock, guest.js sends a message to main.js to remove the
  // webview object. main.js then removes the div containing the webview, which
  // should unlock, and leave the mouse over the mousemove-capture-container
  // div. We then move the mouse over that div to ensure the mouse was properly
  // unlocked and that the div receieves the message.
  ExtensionTestMessageListener move_captured_listener("move-captured", false);
  move_captured_listener.AlsoListenForFailureMessage("timeout");

  // Mouse should already be over lock button (since we just unlocked), so send
  // click to re-lock the mouse.
  SendMouseClickWithListener(ui_controls::LEFT, "deleted");

  // A mousemove event is triggered on the mousemove-capture-container element
  // when we delete the webview container (since the mouse moves onto the
  // element), but just in case, send an explicit mouse movement to be safe.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
      gfx::Point(corner().x() + 50, corner().y() + 10)));

  // Wait for page to receive second (successful) mouselock response.
  bool success = move_captured_listener.WaitUntilSatisfied();
  if (!success) {
    fprintf(stderr, "TIMEOUT - retrying\n");
    // About 1 in 40 tests fail to detect mouse moves at this point (why?).
    // Sending a right click seems to fix this (why?).
    ExtensionTestMessageListener move_listener2("move-captured", false);
    SendMouseClick(ui_controls::RIGHT);
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
        gfx::Point(corner().x() + 51, corner().y() + 11)));
    ASSERT_TRUE(move_listener2.WaitUntilSatisfied());
  }
}

#endif  // (defined(OS_WIN) || defined(OS_LINUX))
