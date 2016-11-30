// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes.h"

using extensions::AppWindow;
using extensions::ExtensionsAPIClient;
using guest_view::GuestViewBase;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;

class WebViewInteractiveTestBase : public extensions::PlatformAppBrowserTest {
 public:
  WebViewInteractiveTestBase()
      : guest_web_contents_(NULL),
        embedder_web_contents_(NULL),
        corner_(gfx::Point()),
        mouse_click_result_(false),
        first_click_(true) {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  TestGuestViewManager* GetGuestViewManager() {
    TestGuestViewManager* manager = static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(browser()->profile()));
    // TestGuestViewManager::WaitForSingleGuestCreated may and will get called
    // before a guest is created.
    if (!manager) {
      manager = static_cast<TestGuestViewManager*>(
          GuestViewManager::CreateWithDelegate(
              browser()->profile(),
              ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                  browser()->profile())));
    }
    return manager;
  }

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
    const extensions::AppWindowRegistry::AppWindowList& app_windows =
        extensions::AppWindowRegistry::Get(browser()->profile())->app_windows();
    return (*app_windows.begin())->GetNativeWindow();
  }

  void SendKeyPressToPlatformApp(ui::KeyboardCode key) {
    ASSERT_EQ(1U, GetAppWindowCount());
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), key, false, false, false, false));
  }

  void SendCopyKeyPressToPlatformApp() {
    ASSERT_EQ(1U, GetAppWindowCount());
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

  enum TestServer {
    NEEDS_TEST_SERVER,
    NO_TEST_SERVER
  };

  std::unique_ptr<ExtensionTestMessageListener> RunAppHelper(
      const std::string& test_name,
      const std::string& app_location,
      TestServer test_server,
      content::WebContents** embedder_web_contents) {
    // For serving guest pages.
    if ((test_server == NEEDS_TEST_SERVER) && !StartEmbeddedTestServer()) {
      LOG(ERROR) << "FAILED TO START TEST SERVER.";
      return std::unique_ptr<ExtensionTestMessageListener>();
    }

    LoadAndLaunchPlatformApp(app_location.c_str(), "Launched");
    if (!ui_test_utils::ShowAndFocusNativeWindow(GetPlatformAppWindow())) {
      LOG(ERROR) << "UNABLE TO FOCUS TEST WINDOW.";
      return std::unique_ptr<ExtensionTestMessageListener>();
    }

    // Flush any pending events to make sure we start with a clean slate.
    content::RunAllPendingInMessageLoop();

    *embedder_web_contents = GetFirstAppWindowWebContents();

    std::unique_ptr<ExtensionTestMessageListener> done_listener(
        new ExtensionTestMessageListener("TEST_PASSED", false));
    done_listener->set_failure_message("TEST_FAILED");
    if (!content::ExecuteScript(
            *embedder_web_contents,
            base::StringPrintf("runTest('%s')", test_name.c_str()))) {
      LOG(ERROR) << "UNABLE TO START TEST";
      return std::unique_ptr<ExtensionTestMessageListener>();
    }

    return done_listener;
  }

  void TestHelper(const std::string& test_name,
                  const std::string& app_location,
                  TestServer test_server) {
    content::WebContents* embedder_web_contents = NULL;
    std::unique_ptr<ExtensionTestMessageListener> done_listener(RunAppHelper(
        test_name, app_location, test_server, &embedder_web_contents));

    ASSERT_TRUE(done_listener);
    ASSERT_TRUE(done_listener->WaitUntilSatisfied());

    embedder_web_contents_ = embedder_web_contents;
    guest_web_contents_ = GetGuestViewManager()->WaitForSingleGuestCreated();
  }

  void SendMessageToEmbedder(const std::string& message) {
    ASSERT_TRUE(content::ExecuteScript(
        GetFirstAppWindowWebContents(),
        base::StringPrintf("onAppMessage('%s');", message.c_str())));
  }

  void SetupTest(const std::string& app_name,
                 const std::string& guest_url_spec) {
    ASSERT_TRUE(StartEmbeddedTestServer());
    GURL::Replacements replace_host;
    replace_host.SetHostStr("localhost");

    GURL guest_url = embedded_test_server()->GetURL(guest_url_spec);
    guest_url = guest_url.ReplaceComponents(replace_host);

    ui_test_utils::UrlLoadObserver guest_observer(
        guest_url, content::NotificationService::AllSources());

    LoadAndLaunchPlatformApp(app_name.c_str(), "connected");

    guest_observer.Wait();
    content::Source<content::NavigationController> source =
        guest_observer.source();
    EXPECT_TRUE(source->GetWebContents()->GetRenderProcessHost()->
        IsForGuestsOnly());

    guest_web_contents_ = source->GetWebContents();
    embedder_web_contents_ =
        GuestViewBase::FromWebContents(guest_web_contents_)->
            embedder_web_contents();

    gfx::Rect offset = embedder_web_contents_->GetContainerBounds();
    corner_ = gfx::Point(offset.x(), offset.y());

    const testing::TestInfo* const test_info =
            testing::UnitTest::GetInstance()->current_test_info();
    const char* prefix = "DragDropWithinWebView";
    if (!strncmp(test_info->name(), prefix, strlen(prefix))) {
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

  void SimulateRWHMouseClick(content::RenderWidgetHost* rwh,
                             blink::WebMouseEvent::Button button,
                             int x,
                             int y) {
    blink::WebMouseEvent mouse_event;
    mouse_event.button = button;
    mouse_event.x = mouse_event.windowX = x;
    mouse_event.y = mouse_event.windowY = y;
    mouse_event.modifiers = 0;
    // Needed for the WebViewTest.ContextMenuPositionAfterCSSTransforms
    gfx::Rect rect = rwh->GetView()->GetViewBounds();
    mouse_event.globalX = x + rect.x();
    mouse_event.globalY = y + rect.y();
    mouse_event.type = blink::WebInputEvent::MouseDown;
    rwh->ForwardMouseEvent(mouse_event);
    mouse_event.type = blink::WebInputEvent::MouseUp;
    rwh->ForwardMouseEvent(mouse_event);
  }

  class PopupCreatedObserver {
   public:
    PopupCreatedObserver()
        : initial_widget_count_(0),
          last_render_widget_host_(NULL) {}

    ~PopupCreatedObserver() {}

    void Wait() {
      if (CountWidgets() == initial_widget_count_ + 1 &&
          last_render_widget_host_->GetView()->GetNativeView()) {
        gfx::Rect popup_bounds =
            last_render_widget_host_->GetView()->GetViewBounds();
        if (!popup_bounds.size().IsEmpty()) {
          if (message_loop_.get())
            message_loop_->Quit();
          return;
        }
      }

      // If we haven't seen any new widget or we get 0 size widget, we need to
      // schedule waiting.
      ScheduleWait();

      if (!message_loop_.get()) {
        message_loop_ = new content::MessageLoopRunner;
        message_loop_->Run();
      }
    }

    void Init() { initial_widget_count_ = CountWidgets(); }

    // Returns the last widget created.
    content::RenderWidgetHost* last_render_widget_host() {
      return last_render_widget_host_;
    }

   private:
    void ScheduleWait() {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&PopupCreatedObserver::Wait, base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(200));
    }

    size_t CountWidgets() {
      std::unique_ptr<content::RenderWidgetHostIterator> widgets(
          content::RenderWidgetHost::GetRenderWidgetHosts());
      size_t num_widgets = 0;
      while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
        if (content::RenderViewHost::From(widget))
          continue;
        ++num_widgets;
        last_render_widget_host_ = widget;
      }
      return num_widgets;
    }

    size_t initial_widget_count_;
    content::RenderWidgetHost* last_render_widget_host_;
    scoped_refptr<content::MessageLoopRunner> message_loop_;

    DISALLOW_COPY_AND_ASSIGN(PopupCreatedObserver);
  };

  void PopupTestHelper(const gfx::Point& padding) {
    PopupCreatedObserver popup_observer;
    popup_observer.Init();
    // Press alt+DOWN to open popup.
    bool alt = true;
    content::SimulateKeyPress(guest_web_contents(), ui::DomKey::ARROW_DOWN,
                              ui::DomCode::ARROW_DOWN, ui::VKEY_DOWN, false,
                              false, alt, false);
    popup_observer.Wait();

    content::RenderWidgetHost* popup_rwh =
        popup_observer.last_render_widget_host();
    gfx::Rect popup_bounds = popup_rwh->GetView()->GetViewBounds();

    content::RenderViewHost* embedder_rvh =
        GetFirstAppWindowWebContents()->GetRenderViewHost();
    gfx::Rect embedder_bounds =
        embedder_rvh->GetWidget()->GetView()->GetViewBounds();
    gfx::Vector2d diff = popup_bounds.origin() - embedder_bounds.origin();
    LOG(INFO) << "DIFF: x = " << diff.x() << ", y = " << diff.y();

    const int left_spacing = 40 + padding.x();  // div.style.paddingLeft = 40px.
    // div.style.paddingTop = 60px + (input box height = 26px).
    const int top_spacing = 60 + 26 + padding.y();

    // If the popup is placed within |threshold_px| of the expected position,
    // then we consider the test as a pass.
    const int threshold_px = 10;

    EXPECT_LE(std::abs(diff.x() - left_spacing), threshold_px);
    EXPECT_LE(std::abs(diff.y() - top_spacing), threshold_px);

    // Close the popup.
    content::SimulateKeyPress(guest_web_contents(), ui::DomKey::ESCAPE,
                              ui::DomCode::ESCAPE, ui::VKEY_ESCAPE, false,
                              false, false, false);
  }

  void DragTestStep1() {
    // Move mouse to start of text.
    MoveMouseInsideWindow(gfx::Point(45, 8));
    MoveMouseInsideWindow(gfx::Point(45, 9));
    SendMouseEvent(ui_controls::LEFT, ui_controls::DOWN);

    MoveMouseInsideWindow(gfx::Point(74, 12));
    MoveMouseInsideWindow(gfx::Point(78, 12));

    // Now wait a bit before moving mouse to initiate drag/drop.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&WebViewInteractiveTestBase::DragTestStep2,
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

    // Now check if we got a drop and read the drop data.
    embedder_web_contents_ = GetFirstAppWindowWebContents();
    ExtensionTestMessageListener drop_listener("guest-got-drop", false);
    EXPECT_TRUE(content::ExecuteScript(embedder_web_contents_,
                                       "window.checkIfGuestGotDrop()"));
    EXPECT_TRUE(drop_listener.WaitUntilSatisfied());

    std::string last_drop_data;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
                    embedder_web_contents_,
                    "window.domAutomationController.send(getLastDropData())",
                    &last_drop_data));

    last_drop_data_ = last_drop_data;
  }

  void FullscreenTestHelper(const std::string& test_name,
                            const std::string& test_dir) {
    TestHelper(test_name, test_dir, NO_TEST_SERVER);
    content::WebContents* embedder_web_contents =
        GetFirstAppWindowWebContents();
    ASSERT_TRUE(embedder_web_contents);
    ASSERT_TRUE(guest_web_contents());
    // Click the guest to request fullscreen.
    ExtensionTestMessageListener passed_listener(
        "FULLSCREEN_STEP_PASSED", false);
    passed_listener.set_failure_message("TEST_FAILED");
    content::SimulateMouseClickAt(guest_web_contents(),
                                  0,
                                  blink::WebMouseEvent::Button::Left,
                                  gfx::Point(20, 20));
    ASSERT_TRUE(passed_listener.WaitUntilSatisfied());
  }

 protected:
  TestGuestViewManagerFactory factory_;
  content::WebContents* guest_web_contents_;
  content::WebContents* embedder_web_contents_;
  gfx::Point corner_;
  bool mouse_click_result_;
  bool first_click_;
  // Only used in drag/drop test.
  base::Closure quit_closure_;
  std::string last_drop_data_;
};

