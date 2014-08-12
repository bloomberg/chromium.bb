// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/guest_view_base.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/guest_view_manager_factory.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes.h"

using apps::AppWindow;

class TestGuestViewManager : public extensions::GuestViewManager {
 public:
  explicit TestGuestViewManager(content::BrowserContext* context) :
      GuestViewManager(context),
      web_contents_(NULL) {}

  content::WebContents* WaitForGuestCreated() {
    if (web_contents_)
      return web_contents_;

    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    return web_contents_;
  }

 private:
  // GuestViewManager override:
  virtual void AddGuest(int guest_instance_id,
                        content::WebContents* guest_web_contents) OVERRIDE{
    extensions::GuestViewManager::AddGuest(
        guest_instance_id, guest_web_contents);
    web_contents_ = guest_web_contents;

    if (message_loop_runner_)
      message_loop_runner_->Quit();
  }

  content::WebContents* web_contents_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

// Test factory for creating test instances of GuestViewManager.
class TestGuestViewManagerFactory : public extensions::GuestViewManagerFactory {
 public:
  TestGuestViewManagerFactory() :
      test_guest_view_manager_(NULL) {}

  virtual ~TestGuestViewManagerFactory() {}

  virtual extensions::GuestViewManager* CreateGuestViewManager(
      content::BrowserContext* context) OVERRIDE {
    return GetManager(context);
  }

  TestGuestViewManager* GetManager(content::BrowserContext* context) {
    if (!test_guest_view_manager_) {
      test_guest_view_manager_ = new TestGuestViewManager(context);
    }
    return test_guest_view_manager_;
  }

 private:
  TestGuestViewManager* test_guest_view_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestGuestViewManagerFactory);
};

