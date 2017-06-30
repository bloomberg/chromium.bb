// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_selection_controller_client_aura.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/touch_selection/touch_selection_controller_test_api.h"

namespace content {
namespace {

bool JSONToPoint(const std::string& str, gfx::PointF* point) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(str);
  if (!value)
    return false;
  base::DictionaryValue* root;
  if (!value->GetAsDictionary(&root))
    return false;
  double x, y;
  if (!root->GetDouble("x", &x))
    return false;
  if (!root->GetDouble("y", &y))
    return false;
  point->set_x(x);
  point->set_y(y);
  return true;
}

// A mock touch selection menu runner to use whenever a default one is not
// installed.
class TestTouchSelectionMenuRunner : public ui::TouchSelectionMenuRunner {
 public:
  TestTouchSelectionMenuRunner() : menu_opened_(false) {}
  ~TestTouchSelectionMenuRunner() override {}

 private:
  bool IsMenuAvailable(
      const ui::TouchSelectionMenuClient* client) const override {
    return true;
  }

  void OpenMenu(ui::TouchSelectionMenuClient* client,
                const gfx::Rect& anchor_rect,
                const gfx::Size& handle_image_size,
                aura::Window* context) override {
    menu_opened_ = true;
  }

  void CloseMenu() override { menu_opened_ = false; }

  bool IsRunning() const override { return menu_opened_; }

  bool menu_opened_;

  DISALLOW_COPY_AND_ASSIGN(TestTouchSelectionMenuRunner);
};

}  // namespace

class TestTouchSelectionControllerClientAura
    : public TouchSelectionControllerClientAura {
 public:
  explicit TestTouchSelectionControllerClientAura(
      RenderWidgetHostViewAura* rwhva)
      : TouchSelectionControllerClientAura(rwhva),
        expected_event_(ui::SELECTION_HANDLES_SHOWN) {
    show_quick_menu_immediately_for_test_ = true;
  }

  ~TestTouchSelectionControllerClientAura() override {}

  void InitWaitForSelectionEvent(ui::SelectionEventType expected_event) {
    DCHECK(!run_loop_);
    expected_event_ = expected_event;
    run_loop_.reset(new base::RunLoop());
  }

  void Wait() {
    DCHECK(run_loop_);
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  // TouchSelectionControllerClientAura:
  void OnSelectionEvent(ui::SelectionEventType event) override {
    TouchSelectionControllerClientAura::OnSelectionEvent(event);
    if (run_loop_ && event == expected_event_)
      run_loop_->Quit();
  }

  bool IsCommandIdEnabled(int command_id) const override {
    // Return true so that quick menu has something to show.
    return true;
  }

  ui::SelectionEventType expected_event_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestTouchSelectionControllerClientAura);
};

class TouchSelectionControllerClientAuraTest : public ContentBrowserTest {
 public:
  TouchSelectionControllerClientAuraTest() {}
  ~TouchSelectionControllerClientAuraTest() override {}

 protected:
  // Starts the test server and navigates to the given url. Sets a large enough
  // size to the root window.  Returns after the navigation to the url is
  // complete.
  void StartTestWithPage(const std::string& url) {
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL test_url(embedded_test_server()->GetURL(url));
    NavigateToURL(shell(), test_url);
    aura::Window* content = shell()->web_contents()->GetContentNativeView();
    content->GetHost()->SetBoundsInPixels(gfx::Rect(800, 600));
  }

  bool GetPointInsideText(gfx::PointF* point) {
    std::string str;
    if (ExecuteScriptAndExtractString(shell(), "get_point_inside_text()",
                                      &str)) {
      return JSONToPoint(str, point);
    }
    return false;
  }

  bool GetPointInsideTextfield(gfx::PointF* point) {
    std::string str;
    if (ExecuteScriptAndExtractString(shell(), "get_point_inside_textfield()",
                                      &str)) {
      return JSONToPoint(str, point);
    }
    return false;
  }