// The following class of tests work for both OOPIF and non-OOPIF <webview>.
class WebViewInteractiveTest : public WebViewInteractiveTestBase,
                               public testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewInteractiveTestBase::SetUpCommandLine(command_line);

    bool use_cross_process_frames_for_guests = GetParam();
    if (use_cross_process_frames_for_guests) {
      command_line->AppendSwitchASCII(
          switches::kEnableFeatures,
          ::features::kGuestViewCrossProcessFrames.name);
    } else {
      command_line->AppendSwitchASCII(
          switches::kDisableFeatures,
          ::features::kGuestViewCrossProcessFrames.name);
    }
  }
};

class WebViewNewWindowInteractiveTest : public WebViewInteractiveTest {};

// The tests below aren't needed in --use-cross-process-frames-for-guests.
class WebViewContextMenuInteractiveTest : public WebViewInteractiveTestBase {};

// The following class of tests do not work for OOPIF <webview>.
// TODO(ekaramad): Make this tests work with OOPIF and replace the test classes
// with WebViewInteractiveTest (see crbug.com/582562).
class WebViewFocusInteractiveTest : public WebViewInteractiveTestBase {};
class WebViewPopupInteractiveTest : public WebViewInteractiveTestBase {};
class WebViewPointerLockInteractiveTest : public WebViewInteractiveTestBase {};
class WebViewDragDropInteractiveTest : public WebViewInteractiveTestBase {};