class WebViewInteractiveTest
    : public extensions::PlatformAppBrowserTest {
 public:
  WebViewInteractiveTest()
      : guest_web_contents_(NULL),
        embedder_web_contents_(NULL),
        corner_(gfx::Point()),
        mouse_click_result_(false),
        first_click_(true) {
    extensions::GuestViewManager::set_factory_for_testing(&factory_);
  }

  TestGuestViewManager* GetGuestViewManager() {
    return factory_.GetManager(browser()->profile());
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
    const apps::AppWindowRegistry::AppWindowList& app_windows =
        apps::AppWindowRegistry::Get(browser()->profile())->app_windows();
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

  scoped_ptr<ExtensionTestMessageListener> RunAppHelper(
      const std::string& test_name,
      const std::string& app_location,
      TestServer test_server,
      content::WebContents** embedder_web_contents) {
    // For serving guest pages.
    if ((test_server == NEEDS_TEST_SERVER) && !StartEmbeddedTestServer()) {
      LOG(ERROR) << "FAILED TO START TEST SERVER.";
      return scoped_ptr<ExtensionTestMessageListener>();
    }

    LoadAndLaunchPlatformApp(app_location.c_str(), "Launched");
    if (!ui_test_utils::ShowAndFocusNativeWindow(GetPlatformAppWindow())) {
      LOG(ERROR) << "UNABLE TO FOCUS TEST WINDOW.";
      return scoped_ptr<ExtensionTestMessageListener>();
    }

    // Flush any pending events to make sure we start with a clean slate.
    content::RunAllPendingInMessageLoop();

    *embedder_web_contents = GetFirstAppWindowWebContents();

    scoped_ptr<ExtensionTestMessageListener> done_listener(
        new ExtensionTestMessageListener("TEST_PASSED", false));
    done_listener->set_failure_message("TEST_FAILED");
    if (!content::ExecuteScript(
            *embedder_web_contents,
            base::StringPrintf("runTest('%s')", test_name.c_str()))) {
      LOG(ERROR) << "UNABLE TO START TEST";
      return scoped_ptr<ExtensionTestMessageListener>();
    }

    return done_listener.Pass();
  }

  void TestHelper(const std::string& test_name,
                  const std::string& app_location,
                  TestServer test_server) {
    content::WebContents* embedder_web_contents = NULL;
    scoped_ptr<ExtensionTestMessageListener> done_listener(
        RunAppHelper(
            test_name, app_location, test_server, &embedder_web_contents));

    ASSERT_TRUE(done_listener);
    ASSERT_TRUE(done_listener->WaitUntilSatisfied());

    guest_web_contents_ = GetGuestViewManager()->WaitForGuestCreated();
  }

  void RunTest(const std::string& app_name) {
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

    LoadAndLaunchPlatformApp(app_name.c_str(), "connected");

    guest_observer.Wait();
    content::Source<content::NavigationController> source =
        guest_observer.source();
    EXPECT_TRUE(source->GetWebContents()->GetRenderProcessHost()->
        IsIsolatedGuest());

    guest_web_contents_ = source->GetWebContents();
    embedder_web_contents_ =
        extensions::GuestViewBase::FromWebContents(guest_web_contents_)->
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

    mouse_event.type = blink::WebInputEvent::MouseDown;
    rwh->ForwardMouseEvent(mouse_event);
    mouse_event.type = blink::WebInputEvent::MouseUp;
    rwh->ForwardMouseEvent(mouse_event);
  }

  class PopupCreatedObserver {
   public:
    explicit PopupCreatedObserver()
        : initial_widget_count_(0),
          last_render_widget_host_(NULL),
          seen_new_widget_(false) {}

    ~PopupCreatedObserver() {}

    void Wait() {
      size_t current_widget_count = CountWidgets();
      if (!seen_new_widget_ &&
          current_widget_count == initial_widget_count_ + 1) {
        seen_new_widget_ = true;
      }

      // If we haven't seen any new widget or we get 0 size widget, we need to
      // schedule waiting.
      bool needs_to_schedule_wait = true;

      if (seen_new_widget_) {
        gfx::Rect popup_bounds =
            last_render_widget_host_->GetView()->GetViewBounds();
        if (!popup_bounds.size().IsEmpty())
          needs_to_schedule_wait = false;
      }

      if (needs_to_schedule_wait) {
        ScheduleWait();
      } else {
        // We are done.
        if (message_loop_)
          message_loop_->Quit();
      }

      if (!message_loop_) {
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
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&PopupCreatedObserver::Wait, base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(200));
    }

    size_t CountWidgets() {
      scoped_ptr<content::RenderWidgetHostIterator> widgets(
          content::RenderWidgetHost::GetRenderWidgetHosts());
      size_t num_widgets = 0;
      while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
        if (widget->IsRenderView())
          continue;
        ++num_widgets;
        last_render_widget_host_ = widget;
      }
      return num_widgets;
    }

    size_t initial_widget_count_;
    content::RenderWidgetHost* last_render_widget_host_;
    scoped_refptr<content::MessageLoopRunner> message_loop_;
    bool seen_new_widget_;

    DISALLOW_COPY_AND_ASSIGN(PopupCreatedObserver);
  };

  void WaitForTitle(const char* title) {
    base::string16 expected_title(base::ASCIIToUTF16(title));
    base::string16 error_title(base::ASCIIToUTF16("FAILED"));
    content::TitleWatcher title_watcher(guest_web_contents(), expected_title);
    title_watcher.AlsoWaitForTitle(error_title);
    ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void PopupTestHelper(const gfx::Point& padding) {
    PopupCreatedObserver popup_observer;
    popup_observer.Init();
    // Press alt+DOWN to open popup.
    bool alt = true;
    content::SimulateKeyPress(
        guest_web_contents(), ui::VKEY_DOWN, false, false, alt, false);
    popup_observer.Wait();

    content::RenderWidgetHost* popup_rwh =
        popup_observer.last_render_widget_host();
    gfx::Rect popup_bounds = popup_rwh->GetView()->GetViewBounds();

    content::RenderViewHost* embedder_rvh =
        GetFirstAppWindowWebContents()->GetRenderViewHost();
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

    // Close the popup.
    content::SimulateKeyPress(
        guest_web_contents(), ui::VKEY_ESCAPE, false, false, false, false);
  }

  void DragTestStep1() {
    // Move mouse to start of text.
    MoveMouseInsideWindow(gfx::Point(45, 8));
    MoveMouseInsideWindow(gfx::Point(45, 9));
    SendMouseEvent(ui_controls::LEFT, ui_controls::DOWN);

    MoveMouseInsideWindow(gfx::Point(74, 12));
    MoveMouseInsideWindow(gfx::Point(78, 12));

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

// ui_test_utils::SendMouseMoveSync doesn't seem to work on OS_MACOSX, and
// likely won't work on many other platforms as well, so for now this test
// is for Windows and Linux only. As of Sept 17th, 2013 this test is disabled
// on Windows due to flakines, see http://crbug.com/293445.

// Disabled on Linux Aura because pointer lock does not work on Linux Aura.
// crbug.com/341876

#if defined(OS_LINUX) && !defined(USE_AURA)

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

#endif  // defined(OS_LINUX) && !defined(USE_AURA)

// Tests that if a <webview> is focused before navigation then the guest starts
// off focused.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, Focus_FocusBeforeNavigation) {
  TestHelper("testFocusBeforeNavigation", "web_view/focus", NO_TEST_SERVER);
}

// Tests that setting focus on the <webview> sets focus on the guest.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, Focus_FocusEvent) {
  TestHelper("testFocusEvent", "web_view/focus", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, Focus_FocusTracksEmbedder) {
  content::WebContents* embedder_web_contents = NULL;

  scoped_ptr<ExtensionTestMessageListener> done_listener(
      RunAppHelper("testFocusTracksEmbedder", "web_view/focus", NO_TEST_SERVER,
                   &embedder_web_contents));
  done_listener->WaitUntilSatisfied();

  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");
  EXPECT_TRUE(content::ExecuteScript(
                  embedder_web_contents,
                  "window.runCommand('testFocusTracksEmbedderRunNextStep');"));

  // Blur the embedder.
  embedder_web_contents->GetRenderViewHost()->Blur();
  // Ensure that the guest is also blurred.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, Focus_AdvanceFocus) {
  content::WebContents* embedder_web_contents = NULL;

  {
    scoped_ptr<ExtensionTestMessageListener> done_listener(
        RunAppHelper("testAdvanceFocus", "web_view/focus", NO_TEST_SERVER,
                     &embedder_web_contents));
    done_listener->WaitUntilSatisfied();
  }

  {
    ExtensionTestMessageListener listener("button1-focused", false);
    listener.set_failure_message("TEST_FAILED");
    SimulateRWHMouseClick(embedder_web_contents->GetRenderViewHost(),
                          blink::WebMouseEvent::ButtonLeft, 200, 20);
    content::SimulateKeyPress(embedder_web_contents, ui::VKEY_TAB,
        false, false, false, false);
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
    content::SimulateKeyPress(embedder_web_contents, ui::VKEY_TAB,
        false, false, false, false);
    content::SimulateKeyPress(embedder_web_contents, ui::VKEY_TAB,
        false, false, false, false);
    content::SimulateKeyPress(embedder_web_contents, ui::VKEY_TAB,
        false, false, false, false);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }
}

// Tests that blurring <webview> also blurs the guest.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, Focus_BlurEvent) {
  TestHelper("testBlurEvent", "web_view/focus", NO_TEST_SERVER);
}

// Tests that guests receive edit commands and respond appropriately.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, EditCommands) {
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
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest,
                       NewWindow_WebViewNameTakesPrecedence) {
  TestHelper("testWebViewNameTakesPrecedence",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, NewWindow_NoName) {
  TestHelper("testNoName",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, NewWindow_Redirect) {
  TestHelper("testNewWindowRedirect",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, NewWindow_Close) {
  TestHelper("testNewWindowClose",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, NewWindow_ExecuteScript) {
  TestHelper("testNewWindowExecuteScript",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest,
                       NewWindow_DeclarativeWebRequest) {
  TestHelper("testNewWindowDeclarativeWebRequest",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, NewWindow_WebRequest) {
  TestHelper("testNewWindowWebRequest",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

// A custom elements bug needs to be addressed to enable this test:
// See http://crbug.com/282477 for more information.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest,
                       DISABLED_NewWindow_WebRequestCloseWindow) {
  TestHelper("testNewWindowWebRequestCloseWindow",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest,
                       NewWindow_WebRequestRemoveElement) {
  TestHelper("testNewWindowWebRequestRemoveElement",
             "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

// Tests that Ctrl+Click/Cmd+Click on a link fires up the newwindow API.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, NewWindow_OpenInNewTab) {
  content::WebContents* embedder_web_contents = NULL;

  ExtensionTestMessageListener loaded_listener("Loaded", false);
  scoped_ptr<ExtensionTestMessageListener> done_listener(
    RunAppHelper("testNewWindowOpenInNewTab",
                 "web_view/newwindow",
                 NEEDS_TEST_SERVER,
                 &embedder_web_contents));

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


IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, ExecuteCode) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "execute_code")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, PopupPositioningBasic) {
  TestHelper("testBasic", "web_view/popup_positioning", NO_TEST_SERVER);
  ASSERT_TRUE(guest_web_contents());
  PopupTestHelper(gfx::Point());

  // TODO(lazyboy): Move the embedder window to a random location and
  // make sure we keep rendering popups correct in webview.
}

// Tests that moving browser plugin (without resize/UpdateRects) correctly
// repositions popup.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, PopupPositioningMoved) {
  TestHelper("testMoved", "web_view/popup_positioning", NO_TEST_SERVER);
  ASSERT_TRUE(guest_web_contents());
  PopupTestHelper(gfx::Point(20, 0));
}

// Drag and drop inside a webview is currently only enabled for linux and mac,
// but the tests don't work on anything except chromeos for now. This is because
// of simulating mouse drag code's dependency on platforms.
#if defined(OS_CHROMEOS) && !defined(USE_OZONE)
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, DragDropWithinWebView) {
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
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&WebViewInteractiveTest::DragTestStep1,
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

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, Navigation) {
  TestHelper("testNavigation", "web_view/navigation", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, Navigation_BackForwardKeys) {
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

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest,
                       PointerLock_PointerLockLostWithFocus) {
  TestHelper("testPointerLockLostWithFocus",
             "web_view/pointerlock",
             NO_TEST_SERVER);
}

// This test exercies the following scenario:
// 1. An <input> in guest has focus.
// 2. User takes focus to embedder by clicking e.g. an <input> in embedder.
// 3. User brings back the focus directly to the <input> in #1.
//
// Now we need to make sure TextInputTypeChanged fires properly for the guest's
// view upon step #3. We simply read the input type's state after #3 to
// make sure it's not TEXT_INPUT_TYPE_NONE.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, Focus_FocusRestored) {
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
                                  blink::WebMouseEvent::ButtonLeft,
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
                                  blink::WebMouseEvent::ButtonLeft,
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
                                  blink::WebMouseEvent::ButtonLeft,
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
      embedder_web_contents->GetRenderViewHost()->GetView()
          ->GetTextInputClient();
  ASSERT_TRUE(text_input_client);
  ASSERT_TRUE(text_input_client->GetTextInputType() !=
              ui::TEXT_INPUT_TYPE_NONE);
#endif
}

// ui::TextInputClient is NULL for mac and android.
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, DISABLED_Focus_InputMethod) {
  content::WebContents* embedder_web_contents = NULL;
  scoped_ptr<ExtensionTestMessageListener> done_listener(
      RunAppHelper("testInputMethod", "web_view/focus", NO_TEST_SERVER,
                   &embedder_web_contents));
  ASSERT_TRUE(done_listener->WaitUntilSatisfied());

  ui::TextInputClient* text_input_client =
      embedder_web_contents->GetRenderViewHost()->GetView()
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
    EXPECT_TRUE(next_step_listener.WaitUntilSatisfied());
  }

  // Moving focus causes IME cancel, and the composition will be committed
  // in first <input> in the <webview>, not in the second <input>.
  {
    next_step_listener.Reset();
    ui::CompositionText composition;
    composition.text = base::UTF8ToUTF16("InputTest789");
    text_input_client->SetCompositionText(composition);
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    "window.runCommand('testInputMethodRunNextStep', 3);"));

    // Wait for the next step to complete.
    EXPECT_TRUE(next_step_listener.WaitUntilSatisfied());
  }

  // Tests ExtendSelectionAndDelete message works in <webview>.
  {
    next_step_listener.Reset();

    // At this point we have set focus on first <input> in the <webview>,
    // and the value it contains is 'InputTestABC' with caret set after 'T'.
    // Now we delete 'Test' in 'InputTestABC', as the caret is after 'T':
    // delete before 1 character ('T') and after 3 characters ('est').
    text_input_client->ExtendSelectionAndDelete(1, 3);
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    "window.runCommand('testInputMethodRunNextStep', 4);"));

    // Wait for the next step to complete.
    EXPECT_TRUE(next_step_listener.WaitUntilSatisfied());
  }
}
#endif

#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, TextSelection) {
  SetupTest("web_view/text_selection",
            "/extensions/platform_apps/web_view/text_selection/guest.html");
  ASSERT_TRUE(guest_web_contents());
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetPlatformAppWindow()));

  // Wait until guest sees a context menu, select an arbitrary item (copy).
  ExtensionTestMessageListener ctx_listener("MSG_CONTEXTMENU", false);
  ContextMenuNotificationObserver menu_observer(IDC_CONTENT_CONTEXT_COPY);
  SimulateRWHMouseClick(guest_web_contents()->GetRenderViewHost(),
                        blink::WebMouseEvent::ButtonRight, 20, 20);
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
