// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/test/ui_controls.h"

using apps::ShellWindow;

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

  void MoveMouseInsideWindow(const gfx::Point& point) {
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
        gfx::Point(corner_.x() + point.x(), corner_.y() + point.y())));
  }

  gfx::NativeWindow GetPlatformAppWindow() {
    const apps::ShellWindowRegistry::ShellWindowList& shell_windows =
        apps::ShellWindowRegistry::Get(
            browser()->profile())->shell_windows();
    return (*shell_windows.begin())->GetNativeWindow();
  }

  void SendKeyPressToPlatformApp(ui::KeyboardCode key) {
    ASSERT_EQ(1U, GetShellWindowCount());
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), key, false, false, false, false));
  }

  void SendCopyKeyPressToPlatformApp() {
    ASSERT_EQ(1U, GetShellWindowCount());
#if defined(OS_MACOSX)
    // Send Cmd+C on MacOSX.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_C, false, false, false, true));
#else
    // Send Ctrl+C on Windows and Linux/ChromeOS.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_C, true, false, false, false));
#endif
  }

  void SendStartOfLineKeyPressToPlatformApp() {
#if defined(OS_MACOSX)
    // Send Cmd+Left on MacOSX.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_LEFT, false, false, false, true));
#else
    // Send Ctrl+Left on Windows and Linux/ChromeOS.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_LEFT, true, false, false, false));
#endif
  }

  void SendBackShortcutToPlatformApp() {
#if defined(OS_MACOSX)
    // Send Cmd+[ on MacOSX.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_OEM_4, false, false, false, true));
#else
    // Send browser back key on Linux/Windows.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_BROWSER_BACK,
        false, false, false, false));
#endif
  }

  void SendForwardShortcutToPlatformApp() {
#if defined(OS_MACOSX)
    // Send Cmd+] on MacOSX.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_OEM_6, false, false, false, true));
#else
    // Send browser back key on Linux/Windows.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_BROWSER_FORWARD,
        false, false, false, false));
