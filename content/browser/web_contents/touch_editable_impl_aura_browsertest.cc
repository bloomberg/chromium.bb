// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/touch_editable_impl_aura.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_aura.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/base/events/event_utils.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace content {

class TestTouchEditableImplAura : public TouchEditableImplAura {
 public:
  TestTouchEditableImplAura()
      : selection_changed_callback_arrived_(false),
        waiting_for_selection_changed_callback_(false),
        gesture_ack_callback_arrived_(false),
        waiting_for_gesture_ack_callback_(false) {}

  void Reset() {
    selection_changed_callback_arrived_ = false;
    waiting_for_selection_changed_callback_ = false;
    gesture_ack_callback_arrived_ = false;
    waiting_for_gesture_ack_callback_ = false;
  }

  void OnSelectionOrCursorChanged(const gfx::Rect& anchor,
                                  const gfx::Rect& focus) OVERRIDE {
    selection_changed_callback_arrived_ = true;
    TouchEditableImplAura::OnSelectionOrCursorChanged(anchor, focus);
    if (waiting_for_selection_changed_callback_)
      selection_changed_wait_run_loop_->Quit();
  }

  void GestureEventAck(int gesture_event_type) OVERRIDE {
    gesture_ack_callback_arrived_ = true;
    TouchEditableImplAura::GestureEventAck(gesture_event_type);
    if (waiting_for_gesture_ack_callback_)
      gesture_ack_wait_run_loop_->Quit();
  }

  void WaitForSelectionChangeCallback() {
    if (selection_changed_callback_arrived_)
      return;
    waiting_for_selection_changed_callback_ = true;
    selection_changed_wait_run_loop_.reset(new base::RunLoop());
    selection_changed_wait_run_loop_->Run();
  }

  void WaitForGestureAck() {
    if (gesture_ack_callback_arrived_)
      return;
    waiting_for_gesture_ack_callback_ = true;
    gesture_ack_wait_run_loop_.reset(new base::RunLoop());
    gesture_ack_wait_run_loop_->Run();
  }

 protected:
  virtual ~TestTouchEditableImplAura() {}

 private:
  bool selection_changed_callback_arrived_;
  bool waiting_for_selection_changed_callback_;
  bool gesture_ack_callback_arrived_;
  bool waiting_for_gesture_ack_callback_;
  scoped_ptr<base::RunLoop> selection_changed_wait_run_loop_;
  scoped_ptr<base::RunLoop> gesture_ack_wait_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestTouchEditableImplAura);
};