INSTANTIATE_TEST_CASE_P(WebViewInteractiveTests,
                        WebViewInteractiveTest,
                        testing::Bool());

INSTANTIATE_TEST_CASE_P(WebViewInteractiveTests,
                        WebViewNewWindowInteractiveTest,
                        testing::Bool());

// ui_test_utils::SendMouseMoveSync doesn't seem to work on OS_MACOSX, and
// likely won't work on many other platforms as well, so for now this test
// is for Windows and Linux only. As of Sept 17th, 2013 this test is disabled
// on Windows due to flakines, see http://crbug.com/293445.

// Disabled on Linux Aura because pointer lock does not work on Linux Aura.
// crbug.com/341876

#if defined(OS_LINUX)
// flaky http://crbug.com/412086
IN_PROC_BROWSER_TEST_F(WebViewPointerLockInteractiveTest,
                       DISABLED_PointerLock) {
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

#if defined(OS_WIN)
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
  move_captured_listener.set_failure_message("timeout");

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

// flaky http://crbug.com/412086
IN_PROC_BROWSER_TEST_F(WebViewPointerLockInteractiveTest,
                       DISABLED_PointerLockFocus) {
  SetupTest("web_view/pointer_lock_focus",
            "/extensions/platform_apps/web_view/pointer_lock_focus/guest.html");

  // Move the mouse over the Lock Pointer button.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
      gfx::Point(corner().x() + 75, corner().y() + 25)));

  // Click the Lock Pointer button, locking the mouse to lockTarget.
  // This will also change focus to another element
  SendMouseClickWithListener(ui_controls::LEFT, "locked");

  // Try to unlock the mouse now that the focus is outside of the BrowserPlugin
  ExtensionTestMessageListener unlocked_listener("unlocked", false);
  // Send a key press to unlock the mouse.
  SendKeyPressToPlatformApp(ui::VKEY_ESCAPE);

  // Wait for page to receive (successful) mouse unlock response.
  ASSERT_TRUE(unlocked_listener.WaitUntilSatisfied());
}

#endif  // defined(OS_LINUX)

// Tests that if a <webview> is focused before navigation then the guest starts
// off focused.
IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest,
                       Focus_FocusBeforeNavigation) {
  TestHelper("testFocusBeforeNavigation", "web_view/focus", NO_TEST_SERVER);
}