#endif
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

  void TestHelper(const std::string& test_name,
                  const std::string& test_passed_msg,
                  const std::string& test_failed_msg,
                  const std::string& app_location) {
    ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
    ExtensionTestMessageListener launched_listener("Launched", false);
    LoadAndLaunchPlatformApp(app_location.c_str());
    ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

    ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
        GetPlatformAppWindow()));

    // Flush any pending events to make sure we start with a clean slate.
    content::RunAllPendingInMessageLoop();

    content::WebContents* embedder_web_contents =
        GetFirstShellWindowWebContents();
    ASSERT_TRUE(embedder_web_contents);

    ExtensionTestMessageListener done_listener(test_passed_msg, false);
    done_listener.AlsoListenForFailureMessage(test_failed_msg);
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    base::StringPrintf("runTest('%s')", test_name.c_str())));
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  }

  void SetupTest(const std::string& app_name,
                 const std::string& guest_url_spec) {
    ASSERT_TRUE(StartEmbeddedTestServer());
    GURL::Replacements replace_host;
    std::string host_str("localhost");  // Must stay in scope with replace_host.
    replace_host.SetHostStr(host_str);

    GURL guest_url = embedded_test_server()->GetURL(guest_url_spec);
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

    const testing::TestInfo* const test_info =
            testing::UnitTest::GetInstance()->current_test_info();
    const std::string& prefix = "DragDropWithinWebView";
    if (!strncmp(test_info->name(), prefix.c_str(), prefix.size())) {
      // In the drag drop test we add 20px padding to the page body because on
      // windows if we get too close to the edge of the window the resize cursor
      // appears and we start dragging the window edge.
      corner_.Offset(20, 20);
    }
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

  void SimulateRWHMouseClick(content::RenderWidgetHost* rwh, int x, int y) {
    WebKit::WebMouseEvent mouse_event;
    mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
    mouse_event.x = mouse_event.windowX = x;
    mouse_event.y = mouse_event.windowY = y;
    mouse_event.modifiers = 0;

    mouse_event.type = WebKit::WebInputEvent::MouseDown;
    rwh->ForwardMouseEvent(mouse_event);
    mouse_event.type = WebKit::WebInputEvent::MouseUp;
    rwh->ForwardMouseEvent(mouse_event);
  }

  class PopupCreatedObserver {
   public:
    PopupCreatedObserver() : created_(false), last_render_widget_host_(NULL) {
      created_callback_ = base::Bind(
          &PopupCreatedObserver::CreatedCallback, base::Unretained(this));
      content::RenderWidgetHost::AddCreatedCallback(created_callback_);
    }
    virtual ~PopupCreatedObserver() {
      content::RenderWidgetHost::RemoveCreatedCallback(created_callback_);
    }
    void Reset() {
      created_ = false;
    }
    void Start() {
      if (created_)
        return;
      message_loop_ = new content::MessageLoopRunner;
      message_loop_->Run();
    }
    content::RenderWidgetHost* last_render_widget_host() {
      return last_render_widget_host_;
    }

   private:
    void CreatedCallback(content::RenderWidgetHost* rwh) {
      last_render_widget_host_ = rwh;
      if (message_loop_.get())
        message_loop_->Quit();
      else
        created_ = true;
    }
    content::RenderWidgetHost::CreatedCallback created_callback_;
    scoped_refptr<content::MessageLoopRunner> message_loop_;
    bool created_;
    content::RenderWidgetHost* last_render_widget_host_;
  };

  void WaitForTitle(const char* title) {
    string16 expected_title(ASCIIToUTF16(title));
    string16 error_title(ASCIIToUTF16("FAILED"));
    content::TitleWatcher title_watcher(guest_web_contents(), expected_title);
    title_watcher.AlsoWaitForTitle(error_title);
    ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void PopupTestHelper(const gfx::Point& padding) {
    PopupCreatedObserver popup_created_observer;
    popup_created_observer.Reset();

    content::SimulateKeyPress(
        guest_web_contents(),
        ui::VKEY_C,  // C to autocomplete.
        false, false, false, false);

    WaitForTitle("PASSED1");

    popup_created_observer.Start();

    content::RenderWidgetHost* popup_rwh = NULL;
    popup_rwh = popup_created_observer.last_render_widget_host();
    // Popup must be present.
    ASSERT_TRUE(popup_rwh);
    ASSERT_TRUE(!popup_rwh->IsRenderView());
    ASSERT_TRUE(popup_rwh->GetView());

    string16 expected_title = ASCIIToUTF16("PASSED2");
    string16 error_title = ASCIIToUTF16("FAILED");
    content::TitleWatcher title_watcher(guest_web_contents(), expected_title);
    title_watcher.AlsoWaitForTitle(error_title);
    EXPECT_TRUE(content::ExecuteScript(guest_web_contents(),
                                       "changeTitle();"));
    ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

    gfx::Rect popup_bounds = popup_rwh->GetView()->GetViewBounds();
    // (2, 2) is expected to lie on the first datalist element.
    SimulateRWHMouseClick(popup_rwh, 2, 2);

    content::RenderViewHost* embedder_rvh =
        GetFirstShellWindowWebContents()->GetRenderViewHost();
    gfx::Rect embedder_bounds = embedder_rvh->GetView()->GetViewBounds();
    gfx::Vector2d diff = popup_bounds.origin() - embedder_bounds.origin();
    LOG(INFO) << "DIFF: x = " << diff.x() << ", y = " << diff.y();

    const int left_spacing = 40 + padding.x();  // div.style.paddingLeft = 40px.
    // div.style.paddingTop = 50px + (input box height = 26px).
    const int top_spacing = 50 + 26 + padding.y();

    // If the popup is placed within |threshold_px| of the expected position,
    // then we consider the test as a pass.
    const int threshold_px = 10;

    EXPECT_LE(std::abs(diff.x() - left_spacing), threshold_px);
    EXPECT_LE(std::abs(diff.y() - top_spacing), threshold_px);

    WaitForTitle("PASSED3");
  }

  void DragTestStep1() {
    // Move mouse to start of text.
    MoveMouseInsideWindow(gfx::Point(45, 8));
    SendMouseEvent(ui_controls::LEFT, ui_controls::DOWN);
    MoveMouseInsideWindow(gfx::Point(76, 12));

    // Now wait a bit before moving mouse to initiate drag/drop.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&WebViewInteractiveTest::DragTestStep2,
                   base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(200));
  }

  void DragTestStep2() {
    // Drag source over target.
    MoveMouseInsideWindow(gfx::Point(76, 76));

    // Create a second mouse over the source to trigger the drag over event.
    MoveMouseInsideWindow(gfx::Point(76, 77));

    // Release mouse to drop.
    SendMouseEvent(ui_controls::LEFT, ui_controls::UP);
    SendMouseClick(ui_controls::LEFT);

    quit_closure_.Run();

    // Note that following ExtensionTestMessageListener and ExecuteScript*
    // call must be after we quit |quit_closure_|. Otherwise the class
    // here won't be able to receive messages sent by chrome.test.sendMessage.
    // This is because of the nature of drag and drop code (esp. the
    // MessageLoop) in it.

    // Now verify we got a drop and correct drop data.
    ExtensionTestMessageListener drop_listener("guest-got-drop", false);
    EXPECT_TRUE(content::ExecuteScript(guest_web_contents_,
                                       "window.pingEmbedder()"));
    EXPECT_TRUE(drop_listener.WaitUntilSatisfied());

    std::string last_drop_data;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        embedder_web_contents_,
        "window.domAutomationController.send(getLastDropData())",
        &last_drop_data));
    EXPECT_EQ(last_drop_data, "Drop me");
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableBrowserPluginDragDrop);
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
  }

  content::WebContents* guest_web_contents_;
  content::WebContents* embedder_web_contents_;
  gfx::Point corner_;
  bool mouse_click_result_;
  bool first_click_;
  // Only used in drag/drop test.
  base::Closure quit_closure_;
};

