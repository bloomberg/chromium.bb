// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using extensions::ExtensionsAPIClient;
using extensions::MimeHandlerViewGuest;
using extensions::TestMimeHandlerViewGuest;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;

// Note: This file contains several old WebViewGuest tests which were for
// certain BrowserPlugin features and no longer made sense for the new
// WebViewGuest which is based on cross-process frames. Since
// MimeHandlerViewGuest is the only guest which still uses BrowserPlugin, the
// test were moved, with adaptation, to this file. Eventually this file might
// contain new tests for MimeHandlerViewGuest but ideally they should all be
// tests which are a) based on cross-process frame version of MHVG, and b) tests
// that need chrome layer API. Anything else should go to the extension layer
// version of the tests. Most of the legacy tests will probably be removed when
// MimeHandlerViewGuest starts using cross-process frames (see
// https://crbug.com/659750).

// A class of tests which were originally designed as WebViewGuest tests which
// were testing some aspects of BrowserPlugin. Since all GuestViews except for
// MimeHandlerViewGuest have now moved on to using cross-process frames these
// tests were modified to using MimeHandlerViewGuest instead. They also could
// not be moved to extensions/browser/guest_view/mime_handler_view due to chrome
// layer dependencies.
class ChromeMimeHandlerViewBrowserPluginTest
    : public extensions::ExtensionApiTest {
 public:
  ChromeMimeHandlerViewBrowserPluginTest() {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  ~ChromeMimeHandlerViewBrowserPluginTest() override {}

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir_.AppendASCII("mime_handler_view"));
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 protected:
  TestGuestViewManager* GetGuestViewManager() {
    TestGuestViewManager* manager = static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(browser()->profile()));
    // TestGuestViewManager::WaitForSingleGuestCreated can and will get called
    // before a guest is created. Since GuestViewManager is usually not created
    // until the first guest is created, this means that |manager| will be
    // nullptr if trying to use the manager to wait for the first guest. Because
    // of this, the manager must be created here if it does not already exist.
    if (!manager) {
      manager = static_cast<TestGuestViewManager*>(
          GuestViewManager::CreateWithDelegate(
              browser()->profile(),
              ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                  browser()->profile())));
    }
    return manager;
  }

  void InitializeTestPage(const GURL& url) {
    // Use the testing subclass of MimeHandlerViewGuest.
    GetGuestViewManager()->RegisterTestGuestViewType<MimeHandlerViewGuest>(
        base::BindRepeating(&TestMimeHandlerViewGuest::Create));

    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("mime_handler_view"));
    ASSERT_TRUE(extension);
    const char kTestExtensionId[] = "oickdpebdnfbgkcaoklfcdhjniefkcji";
    CHECK_EQ(kTestExtensionId, extension->id());

    extensions::ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(), url);

    if (!catcher.GetNextResult())
      FAIL() << catcher.message();

    guest_web_contents_ = GetGuestViewManager()->WaitForSingleGuestCreated();
    embedder_web_contents_ = browser()->tab_strip_model()->GetWebContentsAt(0);
    ASSERT_TRUE(guest_web_contents_);
    ASSERT_TRUE(embedder_web_contents_);
  }

  content::WebContents* guest_web_contents() const {
    return guest_web_contents_;
  }
  content::WebContents* embedder_web_contents() const {
    return embedder_web_contents_;
  }

 private:
  TestGuestViewManagerFactory factory_;
  content::WebContents* guest_web_contents_;
  content::WebContents* embedder_web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMimeHandlerViewBrowserPluginTest);
};

// Helper class to monitor focus on a WebContents with BrowserPlugin (guest).
class FocusChangeWaiter {
 public:
  explicit FocusChangeWaiter(content::WebContents* web_contents,
                             bool expected_focus)
      : web_contents_(web_contents), expected_focus_(expected_focus) {}
  ~FocusChangeWaiter() {}