class TouchEditableImplAuraTest : public ContentBrowserTest {
 public:
  TouchEditableImplAuraTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableTouchEditing);
  }

  // Executes the javascript synchronously and makes sure the returned value is
  // freed properly.
  void ExecuteSyncJSFunction(RenderViewHost* rvh, const std::string& jscript) {
    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(rvh, jscript);
  }

  // Starts the test server and navigates to the given url. Sets a large enough
  // size to the root window.  Returns after the navigation to the url is
  // complete.
  void StartTestWithPage(const std::string& url) {
    ASSERT_TRUE(test_server()->Start());
    GURL test_url(test_server()->GetURL(url));
    NavigateToURL(shell(), test_url);
    aura::Window* content =
        shell()->web_contents()->GetView()->GetContentNativeView();
    content->GetRootWindow()->SetHostSize(gfx::Size(800, 600));
  }

  void TestTouchSelectionOriginatingFromWebpage() {
    ASSERT_NO_FATAL_FAILURE(
        StartTestWithPage("files/touch_selection.html"));
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    RenderViewHostImpl* view_host = static_cast<RenderViewHostImpl*>(
        web_contents->GetRenderViewHost());
    WebContentsViewAura* view_aura = static_cast<WebContentsViewAura*>(
        web_contents->GetView());
    TestTouchEditableImplAura* touch_editable = new TestTouchEditableImplAura;
    view_aura->SetTouchEditableForTest(touch_editable);
    RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
        web_contents->GetRenderWidgetHostView());
    aura::Window* content = web_contents->GetView()->GetContentNativeView();
    aura::test::EventGenerator generator(content->GetRootWindow(), content);
    gfx::Rect bounds = content->GetBoundsInRootWindow();

    touch_editable->Reset();
    ExecuteSyncJSFunction(view_host, "select_all_text()");
    touch_editable->WaitForSelectionChangeCallback();

    // Tap inside selection to bring up selection handles.
    generator.GestureTapAt(gfx::Point(bounds.x() + 10, bounds.y() + 10));
    EXPECT_EQ(touch_editable->rwhva_, rwhva);

    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(view_host, "get_selection()");
    std::string selection;
    value->GetAsString(&selection);

    // Check if selection handles are showing.
    EXPECT_TRUE(touch_editable->touch_selection_controller_.get());
    EXPECT_STREQ("Some text we can select", selection.c_str());

    // Lets move the handles a bit to modify the selection
    touch_editable->Reset();
    generator.GestureScrollSequence(
        gfx::Point(10, 37),
        gfx::Point(30, 37),
        base::TimeDelta::FromMilliseconds(20),
        1);
    EXPECT_TRUE(touch_editable->touch_selection_controller_.get());
    value = content::ExecuteScriptAndGetValue(view_host, "get_selection()");
    value->GetAsString(&selection);

    // It is hard to tell what exactly the selection would be now. But it would
    // definitely be less than whatever was selected before.
    EXPECT_GT(std::strlen("Some text we can select"), selection.size());
  }

  void TestTouchSelectionOnLongPress() {
    ASSERT_NO_FATAL_FAILURE(
        StartTestWithPage("files/touch_selection.html"));
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    RenderViewHostImpl* view_host = static_cast<RenderViewHostImpl*>(
        web_contents->GetRenderViewHost());
    WebContentsViewAura* view_aura = static_cast<WebContentsViewAura*>(
        web_contents->GetView());
    TestTouchEditableImplAura* touch_editable = new TestTouchEditableImplAura;
    view_aura->SetTouchEditableForTest(touch_editable);
    RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
        web_contents->GetRenderWidgetHostView());
    aura::Window* content = web_contents->GetView()->GetContentNativeView();
    aura::test::EventGenerator generator(content->GetRootWindow(), content);
    gfx::Rect bounds = content->GetBoundsInRootWindow();
    EXPECT_EQ(touch_editable->rwhva_, rwhva);

    // Long press to select word.
    ui::GestureEvent long_press(ui::ET_GESTURE_LONG_PRESS,
                                10,
                                10,
                                0,
                                ui::EventTimeForNow(),
                                ui::GestureEventDetails(
                                    ui::ET_GESTURE_LONG_PRESS, 0, 0),
                                1);
    touch_editable->Reset();
    rwhva->OnGestureEvent(&long_press);
    touch_editable->WaitForSelectionChangeCallback();

    // Check if selection handles are showing.
    ui::TouchSelectionController* controller =
        touch_editable->touch_selection_controller_.get();
    EXPECT_TRUE(controller);

    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(view_host, "get_selection()");
    std::string selection;
    value->GetAsString(&selection);
    EXPECT_STREQ("Some", selection.c_str());
  }

  void TestTouchCursorInTextfield() {
    ASSERT_NO_FATAL_FAILURE(
        StartTestWithPage("files/touch_selection.html"));
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    RenderViewHostImpl* view_host = static_cast<RenderViewHostImpl*>(
        web_contents->GetRenderViewHost());
    WebContentsViewAura* view_aura = static_cast<WebContentsViewAura*>(
        web_contents->GetView());
    TestTouchEditableImplAura* touch_editable = new TestTouchEditableImplAura;
    view_aura->SetTouchEditableForTest(touch_editable);
    RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
        web_contents->GetRenderWidgetHostView());
    aura::Window* content = web_contents->GetView()->GetContentNativeView();
    aura::test::EventGenerator generator(content->GetRootWindow(), content);
    gfx::Rect bounds = content->GetBoundsInRootWindow();
    EXPECT_EQ(touch_editable->rwhva_, rwhva);
    ExecuteSyncJSFunction(view_host, "focus_textfield()");

    // Tap textfield
    touch_editable->Reset();
    generator.GestureTapAt(gfx::Point(bounds.x() + 50, bounds.y() + 40));
    touch_editable->WaitForGestureAck();  // Wait for Tap Down Ack
    touch_editable->Reset();
    touch_editable->WaitForGestureAck();  // Wait for Tap Ack.

    // Check if cursor handle is showing.
    ui::TouchSelectionController* controller =
        touch_editable->touch_selection_controller_.get();
    EXPECT_NE(ui::TEXT_INPUT_TYPE_NONE, touch_editable->text_input_type_);
    EXPECT_TRUE(controller);

    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(view_host, "get_cursor_position()");
    int cursor_pos = -1;
    value->GetAsInteger(&cursor_pos);
    EXPECT_NE(-1, cursor_pos);

    // Move the cursor handle.
    generator.GestureScrollSequence(
        gfx::Point(50, 59),
        gfx::Point(10, 59),
        base::TimeDelta::FromMilliseconds(20),
        1);
    EXPECT_TRUE(touch_editable->touch_selection_controller_.get());
    value = content::ExecuteScriptAndGetValue(
        view_host, "get_cursor_position()");
    int new_cursor_pos = -1;
    value->GetAsInteger(&new_cursor_pos);
    EXPECT_NE(-1, new_cursor_pos);
    // Cursor should have moved.
    EXPECT_NE(new_cursor_pos, cursor_pos);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchEditableImplAuraTest);
};

IN_PROC_BROWSER_TEST_F(TouchEditableImplAuraTest,
                       TouchSelectionOriginatingFromWebpageTest) {
  TestTouchSelectionOriginatingFromWebpage();
}

IN_PROC_BROWSER_TEST_F(TouchEditableImplAuraTest,
                       TouchSelectionOnLongPressTest) {
  TestTouchSelectionOnLongPress();
}

// TODO(miu): Disabled test due to flakiness.  http://crbug.com/235991
IN_PROC_BROWSER_TEST_F(TouchEditableImplAuraTest,
                       DISABLED_TouchCursorInTextfieldTest) {
  TestTouchCursorInTextfield();
}

}  // namespace content