// Tests that setting focus on the <webview> sets focus on the guest.
IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest, Focus_FocusEvent) {
  TestHelper("testFocusEvent", "web_view/focus", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest, Focus_FocusTracksEmbedder) {
  content::WebContents* embedder_web_contents = NULL;

  std::unique_ptr<ExtensionTestMessageListener> done_listener(
      RunAppHelper("testFocusTracksEmbedder", "web_view/focus", NO_TEST_SERVER,
                   &embedder_web_contents));
  done_listener->WaitUntilSatisfied();

  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");
  EXPECT_TRUE(content::ExecuteScript(
                  embedder_web_contents,
                  "window.runCommand('testFocusTracksEmbedderRunNextStep');"));

  // Blur the embedder.
  embedder_web_contents->GetRenderViewHost()->GetWidget()->Blur();
  // Ensure that the guest is also blurred.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest, Focus_AdvanceFocus) {
  content::WebContents* embedder_web_contents = NULL;

  {
    std::unique_ptr<ExtensionTestMessageListener> done_listener(
        RunAppHelper("testAdvanceFocus", "web_view/focus", NO_TEST_SERVER,
                     &embedder_web_contents));
    done_listener->WaitUntilSatisfied();
  }

  {
    ExtensionTestMessageListener listener("button1-focused", false);
    listener.set_failure_message("TEST_FAILED");
    SimulateRWHMouseClick(
        embedder_web_contents->GetRenderViewHost()->GetWidget(),
        blink::WebMouseEvent::Button::Left, 200, 20);
    content::SimulateKeyPress(embedder_web_contents, ui::DomKey::TAB,
                              ui::DomCode::TAB, ui::VKEY_TAB, false, false,
                              false, false);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  {
    // Wait for button1 to be focused again, this means we were asked to
    // move the focus to the next focusable element.
    ExtensionTestMessageListener listener("button1-advance-focus", false);
    listener.set_failure_message("TEST_FAILED");
    // TODO(fsamuel): A third Tab key press should not be necessary.
    // The <webview> will take keyboard focus but it will not focus an initial
    // element. The initial element is dependent upon tab direction which blink
    // does not propagate to the plugin.
    // See http://crbug.com/147644.
    content::SimulateKeyPress(embedder_web_contents, ui::DomKey::TAB,
                              ui::DomCode::TAB, ui::VKEY_TAB, false, false,
                              false, false);
    content::SimulateKeyPress(embedder_web_contents, ui::DomKey::TAB,
                              ui::DomCode::TAB, ui::VKEY_TAB, false, false,
                              false, false);
    content::SimulateKeyPress(embedder_web_contents, ui::DomKey::TAB,
                              ui::DomCode::TAB, ui::VKEY_TAB, false, false,
                              false, false);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }
}

// Tests that blurring <webview> also blurs the guest.
IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest, Focus_BlurEvent) {
  TestHelper("testBlurEvent", "web_view/focus", NO_TEST_SERVER);
}

// Tests that guests receive edit commands and respond appropriately.
IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest, EditCommands) {
#if defined(OS_MACOSX)
  // TODO(ekaramad): This test is failing under OOPIF in MAC.
  if (GetParam())
    return;
#endif
  LoadAndLaunchPlatformApp("web_view/edit_commands", "connected");

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
// http://crbug.com/417892
IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest, DISABLED_EditCommandsNoMenu) {
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

IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest,
                       NewWindow_AttachAfterOpenerDestroyed) {
  TestHelper("testNewWindowAttachAfterOpenerDestroyed",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest,
                       NewWindow_NewWindowNameTakesPrecedence) {
  TestHelper("testNewWindowNameTakesPrecedence",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest,
                       NewWindow_WebViewNameTakesPrecedence) {
  TestHelper("testNewWindowWebViewNameTakesPrecedence", "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest, NewWindow_NoName) {
  TestHelper("testNewWindowNoName",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

// Flaky on win_chromium_rel_ng. https://crbug.com/504054
#if defined(OS_WIN)
#define MAYBE_NewWindow_Redirect DISABLED_NewWindow_Redirect
#else
#define MAYBE_NewWindow_Redirect NewWindow_Redirect
#endif
IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest,
                       MAYBE_NewWindow_Redirect) {
  TestHelper("testNewWindowRedirect",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest, NewWindow_Close) {
  TestHelper("testNewWindowClose",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest,
                       NewWindow_DeferredAttachment) {
  TestHelper("testNewWindowDeferredAttachment",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest,
                       NewWindow_ExecuteScript) {
  TestHelper("testNewWindowExecuteScript",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest,
                       NewWindow_DeclarativeWebRequest) {
  TestHelper("testNewWindowDeclarativeWebRequest",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest,
                       NewWindow_DiscardAfterOpenerDestroyed) {
  TestHelper("testNewWindowDiscardAfterOpenerDestroyed",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

// Causes problems on windows: http://crbug.com/544037
#if defined(OS_WIN)
#define MAYBE_NewWindow_WebRequest DISABLED_NewWindow_WebRequest
#else
#define MAYBE_NewWindow_WebRequest NewWindow_WebRequest
#endif
IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest,
                       MAYBE_NewWindow_WebRequest) {
  TestHelper("testNewWindowWebRequest",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

// A custom elements bug needs to be addressed to enable this test:
// See http://crbug.com/282477 for more information.
IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest,
                       DISABLED_NewWindow_WebRequestCloseWindow) {
  TestHelper("testNewWindowWebRequestCloseWindow",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest,
                       NewWindow_WebRequestRemoveElement) {
  TestHelper("testNewWindowWebRequestRemoveElement",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

// There is a problem of missing keyup events with the command key after
// the NSEvent is sent to NSApplication in ui/base/test/ui_controls_mac.mm .
// This test is disabled on only the Mac until the problem is resolved.
// See http://crbug.com/425859 for more information.
#if !defined(OS_MACOSX)
// Tests that Ctrl+Click/Cmd+Click on a link fires up the newwindow API.
IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest, NewWindow_OpenInNewTab) {
  content::WebContents* embedder_web_contents = NULL;

  ExtensionTestMessageListener loaded_listener("Loaded", false);
  std::unique_ptr<ExtensionTestMessageListener> done_listener(
      RunAppHelper("testNewWindowOpenInNewTab", "web_view/newwindow",
                   NEEDS_TEST_SERVER, &embedder_web_contents));

  loaded_listener.WaitUntilSatisfied();
#if defined(OS_MACOSX)
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_RETURN,
      false, false, false, true /* cmd */));
#else
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_RETURN,
      true /* ctrl */, false, false, false));
#endif

  // Wait for the embedder to receive a 'newwindow' event.
  ASSERT_TRUE(done_listener->WaitUntilSatisfied());
}
#endif

IN_PROC_BROWSER_TEST_P(WebViewNewWindowInteractiveTest,
                       NewWindow_OpenerDestroyedWhileUnattached) {
  TestHelper("testNewWindowOpenerDestroyedWhileUnattached",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
  ASSERT_EQ(2u, GetGuestViewManager()->num_guests_created());

  // We have two guests in this test, one is the intial one, the other
  // is the newwindow one.
  // Before the embedder goes away, both the guests should go away.
  // This ensures that unattached guests are gone if opener is gone.
  GetGuestViewManager()->WaitForAllGuestsDeleted();
}

// Tests whether <webview> context menu sees <webview> local coordinates
// in its RenderViewContextMenu params.
// Local coordinates are required for plugin actions to work properly.
//
// This test is not needed in --use-cross-process-frames-for-guests,
// since it tests that the <webview> sees local coordinates, which is not
// true with the out-of-process iframes architecture. In that case, the
// <webview> sees transformed coordinates, the point is transformed in
// CrossProcessFrameConnector::TransformPointToRootCoordSpace.
IN_PROC_BROWSER_TEST_F(WebViewContextMenuInteractiveTest,
                       ContextMenuParamCoordinates) {
  TestHelper("testCoordinates", "web_view/context_menus/coordinates",
             NO_TEST_SERVER);
  ASSERT_TRUE(guest_web_contents());

  ContextMenuWaiter menu_observer(content::NotificationService::AllSources());
  SimulateRWHMouseClick(guest_web_contents()->GetRenderViewHost()->GetWidget(),
                        blink::WebMouseEvent::Button::Right, 10, 20);
  // Wait until the context menu is opened and closed.
  menu_observer.WaitForMenuOpenAndClose();
  ASSERT_EQ(10, menu_observer.params().x);
  ASSERT_EQ(20, menu_observer.params().y);
}

// Tests whether <webview> context menu sees <webview> local coordinates in its
// RenderViewContextMenu params, when it is subject to CSS transforms.
//
// This test doesn't makes sense in --use-cross-process-frames-for-guests, since
// it tests that events forwarded from the embedder are properly transformed,
// and in oopif-mode the events are sent directly to the child process without
// the forwarding code path (relying on surface-based hittesting).

// Flaky.  http://crbug.com/613258
IN_PROC_BROWSER_TEST_F(WebViewContextMenuInteractiveTest,
                       DISABLED_ContextMenuParamsAfterCSSTransforms) {
  LoadAndLaunchPlatformApp("web_view/context_menus/coordinates_with_transforms",
                           "Launched");

  if (!embedder_web_contents_)
    embedder_web_contents_ = GetFirstAppWindowWebContents();
  EXPECT_TRUE(embedder_web_contents());

  if (!guest_web_contents_)
    guest_web_contents_ = GetGuestViewManager()->WaitForSingleGuestCreated();
  EXPECT_TRUE(guest_web_contents());

  // We will send the input event to the embedder rather than the guest; which
  // is more realistic. We need to do this to make sure that the MouseDown event
  // is received forwarded by the BrowserPlugin to the RWHVG and eventually back
  // to the guest. The RWHVG will in turn notify the ChromeWVGDelegate of the
  // newly observed mouse down (potentially a context menu).
  const std::string transforms[] = {"rotate(20deg)", "scale(1.5, 2.0)",
                                    "translate(20px, 30px)", "NONE"};
  for (size_t index = 0; index < 4; ++index) {
    std::string command =
        base::StringPrintf("setTransform('%s')", transforms[index].c_str());
    ExtensionTestMessageListener transform_set_listener("TRANSFORM_SET", false);
    EXPECT_TRUE(content::ExecuteScript(embedder_web_contents(), command));
    ASSERT_TRUE(transform_set_listener.WaitUntilSatisfied());

    gfx::Rect embedder_view_bounds =
        embedder_web_contents()->GetRenderWidgetHostView()->GetViewBounds();
    gfx::Rect guest_view_bounds =
        guest_web_contents()->GetRenderWidgetHostView()->GetViewBounds();
    ContextMenuWaiter menu_observer(content::NotificationService::AllSources());
    gfx::Point guest_window_point(150, 150);
    gfx::Point embedder_window_point = guest_window_point;
    embedder_window_point += guest_view_bounds.OffsetFromOrigin();
    embedder_window_point -= embedder_view_bounds.OffsetFromOrigin();
    SimulateRWHMouseClick(
        embedder_web_contents()->GetRenderViewHost()->GetWidget(),
        blink::WebMouseEvent::Button::Right,
        /* Using window coordinates for the embedder */
        embedder_window_point.x(), embedder_window_point.y());

    menu_observer.WaitForMenuOpenAndClose();
    EXPECT_EQ(menu_observer.params().x, guest_window_point.x());
    EXPECT_EQ(menu_observer.params().y, guest_window_point.y());
  }
}

IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest, ExecuteCode) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "execute_code")) << message_;
}

// Causes problems on windows: http://crbug.com/544037
#if defined(OS_WIN)
#define MAYBE_PopupPositioningBasic DISABLED_PopupPositioningBasic
#else
#define MAYBE_PopupPositioningBasic PopupPositioningBasic
#endif
IN_PROC_BROWSER_TEST_F(WebViewPopupInteractiveTest,
                       MAYBE_PopupPositioningBasic) {
  TestHelper("testBasic", "web_view/popup_positioning", NO_TEST_SERVER);
  ASSERT_TRUE(guest_web_contents());
  PopupTestHelper(gfx::Point());

  // TODO(lazyboy): Move the embedder window to a random location and
  // make sure we keep rendering popups correct in webview.
}

// Flaky on ChromeOS and Linux: http://crbug.com/526886
// Causes problems on windows: http://crbug.com/544998
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_PopupPositioningMoved DISABLED_PopupPositioningMoved
#else
#define MAYBE_PopupPositioningMoved PopupPositioningMoved
#endif
// Tests that moving browser plugin (without resize/UpdateRects) correctly
// repositions popup.
IN_PROC_BROWSER_TEST_F(WebViewPopupInteractiveTest,
                       MAYBE_PopupPositioningMoved) {
  TestHelper("testMoved", "web_view/popup_positioning", NO_TEST_SERVER);
  ASSERT_TRUE(guest_web_contents());
  PopupTestHelper(gfx::Point(20, 0));
}

// Drag and drop inside a webview is currently only enabled for linux and mac,
// but the tests don't work on anything except chromeos for now. This is because
// of simulating mouse drag code's dependency on platforms.
#if defined(OS_CHROMEOS) && !defined(USE_OZONE)
IN_PROC_BROWSER_TEST_F(WebViewDragDropInteractiveTest, DragDropWithinWebView) {
  LoadAndLaunchPlatformApp("web_view/dnd_within_webview", "connected");
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(GetPlatformAppWindow()));

  embedder_web_contents_ = GetFirstAppWindowWebContents();
  gfx::Rect offset = embedder_web_contents_->GetContainerBounds();
  corner_ = gfx::Point(offset.x(), offset.y());

  // In the drag drop test we add 20px padding to the page body because on
  // windows if we get too close to the edge of the window the resize cursor
  // appears and we start dragging the window edge.
  corner_.Offset(20, 20);

  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();
  for (;;) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&WebViewInteractiveTestBase::DragTestStep1,
                              base::Unretained(this)));
    run_loop.Run();

    if (last_drop_data_ == "Drop me")
      break;

    LOG(INFO) << "Drag was cancelled in interactive_test, restarting drag";

    // Reset state for next try.
    ExtensionTestMessageListener reset_listener("resetStateReply", false);
    EXPECT_TRUE(content::ExecuteScript(embedder_web_contents_,
                                       "window.resetState()"));
    ASSERT_TRUE(reset_listener.WaitUntilSatisfied());
  }
  ASSERT_EQ("Drop me", last_drop_data_);
}
#endif  // (defined(OS_CHROMEOS))

IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest, Navigation) {
  TestHelper("testNavigation", "web_view/navigation", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest, Navigation_BackForwardKeys) {
  LoadAndLaunchPlatformApp("web_view/navigation", "Launched");
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetPlatformAppWindow()));
  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);

  ExtensionTestMessageListener done_listener(
      "TEST_PASSED", false);
  done_listener.set_failure_message("TEST_FAILED");
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