// ui_test_utils::SendMouseMoveSync doesn't seem to work on OS_MACOSX, and
// likely won't work on many other platforms as well, so for now this test
// is for Windows and Linux only.
#if (defined(OS_WIN) || defined(OS_LINUX))

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, PointerLock) {
  SetupTest("web_view/pointer_lock",
            "/extensions/platform_apps/web_view/pointer_lock/guest.html");

  // Move the mouse over the Lock Pointer button.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
      gfx::Point(corner().x() + 75, corner().y() + 25)));

  // Click the Lock Pointer button. The first two times the button is clicked
  // the permission API will deny the request (intentional).
  ExtensionTestMessageListener exception_listener("request exception", false);
  SendMouseClickWithListener(ui_controls::LEFT, "lock error");
  ASSERT_TRUE(exception_listener.WaitUntilSatisfied());
  SendMouseClickWithListener(ui_controls::LEFT, "lock error");

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

// Tests that setting focus on the <webview> sets focus on the guest.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, DISABLED_Focus) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/focus"))
      << message_;
}

// Tests that guests receive edit commands and respond appropriately.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, EditCommands) {
  SetupTest("web_view/edit_commands",
            "/extensions/platform_apps/web_view/edit_commands/guest.html");

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetPlatformAppWindow()));

  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();

  ExtensionTestMessageListener copy_listener("copy", false);
  SendCopyKeyPressToPlatformApp();

  // Wait for the guest to receive a 'copy' edit command.
  ASSERT_TRUE(copy_listener.WaitUntilSatisfied());
}

// Tests that guests receive edit commands and respond appropriately.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, EditCommandsNoMenu) {
  SetupTest("web_view/edit_commands_no_menu",
      "/extensions/platform_apps/web_view/edit_commands_no_menu/"
      "guest.html");

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetPlatformAppWindow()));

  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();

  ExtensionTestMessageListener start_of_line_listener("StartOfLine", false);
  SendStartOfLineKeyPressToPlatformApp();
  // Wait for the guest to receive a 'copy' edit command.
  ASSERT_TRUE(start_of_line_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest,
                       NewWindow_NewWindowNameTakesPrecedence) {
  TestHelper("testNewWindowNameTakesPrecedence",
             "DoneNewWindowTest.PASSED",
             "DoneNewWindowTest.FAILED",
             "web_view/newwindow");
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest,
                       NewWindow_WebViewNameTakesPrecedence) {
  TestHelper("testWebViewNameTakesPrecedence",
             "DoneNewWindowTest.PASSED",
             "DoneNewWindowTest.FAILED",
             "web_view/newwindow");
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, NewWindow_NoName) {
  TestHelper("testNoName",
             "DoneNewWindowTest.PASSED",
             "DoneNewWindowTest.FAILED",
             "web_view/newwindow");
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, NewWindow_Redirect) {
  TestHelper("testNewWindowRedirect",
             "DoneNewWindowTest.PASSED",
             "DoneNewWindowTest.FAILED",
             "web_view/newwindow");
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, NewWindow_Close) {
  TestHelper("testNewWindowClose",
             "DoneNewWindowTest.PASSED",
             "DoneNewWindowTest.FAILED",
             "web_view/newwindow");
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, NewWindow_ExecuteScript) {
  TestHelper("testNewWindowExecuteScript",
             "DoneNewWindowTest.PASSED",
             "DoneNewWindowTest.FAILED",
             "web_view/newwindow");
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, NewWindow_WebRequest) {
  TestHelper("testNewWindowWebRequest",
             "DoneNewWindowTest.PASSED",
             "DoneNewWindowTest.FAILED",
             "web_view/newwindow");
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, ExecuteCode) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "execute_code")) << message_;
}