  RenderWidgetHostViewAura* GetRenderWidgetHostViewAura() {
    return static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());
  }

  TestTouchSelectionControllerClientAura* selection_controller_client() {
    return selection_controller_client_;
  }

  void InitSelectionController() {
    RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
    selection_controller_client_ =
        new TestTouchSelectionControllerClientAura(rwhva);
    rwhva->SetSelectionControllerClientForTest(
        base::WrapUnique(selection_controller_client_));
    rwhva->event_handler()->disable_input_event_router_for_testing();
  }

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    if (!ui::TouchSelectionMenuRunner::GetInstance())
      menu_runner_.reset(new TestTouchSelectionMenuRunner);
  }

 private:
  void TearDownOnMainThread() override {
    menu_runner_ = nullptr;
    selection_controller_client_ = nullptr;
    ContentBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<TestTouchSelectionMenuRunner> menu_runner_;

  TestTouchSelectionControllerClientAura* selection_controller_client_ =
      nullptr;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionControllerClientAuraTest);
};

// Tests that long-pressing on a text brings up selection handles and the quick
// menu properly.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest, BasicSelection) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Long-press on the text and wait for handles to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  gfx::PointF point;
  ASSERT_TRUE(GetPointInsideText(&point));
  ui::GestureEventDetails long_press_details(ui::ET_GESTURE_LONG_PRESS);
  long_press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_press(point.x(), point.y(), 0, ui::EventTimeForNow(),
                              long_press_details);
  rwhva->OnGestureEvent(&long_press);

  selection_controller_client()->Wait();

  // Check that selection is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

class TouchSelectionControllerClientAuraSiteIsolationTest
    : public TouchSelectionControllerClientAuraTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    TouchSelectionControllerClientAuraTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SelectWithLongPress(gfx::Point point) {
    // Get main frame view for event insertion.
    RenderWidgetHostViewAura* main_view = GetRenderWidgetHostViewAura();

    SendTouch(main_view, ui::ET_TOUCH_PRESSED, point);
    SendTouch(main_view, ui::ET_TOUCH_RELEASED, point);
    SendGestureTap(main_view, point);
    SendGestureLongPress(main_view, point);
  }

  void SimpleTap(gfx::Point point) {
    // Get main frame view for event insertion.
    RenderWidgetHostViewAura* main_view = GetRenderWidgetHostViewAura();

    SendTouch(main_view, ui::ET_TOUCH_PRESSED, point);
    SendTouch(main_view, ui::ET_TOUCH_RELEASED, point);
    SendGestureTap(main_view, point);
  }

 private:
  void SendTouch(RenderWidgetHostViewAura* view,
                 ui::EventType type,
                 gfx::Point point) {
    DCHECK(type >= ui::ET_TOUCH_RELEASED && type << ui::ET_TOUCH_CANCELLED);
    ui::TouchEvent touch(
        type, point, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
    view->OnTouchEvent(&touch);
  }

  void SendGestureTap(RenderWidgetHostViewAura* view, gfx::Point point) {
    ui::GestureEventDetails tap_down_details(ui::ET_GESTURE_TAP_DOWN);
    tap_down_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_tap_down(point.x(), point.y(), 0,
                                      ui::EventTimeForNow(), tap_down_details);
    view->OnGestureEvent(&gesture_tap_down);
    ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP);
    tap_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    tap_details.set_tap_count(1);
    ui::GestureEvent gesture_tap(point.x(), point.y(), 0, ui::EventTimeForNow(),
                                 tap_details);
    view->OnGestureEvent(&gesture_tap);
  }

  void SendGestureLongPress(RenderWidgetHostViewAura* view, gfx::Point point) {
    ui::GestureEventDetails long_press_details(ui::ET_GESTURE_LONG_PRESS);
    long_press_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_long_press(
        point.x(), point.y(), 0, ui::EventTimeForNow(), long_press_details);
    view->OnGestureEvent(&gesture_long_press);
  }
};