IN_PROC_BROWSER_TEST_F(WebViewPointerLockInteractiveTest,
                       PointerLock_PointerLockLostWithFocus) {
  TestHelper("testPointerLockLostWithFocus",
             "web_view/pointerlock",
             NO_TEST_SERVER);
}

// Disable this on mac, throws an assertion failure on teardown which
// will result in flakiness:
//
// "not is fullscreen state"
// "*** Assertion failure in -[_NSWindowFullScreenTransition
//     transitionedWindowFrame],"
// See similar bug: http://crbug.com/169820.
//
// In addition to the above, these tests are flaky on many platforms:
// http://crbug.com/468660
IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest,
                       DISABLED_FullscreenAllow_EmbedderHasPermission) {
  FullscreenTestHelper("testFullscreenAllow",
                       "web_view/fullscreen/embedder_has_permission");
}

IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest,
                       DISABLED_FullscreenDeny_EmbedderHasPermission) {
  FullscreenTestHelper("testFullscreenDeny",
                       "web_view/fullscreen/embedder_has_permission");
}

IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest,
                       DISABLED_FullscreenAllow_EmbedderHasNoPermission) {
  FullscreenTestHelper("testFullscreenAllow",
                       "web_view/fullscreen/embedder_has_no_permission");
}

IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest,
                       DISABLED_FullscreenDeny_EmbedderHasNoPermission) {
  FullscreenTestHelper("testFullscreenDeny",
                       "web_view/fullscreen/embedder_has_no_permission");
}