  void WaitForFocusChange() {
    while (expected_focus_ !=
           IsWebContentsBrowserPluginFocused(web_contents_)) {
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  content::WebContents* web_contents_;
  bool expected_focus_;
};

// Flaky under MSan: https://crbug.com/837757
#if defined(MEMORY_SANITIZER)
#define MAYBE_BP_AutoResizeMessages DISABLED_AutoResizeMessages
#else
#define MAYBE_BP_AutoResizeMessages AutoResizeMessages
#endif
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewBrowserPluginTest,
                       MAYBE_BP_AutoResizeMessages) {
  InitializeTestPage(embedded_test_server()->GetURL("/testBasic.csv"));

  // Helper function as this test requires inspecting a number of content::
  // internal objects.
  EXPECT_TRUE(content::TestChildOrGuestAutoresize(
      true,
      embedder_web_contents()
          ->GetRenderWidgetHostView()
          ->GetRenderWidgetHost()
          ->GetProcess(),
      guest_web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost()));
}

#if defined(USE_AURA)
// Flaky on Linux. See: https://crbug.com/870604.
#if defined(OS_LINUX)
#define MAYBE_TouchFocusesEmbedder DISABLED_TouchFocusesEmbedder
#else
#define MAYBE_TouchFocusesEmbedder TouchFocusesEmbedder
#endif
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewBrowserPluginTest,
                       MAYBE_TouchFocusesEmbedder) {
  InitializeTestPage(embedded_test_server()->GetURL("/testBasic.csv"));

  content::RenderViewHost* embedder_rvh =
      embedder_web_contents()->GetRenderViewHost();
  content::RenderFrameSubmissionObserver frame_observer(
      embedder_web_contents());

  bool embedder_has_touch_handler =
      content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh);
  ASSERT_FALSE(embedder_has_touch_handler);

  ASSERT_TRUE(ExecuteScript(
      guest_web_contents(),
      "document.addEventListener('touchstart', dummyTouchStartHandler);"));
  // Wait until embedder has touch handlers.
  while (!content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  auto* top_level_window =
      embedder_web_contents()->GetNativeView()->GetToplevelWindow();
  ASSERT_TRUE(top_level_window);
  auto* widget = views::Widget::GetWidgetForNativeWindow(top_level_window);
  ASSERT_TRUE(widget);
  ASSERT_TRUE(widget->GetRootView());

  // Find WebView corresponding to embedder_web_contents().
  views::View* aura_webview = nullptr;
  base::queue<views::View*> queue;
  queue.push(widget->GetRootView());
  while (!queue.empty()) {
    views::View* current = queue.front();
    queue.pop();
    if (std::string(current->GetClassName()).find("WebView") !=
            std::string::npos &&
        static_cast<views::WebView*>(current)->GetWebContents() ==
            embedder_web_contents()) {
      aura_webview = current;
      break;
    }
    for (int i = 0; i < current->child_count(); ++i)
      queue.push(current->child_at(i));
  }
  ASSERT_TRUE(aura_webview);
  gfx::Rect bounds(aura_webview->bounds());
  EXPECT_TRUE(aura_webview->IsFocusable());

  views::View* other_focusable_view = new views::View();
  other_focusable_view->SetBounds(bounds.x() + bounds.width(), bounds.y(), 100,
                                  100);
  other_focusable_view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  // Focusable views require an accessible name to pass accessibility checks.
  other_focusable_view->GetViewAccessibility().OverrideName("Any name");
  aura_webview->parent()->AddChildView(other_focusable_view);
  other_focusable_view->SetPosition(gfx::Point(bounds.x() + bounds.width(), 0));

  // Sync changes to compositor.
  while (!RequestFrame(embedder_web_contents())) {
    // RequestFrame failed because we were waiting on an ack ... wait a short
    // time and retry.
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(10));
    run_loop.Run();
  }
  frame_observer.WaitForAnyFrameSubmission();

  aura_webview->RequestFocus();
  // Verify that other_focusable_view can steal focus from aura_webview.
  EXPECT_TRUE(aura_webview->HasFocus());
  other_focusable_view->RequestFocus();
  EXPECT_TRUE(other_focusable_view->HasFocus());
  EXPECT_FALSE(aura_webview->HasFocus());

  // Compute location of guest within embedder so we can more accurately
  // target our touch event.
  gfx::Rect guest_rect = guest_web_contents()->GetContainerBounds();
  gfx::Point embedder_origin =
      embedder_web_contents()->GetContainerBounds().origin();
  guest_rect.Offset(-embedder_origin.x(), -embedder_origin.y());

  // Generate and send synthetic touch event.
  content::InputEventAckWaiter waiter(
      guest_web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost(),
      blink::WebInputEvent::kTouchStart);
  content::SimulateTouchPressAt(embedder_web_contents(),
                                guest_rect.CenterPoint());
  waiter.Wait();
  EXPECT_TRUE(aura_webview->HasFocus());
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewBrowserPluginTest,
                       TouchFocusesBrowserPluginInEmbedder) {
  InitializeTestPage(embedded_test_server()->GetURL("/test_embedded.html"));

  auto embedder_rect = embedder_web_contents()->GetContainerBounds();
  auto guest_rect = guest_web_contents()->GetContainerBounds();

  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());
  embedder_rect.set_x(0);
  embedder_rect.set_y(0);

  // Don't send events that need to be routed until we know the child's surface
  // is ready for hit testing.
  content::WaitForHitTestDataOrGuestSurfaceReady(guest_web_contents());

  // 1) BrowserPlugin should not be focused at start.
  ASSERT_FALSE(IsWebContentsBrowserPluginFocused(guest_web_contents()));

  // 2) Send touch event to guest, now BrowserPlugin should get focus.
  {
    gfx::Point point = guest_rect.CenterPoint();
    FocusChangeWaiter focus_waiter(guest_web_contents(), true);
    SendRoutedTouchTapSequence(embedder_web_contents(), point);
    SendRoutedGestureTapSequence(embedder_web_contents(), point);
    focus_waiter.WaitForFocusChange();
    ASSERT_TRUE(IsWebContentsBrowserPluginFocused(guest_web_contents()));
  }

  // 3) Send touch start to embedder, now BrowserPlugin should lose focus.
  {
    // Choose a point outside of guest (but inside the embedder).
    gfx::Point point = guest_rect.bottom_right();
    point += gfx::Vector2d(10, 10);
    EXPECT_TRUE(embedder_rect.Contains(point));
    FocusChangeWaiter focus_waiter(guest_web_contents(), false);
    SendRoutedTouchTapSequence(embedder_web_contents(), point);
    SendRoutedGestureTapSequence(embedder_web_contents(), point);
    focus_waiter.WaitForFocusChange();
    ASSERT_FALSE(IsWebContentsBrowserPluginFocused(guest_web_contents()));
  }
}
#endif  // USE_AURA