class FrameStableObserver {
 public:
  FrameStableObserver(RenderWidgetHostViewBase* view, base::TimeDelta delta)
      : view_(view), delta_(delta) {}
  virtual ~FrameStableObserver() {}

  void WaitUntilStable() {
    uint32_t current_frame_number = view_->RendererFrameNumber();
    uint32_t previous_frame_number;

    do {
      base::RunLoop run_loop;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), delta_);
      run_loop.Run();
      previous_frame_number = current_frame_number;
      current_frame_number = view_->RendererFrameNumber();
    } while (current_frame_number != previous_frame_number);
  }

 private:
  RenderWidgetHostViewBase* view_;
  base::TimeDelta delta_;

  DISALLOW_COPY_AND_ASSIGN(FrameStableObserver);
};

INSTANTIATE_TEST_CASE_P(TouchSelectionForCrossProcessFramesTests,
                        TouchSelectionControllerClientAuraSiteIsolationTest,
                        testing::Bool());

IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAuraSiteIsolationTest,
                       BasicSelectionIsolatedIframe) {
  GURL test_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(
      " Site A\n"
      "   +--Site A\n"
      "Where A = http://a.com/",
      FrameTreeVisualizer().DepictFrameTree(root));
  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  RenderWidgetHostViewAura* parent_view =
      static_cast<RenderWidgetHostViewAura*>(
          root->current_frame_host()->GetRenderWidgetHost()->GetView());
  TestTouchSelectionControllerClientAura* parent_selection_controller_client =
      new TestTouchSelectionControllerClientAura(parent_view);
  parent_view->SetSelectionControllerClientForTest(
      base::WrapUnique(parent_selection_controller_client));

  // We need to load the desired subframe and then wait until it's stable, i.e.
  // generates no new frames for some reasonable time period: a stray frame
  // between touch selection's pre-handling of GestureLongPress and the
  // expected frame containing the selected region can confuse the
  // TouchSelectionController, causing it to fail to show selection handles.
  // Note this is an issue with the TouchSelectionController in general, and
  // not a property of this test.
  GURL child_url(
      embedded_test_server()->GetURL("b.com", "/touch_selection.html"));
  NavigateFrameToURL(child, child_url);
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      FrameTreeVisualizer().DepictFrameTree(root));

  // The child will change with the cross-site navigation. It shouldn't change
  // after this.
  child = root->child_at(0);
  WaitForChildFrameSurfaceReady(child->current_frame_host());

  RenderWidgetHostViewChildFrame* child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          child->current_frame_host()->GetRenderWidgetHost()->GetView());

  EXPECT_EQ(child_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  FrameStableObserver child_frame_stable_observer(child_view,
                                                  TestTimeouts::tiny_timeout());
  child_frame_stable_observer.WaitUntilStable();

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            parent_view->selection_controller()->active_status());

  // Find the location of some text to select.
  gfx::PointF point_f;
  std::string str;
  EXPECT_TRUE(ExecuteScriptAndExtractString(child->current_frame_host(),
                                            "get_point_inside_text()", &str));
  JSONToPoint(str, &point_f);
  gfx::Point origin = child_view->GetViewOriginInRoot();
  gfx::Vector2dF origin_vec(origin.x(), origin.y());
  point_f += origin_vec;

  // Initiate selection with a sequence of events that go through the targeting
  // system.
  parent_selection_controller_client->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  SelectWithLongPress(gfx::Point(point_f.x(), point_f.y()));

  parent_selection_controller_client->Wait();

  // Check that selection is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            parent_view->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap inside/outside the iframe and make sure the selection handles go away.
  parent_selection_controller_client->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_CLEARED);
  if (GetParam()) {
    gfx::PointF point_outside_iframe = gfx::PointF(-1.f, -1.f) + origin_vec;
    SimpleTap(gfx::Point(point_outside_iframe.x(), point_outside_iframe.y()));
  } else {
    gfx::PointF point_inside_iframe = gfx::PointF(+1.f, +1.f) + origin_vec;
    SimpleTap(gfx::Point(point_inside_iframe.x(), point_inside_iframe.y()));
  }
  parent_selection_controller_client->Wait();

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            parent_view->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests that tapping in a textfield brings up the insertion handle, but not the
// quick menu, initially. Then, successive taps on the insertion handle toggle
// the quick menu visibility.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       BasicInsertionFollowedByTapsOnHandle) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  ui::test::EventGeneratorDelegate* generator_delegate =
      ui::test::EventGenerator::default_delegate;
  gfx::NativeView native_view = rwhva->GetNativeView();
  ui::test::EventGenerator generator(native_view->GetRootWindow());

  // Tap inside the textfield and wait for the insertion handle to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_SHOWN);

  gfx::PointF point_f;
  ASSERT_TRUE(GetPointInsideTextfield(&point_f));
  gfx::Point point = gfx::ToRoundedPoint(point_f);
  generator_delegate->ConvertPointFromTarget(native_view, &point);
  generator.GestureTapAt(point);

  selection_controller_client()->Wait();

  // Check that insertion is active, but the quick menu is not showing.
  EXPECT_EQ(ui::TouchSelectionController::INSERTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap on the insertion handle; the quick menu should appear.
  gfx::Point handle_center = gfx::ToRoundedPoint(
      rwhva->selection_controller()->GetStartHandleRect().CenterPoint());
  generator_delegate->ConvertPointFromTarget(native_view, &handle_center);
  generator.GestureTapAt(handle_center);
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap once more on the insertion handle; the quick menu should disappear.
  generator.GestureTapAt(handle_center);
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests that the quick menu is hidden whenever a touch point is active.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       QuickMenuHiddenOnTouch) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Long-press on the text and wait for selection handles to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  gfx::PointF point;
  ASSERT_TRUE(GetPointInsideText(&point));
  ui::GestureEventDetails long_press_details(ui::ET_GESTURE_LONG_PRESS);
  long_press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_press(point.x(), point.y(), 0, ui::EventTimeForNow(),
                              long_press_details);
  rwhva->OnGestureEvent(&long_press);

  selection_controller_client()->Wait();

  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  ui::test::EventGenerator generator(rwhva->GetNativeView()->GetRootWindow(),
                                     rwhva->GetNativeView());

  // Put the first finger down: the quick menu should get hidden.
  generator.PressTouchId(0);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Put a second finger down: the quick menu should remain hidden.
  generator.PressTouchId(1);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Lift the first finger up: the quick menu should still remain hidden.
  generator.ReleaseTouchId(0);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Lift the second finger up: the quick menu should re-appear.
  generator.ReleaseTouchId(1);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests that the quick menu and touch handles are hidden during an scroll.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest, HiddenOnScroll) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  ui::TouchSelectionControllerTestApi selection_controller_test_api(
      rwhva->selection_controller());

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Long-press on the text and wait for selection handles to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  gfx::PointF point;
  ASSERT_TRUE(GetPointInsideText(&point));
  ui::GestureEventDetails long_press_details(ui::ET_GESTURE_LONG_PRESS);
  long_press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_press(point.x(), point.y(), 0, ui::EventTimeForNow(),
                              long_press_details);
  rwhva->OnGestureEvent(&long_press);

  selection_controller_client()->Wait();

  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Put a finger down: the quick menu should go away, while touch handles stay
  // there.
  ui::TouchEvent touch_down(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_down);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Start scrolling: touch handles should get hidden, while touch selection is
  // still active.
  ui::GestureEventDetails scroll_begin_details(ui::ET_GESTURE_SCROLL_BEGIN);
  scroll_begin_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_begin(10, 10, 0, ui::EventTimeForNow(),
                                scroll_begin_details);
  rwhva->OnGestureEvent(&scroll_begin);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // End scrolling: touch handles should re-appear.
  ui::GestureEventDetails scroll_end_details(ui::ET_GESTURE_SCROLL_END);
  scroll_end_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_end(10, 10, 0, ui::EventTimeForNow(),
                              scroll_end_details);
  rwhva->OnGestureEvent(&scroll_end);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Lift the finger up: the quick menu should re-appear.
  ui::TouchEvent touch_up(
      ui::ET_TOUCH_RELEASED, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_up);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests that touch selection gets deactivated after an overscroll completes.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       HiddenAfterOverscroll) {
  // Set the page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();
  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Long-press on the text and wait for touch handles to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  gfx::PointF point;
  ASSERT_TRUE(GetPointInsideText(&point));
  ui::GestureEventDetails long_press_details(ui::ET_GESTURE_LONG_PRESS);
  long_press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_press(point.x(), point.y(), 0, ui::EventTimeForNow(),
                              long_press_details);
  rwhva->OnGestureEvent(&long_press);

  selection_controller_client()->Wait();

  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Scroll such that an overscroll is initiated and wait for it to complete:
  // touch selection should not be active at the end.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_CLEARED);

  float event_x = 10.f;
  const float event_y = 10.f;
  ui::GestureEventDetails scroll_begin_details(ui::ET_GESTURE_SCROLL_BEGIN);
  scroll_begin_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_begin(event_x, event_y, 0, ui::EventTimeForNow(),
                                scroll_begin_details);
  rwhva->OnGestureEvent(&scroll_begin);

  const int window_width = rwhva->GetNativeView()->bounds().width();
  const float overscroll_threshold =
      GetOverscrollConfig(OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHSCREEN);
  const float scroll_amount = window_width * overscroll_threshold + 1;
  event_x += scroll_amount;
  ui::GestureEventDetails scroll_update_details(ui::ET_GESTURE_SCROLL_UPDATE,
                                                scroll_amount, 0);
  scroll_update_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_update(event_x, event_y, 0, ui::EventTimeForNow(),
                                 scroll_update_details);
  rwhva->OnGestureEvent(&scroll_update);

  ui::GestureEventDetails scroll_end_details(ui::ET_GESTURE_SCROLL_END);
  scroll_end_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_end(event_x, event_y, 0, ui::EventTimeForNow(),
                              scroll_end_details);
  rwhva->OnGestureEvent(&scroll_end);

  selection_controller_client()->Wait();

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

class TouchSelectionControllerClientAuraScaleFactorTest
    : public TouchSelectionControllerClientAuraTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");
  }
};