// This test exercies the following scenario:
// 1. An <input> in guest has focus.
// 2. User takes focus to embedder by clicking e.g. an <input> in embedder.
// 3. User brings back the focus directly to the <input> in #1.
//
// Now we need to make sure TextInputTypeChanged fires properly for the guest's
// view upon step #3. We simply read the input type's state after #3 to
// make sure it's not TEXT_INPUT_TYPE_NONE.
IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest, Focus_FocusRestored) {
  TestHelper("testFocusRestored", "web_view/focus", NO_TEST_SERVER);
  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);
  ASSERT_TRUE(guest_web_contents());

  // 1) We click on the guest so that we get a focus event.
  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");
  {
    content::SimulateMouseClickAt(guest_web_contents(),
                                  0,
                                  blink::WebMouseEvent::Button::Left,
                                  gfx::Point(10, 10));
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    "window.runCommand('testFocusRestoredRunNextStep', 1);"));
  }
  // Wait for the next step to complete.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  // 2) We click on the embedder so the guest's focus goes away and it observes
  // a blur event.
  next_step_listener.Reset();
  {
    content::SimulateMouseClickAt(embedder_web_contents,
                                  0,
                                  blink::WebMouseEvent::Button::Left,
                                  gfx::Point(200, 20));
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    "window.runCommand('testFocusRestoredRunNextStep', 2);"));
  }
  // Wait for the next step to complete.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  // 3) We click on the guest again to bring back focus directly to the previous
  // input element, then we ensure text_input_type is properly set.
  next_step_listener.Reset();
  {
    content::SimulateMouseClickAt(guest_web_contents(),
                                  0,
                                  blink::WebMouseEvent::Button::Left,
                                  gfx::Point(10, 10));
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    "window.runCommand('testFocusRestoredRunNextStep', 3)"));
  }
  // Wait for the next step to complete.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  // |text_input_client| is not available for mac and android.
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  ui::TextInputClient* text_input_client =
      embedder_web_contents->GetRenderViewHost()
          ->GetWidget()
          ->GetView()
          ->GetTextInputClient();
  ASSERT_TRUE(text_input_client);
  ASSERT_TRUE(text_input_client->GetTextInputType() !=
              ui::TEXT_INPUT_TYPE_NONE);