// This test used the old Autofill UI, which has been removed.
// See crbug.com/259438
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, DISABLED_PopupPositioning) {
  SetupTest(
      "web_view/popup_positioning",
      "/extensions/platform_apps/web_view/popup_positioning/guest.html");
  ASSERT_TRUE(guest_web_contents());

  PopupTestHelper(gfx::Point());

  // moveTo a random location and run the steps again.
  EXPECT_TRUE(content::ExecuteScript(embedder_web_contents(),
                                     "window.moveTo(16, 20);"));
  PopupTestHelper(gfx::Point());
}

// Tests that moving browser plugin (without resize/UpdateRects) correctly
// repositions popup.
// Started flakily failing after a Blink roll: http://crbug.com/245332
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, DISABLED_PopupPositioningMoved) {
  SetupTest(
      "web_view/popup_positioning_moved",
      "/extensions/platform_apps/web_view/popup_positioning_moved"
      "/guest.html");
  ASSERT_TRUE(guest_web_contents());

  PopupTestHelper(gfx::Point(20, 0));
}

// Drag and drop inside a webview is currently only enabled for linux and mac,
// but the tests don't work on anything except chromeos for now. This is because
// of simulating mouse drag code's dependency on platforms.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, DragDropWithinWebView) {
  SetupTest(
      "web_view/dnd_within_webview",
      "/extensions/platform_apps/web_view/dnd_within_webview/guest.html");
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(GetPlatformAppWindow()));

  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&WebViewInteractiveTest::DragTestStep1,
                 base::Unretained(this)));
  run_loop.Run();
}
#endif  // (defined(OS_CHROMEOS))

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, Navigation) {
  TestHelper("testNavigation",
             "DoneNavigationTest.PASSED",
             "DoneNavigationTest.FAILED",
             "web_view/navigation");
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, Navigation_BackForwardKeys) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ExtensionTestMessageListener launched_listener("Launched", false);
  LoadAndLaunchPlatformApp("web_view/navigation");
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetPlatformAppWindow()));
  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();

  content::WebContents* embedder_web_contents =
      GetFirstShellWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);

  ExtensionTestMessageListener done_listener(
      "DoneNavigationTest.PASSED", false);
  done_listener.AlsoListenForFailureMessage("DoneNavigationTest.FAILED");
  ExtensionTestMessageListener ready_back_key_listener(
      "ReadyForBackKey", false);
  ExtensionTestMessageListener ready_forward_key_listener(
      "ReadyForForwardKey", false);

  EXPECT_TRUE(content::ExecuteScript(
                  embedder_web_contents,
                  "runTest('testBackForwardKeys')"));

  ASSERT_TRUE(ready_back_key_listener.WaitUntilSatisfied());
  SendBackShortcutToPlatformApp();

  ASSERT_TRUE(ready_forward_key_listener.WaitUntilSatisfied());
  SendForwardShortcutToPlatformApp();

  ASSERT_TRUE(done_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest,
                       PointerLock_PointerLockLostWithFocus) {
  TestHelper("testPointerLockLostWithFocus",
             "DonePointerLockTest.PASSED",
             "DonePointerLockTest.FAILED",
             "web_view/pointerlock");
}