#if defined(OS_WIN)
// High DPI tests are disabled on Windows due to crbug.com/545547.
#define MAYBE_SelectionHandleCoordinates DISABLED_SelectionHandleCoordinates
#define MAYBE_InsertionHandleCoordinates DISABLED_InsertionHandleCoordinates
#else
#define MAYBE_SelectionHandleCoordinates SelectionHandleCoordinates
#define MAYBE_InsertionHandleCoordinates InsertionHandleCoordinates
#endif

// Tests that selection handles are properly positioned at 2x DSF.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraScaleFactorTest,
                       MAYBE_SelectionHandleCoordinates) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  EXPECT_EQ(2.f, rwhva->current_device_scale_factor());

  // Long-press on the text and wait for handles to appear.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);
  gfx::PointF point;
  ASSERT_TRUE(GetPointInsideText(&point));
  ui::GestureEventDetails long_press_details(ui::ET_GESTURE_LONG_PRESS);
  long_press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_press(point.x(), point.y(), 0, ui::EventTimeForNow(),
                              long_press_details);
  rwhva->OnGestureEvent(&long_press);
  selection_controller_client()->Wait();

  // Check that selection is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  const ui::TouchSelectionController* controller =
      GetRenderWidgetHostViewAura()->selection_controller();

  gfx::PointF start_top = controller->start().edge_top();

  // The selection start should be uppper left, and selection end should be
  // upper right.
  EXPECT_LT(controller->start().edge_top().x(), point.x());
  EXPECT_LT(controller->start().edge_bottom().x(), point.x());

  EXPECT_LT(point.x(), controller->end().edge_top().x());
  EXPECT_LT(point.x(), controller->end().edge_bottom().x());

  // Handles are created below the selection. The top position should roughly
  // be within the handle size from the touch position.
  float handle_size = controller->start().edge_bottom().y() -
                      controller->start().edge_top().y();
  float handle_max_bottom = point.y() + handle_size;
  EXPECT_GT(handle_max_bottom, controller->start().edge_top().y());
  EXPECT_GT(handle_max_bottom, controller->end().edge_top().y());

  gfx::Point handle_point = gfx::ToRoundedPoint(
      rwhva->selection_controller()->GetStartHandleRect().CenterPoint());

  // Move the selection handle. Touch the handle first.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLE_DRAG_STARTED);
  ui::TouchEvent touch_down(
      ui::ET_TOUCH_PRESSED, handle_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_down);
  selection_controller_client()->Wait();

  // Move it.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_MOVED);
  handle_point.Offset(10, 0);
  ui::TouchEvent touch_move(
      ui::ET_TOUCH_MOVED, handle_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_move);
  selection_controller_client()->Wait();

  // Then release.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLE_DRAG_STOPPED);
  ui::TouchEvent touch_up(
      ui::ET_TOUCH_RELEASED, handle_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_up);
  selection_controller_client()->Wait();

  // The handle should have moved to right.
  EXPECT_EQ(start_top.y(), controller->start().edge_top().y());
  EXPECT_LT(start_top.x(), controller->start().edge_top().x());

  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
}