#endif
}

// ui::TextInputClient is NULL for mac and android.
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest, DISABLED_Focus_InputMethod) {
  content::WebContents* embedder_web_contents = NULL;
  std::unique_ptr<ExtensionTestMessageListener> done_listener(
      RunAppHelper("testInputMethod", "web_view/focus", NO_TEST_SERVER,
                   &embedder_web_contents));
  ASSERT_TRUE(done_listener->WaitUntilSatisfied());

  ui::TextInputClient* text_input_client =
      embedder_web_contents->GetRenderViewHost()
          ->GetWidget()
          ->GetView()
          ->GetTextInputClient();
  ASSERT_TRUE(text_input_client);

  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");

  // An input element inside the <webview> gets focus and is given some
  // user input via IME.
  {
    ui::CompositionText composition;
    composition.text = base::UTF8ToUTF16("InputTest123");
    text_input_client->SetCompositionText(composition);
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    "window.runCommand('testInputMethodRunNextStep', 1);"));

    // Wait for the next step to complete.
    ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
  }

  // A composition is committed via IME.
  {
    next_step_listener.Reset();

    ui::CompositionText composition;
    composition.text = base::UTF8ToUTF16("InputTest456");
    text_input_client->SetCompositionText(composition);
    text_input_client->ConfirmCompositionText();
    EXPECT_TRUE(content::ExecuteScript(
                  embedder_web_contents,
                  "window.runCommand('testInputMethodRunNextStep', 2);"));

    // Wait for the next step to complete.
    ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
  }

  // TODO(lazyboy): http://crbug.com/457007, Add a step or a separate test to
  // check the following, currently it turns this test to be flaky:
  // If we move the focus from the first <input> to the second one after we
  // have some composition text set but *not* committed (by calling
  // text_input_client->SetCompositionText()), then it would cause IME cancel
  // and the onging composition is committed in the first <input> in the
  // <webview>, not the second one.

  // Tests ExtendSelectionAndDelete message works in <webview>.
  {
    next_step_listener.Reset();

    // At this point we have set focus on first <input> in the <webview>,
    // and the value it contains is 'InputTest456' with caret set after 'T'.
    // Now we delete 'Test' in 'InputTest456', as the caret is after 'T':
    // delete before 1 character ('T') and after 3 characters ('est').
    text_input_client->ExtendSelectionAndDelete(1, 3);
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    "window.runCommand('testInputMethodRunNextStep', 3);"));

    // Wait for the next step to complete.
    ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
  }
}
#endif

#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest, TextSelection) {
#if defined(OS_MACOSX)
  // TODO(ekaramad): This test is failing under OOPIF for MAC.
  if (GetParam())
    return;
#endif
  SetupTest("web_view/text_selection",
            "/extensions/platform_apps/web_view/text_selection/guest.html");
  ASSERT_TRUE(guest_web_contents());
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetPlatformAppWindow()));

  // Wait until guest sees a context menu, select an arbitrary item (copy).
  ExtensionTestMessageListener ctx_listener("MSG_CONTEXTMENU", false);
  ContextMenuNotificationObserver menu_observer(IDC_CONTENT_CONTEXT_COPY);
  SimulateRWHMouseClick(guest_web_contents()->GetRenderViewHost()->GetWidget(),
                        blink::WebMouseEvent::Button::Right, 20, 20);
  ASSERT_TRUE(ctx_listener.WaitUntilSatisfied());

  // Now verify that the selection text propagates properly to RWHV.
  content::RenderWidgetHostView* guest_rwhv =
      guest_web_contents()->GetRenderWidgetHostView();
  ASSERT_TRUE(guest_rwhv);
  std::string selected_text = base::UTF16ToUTF8(guest_rwhv->GetSelectedText());
  ASSERT_TRUE(selected_text.size() >= 10u);
  ASSERT_EQ("AAAAAAAAAA", selected_text.substr(0, 10));
}
#endif