class ChromeMimeHandlerViewBrowserPluginScrollTest
    : public ChromeMimeHandlerViewBrowserPluginTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeMimeHandlerViewBrowserPluginTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kTouchEventFeatureDetection,
        switches::kTouchEventFeatureDetectionEnabled);
  }
};

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
#define MAYBE_ScrollGuestContent DISABLED_ScrollGuestContent
#else
#define MAYBE_ScrollGuestContent ScrollGuestContent
#endif
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewBrowserPluginScrollTest,
                       MAYBE_ScrollGuestContent) {
  InitializeTestPage(embedded_test_server()->GetURL("/test_embedded.html"));

  ASSERT_TRUE(ExecuteScript(guest_web_contents(), "ensurePageIsScrollable();"));

  content::RenderFrameSubmissionObserver embedder_frame_observer(
      embedder_web_contents());
  content::RenderFrameSubmissionObserver guest_frame_observer(
      guest_web_contents());

  gfx::Rect embedder_rect = embedder_web_contents()->GetContainerBounds();
  gfx::Rect guest_rect = guest_web_contents()->GetContainerBounds();
  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());

  gfx::Vector2dF default_offset;
  guest_frame_observer.WaitForScrollOffset(default_offset);
  embedder_frame_observer.WaitForScrollOffset(default_offset);

  gfx::Point guest_scroll_location(guest_rect.width() / 2,
                                   guest_rect.height() / 2);
  float gesture_distance = 15.f;
  {
    gfx::Vector2dF expected_offset(0.f, gesture_distance);

    content::SimulateGestureScrollSequence(
        guest_web_contents(), guest_scroll_location,
        gfx::Vector2dF(0, -gesture_distance));

    guest_frame_observer.WaitForScrollOffset(expected_offset);
  }

  embedder_frame_observer.WaitForScrollOffset(default_offset);

  // Use fling gesture to scroll back, velocity should be big enough to scroll
  // content back.
  float fling_velocity = 300.f;
  {
    content::SimulateGestureFlingSequence(guest_web_contents(),
                                          guest_scroll_location,
                                          gfx::Vector2dF(0, fling_velocity));

    guest_frame_observer.WaitForScrollOffset(default_offset);
  }

  embedder_frame_observer.WaitForScrollOffset(default_offset);
}