// Tests that insertion handles are properly positioned at 2x DSF.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraScaleFactorTest,
                       MAYBE_InsertionHandleCoordinates) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("/touch_selection.html"));
  InitSelectionController();

  RenderWidgetHostViewAura* rwhva = GetRenderWidgetHostViewAura();

  // Tap inside the textfield and wait for the insertion cursor.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_SHOWN);

  gfx::PointF point;
  ASSERT_TRUE(GetPointInsideTextfield(&point));
  ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP);
  tap_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  tap_details.set_tap_count(1);
  ui::GestureEvent tap(point.x(), point.y(), 0, ui::EventTimeForNow(),
                       tap_details);
  rwhva->OnGestureEvent(&tap);

  selection_controller_client()->Wait();

  EXPECT_EQ(ui::TouchSelectionController::INSERTION_ACTIVE,
            rwhva->selection_controller()->active_status());

  gfx::RectF initial_handle_rect =
      rwhva->selection_controller()->GetStartHandleRect();

  // Move the insertion handle. Touch the handle first.
  gfx::Point handle_point =
      gfx::ToRoundedPoint(initial_handle_rect.CenterPoint());

  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_DRAG_STARTED);
  ui::TouchEvent touch_down(
      ui::ET_TOUCH_PRESSED, handle_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_down);
  selection_controller_client()->Wait();

  // Move it.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_MOVED);
  handle_point.Offset(10, 0);
  ui::TouchEvent touch_move(
      ui::ET_TOUCH_MOVED, handle_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_move);
  selection_controller_client()->Wait();

  // Then release.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_DRAG_STOPPED);
  ui::TouchEvent touch_up(
      ui::ET_TOUCH_RELEASED, handle_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_up);
  selection_controller_client()->Wait();

  gfx::RectF moved_handle_rect =
      rwhva->selection_controller()->GetStartHandleRect();

  // The handle should have moved to right.
  EXPECT_EQ(initial_handle_rect.y(), moved_handle_rect.y());
  EXPECT_LT(initial_handle_rect.x(), moved_handle_rect.x());

  EXPECT_EQ(ui::TouchSelectionController::INSERTION_ACTIVE,
            rwhva->selection_controller()->active_status());
}

}  // namespace content