// Flaky on MacOS builders. https://crbug.com/670008
#if defined(OS_MACOSX)
#define MAYBE_FocusAndVisibility DISABLED_FocusAndVisibility
#else
#define MAYBE_FocusAndVisibility FocusAndVisibility
#endif

IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest, MAYBE_FocusAndVisibility) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  LoadAndLaunchPlatformApp("web_view/focus_visibility",
                           "WebViewInteractiveTest.LOADED");
  ExtensionTestMessageListener test_init_listener(
      "WebViewInteractiveTest.WebViewInitialized", false);
  SendMessageToEmbedder("init");
  test_init_listener.WaitUntilSatisfied();
  // Send several tab-keys. The button inside webview should receive focus at
  // least once.
  for (size_t i = 0; i < 4; ++i)
    SendKeyPressToPlatformApp(ui::VKEY_TAB);
  ExtensionTestMessageListener webview_button_focused_listener(
      "WebViewInteractiveTest.WebViewButtonWasFocused", false);
  webview_button_focused_listener.set_failure_message(
      "WebViewInteractiveTest.WebViewButtonWasNotFocused");
  SendMessageToEmbedder("verify");
  EXPECT_TRUE(webview_button_focused_listener.WaitUntilSatisfied());
  // Now make the <webview> invisible.
  ExtensionTestMessageListener reset_listener("WebViewInteractiveTest.DidReset",
                                              false);
  SendMessageToEmbedder("reset");
  reset_listener.WaitUntilSatisfied();
  ExtensionTestMessageListener did_hide_webview_listener(
      "WebViewInteractiveTest.DidHideWebView", false);
  SendMessageToEmbedder("hide-webview");
  did_hide_webview_listener.WaitUntilSatisfied();
  // Send the same number of keys and verify that the webview button was not
  // this time.
  for (size_t i = 0; i < 4; ++i)
    SendKeyPressToPlatformApp(ui::VKEY_TAB);
  ExtensionTestMessageListener webview_button_not_focused_listener(
      "WebViewInteractiveTest.WebViewButtonWasNotFocused", false);
  webview_button_not_focused_listener.set_failure_message(
      "WebViewInteractiveTest.WebViewButtonWasFocused");
  SendMessageToEmbedder("verify");
  EXPECT_TRUE(webview_button_not_focused_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest, KeyboardFocusSimple) {
  TestHelper("testKeyboardFocusSimple", "web_view/focus", NO_TEST_SERVER);

  EXPECT_EQ(embedder_web_contents()->GetFocusedFrame(),
            embedder_web_contents()->GetMainFrame());
  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");
  {
    gfx::Rect offset = embedder_web_contents()->GetContainerBounds();
    // Click the <input> element inside the <webview>.
    // If we wanted, we could ask the embedder to compute an appropriate point.
    MoveMouseInsideWindow(gfx::Point(offset.x() + 40, offset.y() + 40));
    SendMouseClick(ui_controls::LEFT);
  }

  // Waits for the renderer to know the input has focus.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_A, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_B, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_C, false, false, false, false));

  next_step_listener.Reset();
  EXPECT_TRUE(content::ExecuteScript(
      embedder_web_contents(),
      "window.runCommand('testKeyboardFocusRunNextStep', 'aBc');"));

  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
}

// Ensures that input is routed to the webview after the containing window loses
// and regains focus. Additionally, the webview does not process keypresses sent
// while another window is focused.
// http://crbug.com/660044.
IN_PROC_BROWSER_TEST_P(WebViewInteractiveTest, KeyboardFocusWindowCycle) {
  TestHelper("testKeyboardFocusWindowFocusCycle", "web_view/focus",
             NO_TEST_SERVER);

  EXPECT_EQ(embedder_web_contents()->GetFocusedFrame(),
            embedder_web_contents()->GetMainFrame());
  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");
  {
    gfx::Rect offset = embedder_web_contents()->GetContainerBounds();
    // Click the <input> element inside the <webview>.
    // If we wanted, we could ask the embedder to compute an appropriate point.
    MoveMouseInsideWindow(gfx::Point(offset.x() + 40, offset.y() + 40));
    SendMouseClick(ui_controls::LEFT);
  }

  // Waits for the renderer to know the input has focus.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_A, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_B, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_C, false, false, false, false));

  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("minimal", "Launched");
  extensions::AppWindow* window = GetFirstAppWindowForApp(extension->id());
  EXPECT_TRUE(content::ExecuteScript(
      embedder_web_contents(),
      "window.runCommand('monitorGuestEvent', 'focus');"));

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_F, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_O, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_O, false, true, false, false));

  // Close the other window and wait for the webview to regain focus.
  CloseAppWindow(window);
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(GetPlatformAppWindow()));
  next_step_listener.Reset();
  EXPECT_TRUE(
      content::ExecuteScript(embedder_web_contents(),
                             "window.runCommand('waitGuestEvent', 'focus');"));
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_X, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_Y, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_Z, false, false, false, false));

  next_step_listener.Reset();
  EXPECT_TRUE(content::ExecuteScript(
      embedder_web_contents(),
      "window.runCommand('testKeyboardFocusRunNextStep', 'aBcxYz');"));

  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
}
