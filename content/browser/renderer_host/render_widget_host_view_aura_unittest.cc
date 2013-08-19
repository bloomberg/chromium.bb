// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/basictypes.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_utils.h"
#include "ui/base/ui_base_types.h"

namespace content {
namespace {
class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate() {}
  virtual ~MockRenderWidgetHostDelegate() {}
};

// Simple observer that keeps track of changes to a window for tests.
class TestWindowObserver : public aura::WindowObserver {
 public:
  explicit TestWindowObserver(aura::Window* window_to_observe)
      : window_(window_to_observe) {
    window_->AddObserver(this);
  }
  virtual ~TestWindowObserver() {
    if (window_)
      window_->RemoveObserver(this);
  }

  bool destroyed() const { return destroyed_; }

  // aura::WindowObserver overrides:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {
    CHECK_EQ(window, window_);
    destroyed_ = true;
    window_ = NULL;
  }

 private:
  // Window that we're observing, or NULL if it's been destroyed.
  aura::Window* window_;

  // Was |window_| destroyed?
  bool destroyed_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowObserver);
};

class RenderWidgetHostViewAuraTest : public testing::Test {
 public:
  RenderWidgetHostViewAuraTest() {}

  virtual void SetUp() {
    aura_test_helper_.reset(new aura::test::AuraTestHelper(&message_loop_));
    aura_test_helper_->SetUp();

    browser_context_.reset(new TestBrowserContext);
    MockRenderProcessHost* process_host =
        new MockRenderProcessHost(browser_context_.get());

    sink_ = &process_host->sink();

    parent_host_ = new RenderWidgetHostImpl(
        &delegate_, process_host, MSG_ROUTING_NONE);
    parent_view_ = static_cast<RenderWidgetHostViewAura*>(
        RenderWidgetHostView::CreateViewForWidget(parent_host_));
    parent_view_->InitAsChild(NULL);
    parent_view_->GetNativeView()->SetDefaultParentByRootWindow(
        aura_test_helper_->root_window(), gfx::Rect());

    widget_host_ = new RenderWidgetHostImpl(
        &delegate_, process_host, MSG_ROUTING_NONE);
    widget_host_->Init();
    view_ = static_cast<RenderWidgetHostViewAura*>(
        RenderWidgetHostView::CreateViewForWidget(widget_host_));
  }

  virtual void TearDown() {
    sink_ = NULL;
    if (view_)
      view_->Destroy();
    delete widget_host_;

    parent_view_->Destroy();
    delete parent_host_;

    browser_context_.reset();
    aura_test_helper_->TearDown();

    message_loop_.DeleteSoon(FROM_HERE, browser_context_.release());
    message_loop_.RunUntilIdle();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  scoped_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  scoped_ptr<BrowserContext> browser_context_;
  MockRenderWidgetHostDelegate delegate_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  RenderWidgetHostImpl* parent_host_;
  RenderWidgetHostViewAura* parent_view_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  RenderWidgetHostImpl* widget_host_;
  RenderWidgetHostViewAura* view_;

  IPC::TestSink* sink_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAuraTest);
};

// A layout manager that always resizes a child to the root window size.
class FullscreenLayoutManager : public aura::LayoutManager {
 public:
  explicit FullscreenLayoutManager(aura::RootWindow* owner)
      : owner_(owner) {}
  virtual ~FullscreenLayoutManager() {}

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE {
    aura::Window::Windows::const_iterator i;
    for (i = owner_->children().begin(); i != owner_->children().end(); ++i) {
      (*i)->SetBounds(gfx::Rect());
    }
  }
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    child->SetBounds(gfx::Rect());
  }
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {
  }
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {
  }
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {
  }
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    SetChildBoundsDirect(child, gfx::Rect(owner_->bounds().size()));
  }

 private:
  aura::RootWindow* owner_;
  DISALLOW_COPY_AND_ASSIGN(FullscreenLayoutManager);
};

}  // namespace

// Checks that a fullscreen view has the correct show-state and receives the
// focus.
TEST_F(RenderWidgetHostViewAuraTest, FocusFullscreen) {
  view_->InitAsFullscreen(parent_view_);
  aura::Window* window = view_->GetNativeView();
  ASSERT_TRUE(window != NULL);
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN,
            window->GetProperty(aura::client::kShowStateKey));

  // Check that we requested and received the focus.
  EXPECT_TRUE(window->HasFocus());

  // Check that we'll also say it's okay to activate the window when there's an
  // ActivationClient defined.
  EXPECT_TRUE(view_->ShouldActivate());
}

// Checks that a fullscreen view is destroyed when it loses the focus.
TEST_F(RenderWidgetHostViewAuraTest, DestroyFullscreenOnBlur) {
  view_->InitAsFullscreen(parent_view_);
  aura::Window* window = view_->GetNativeView();
  ASSERT_TRUE(window != NULL);
  ASSERT_TRUE(window->HasFocus());

  // After we create and focus another window, the RWHVA's window should be
  // destroyed.
  TestWindowObserver observer(window);
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> sibling(new aura::Window(&delegate));
  sibling->Init(ui::LAYER_TEXTURED);
  sibling->Show();
  window->parent()->AddChild(sibling.get());
  sibling->Focus();
  ASSERT_TRUE(sibling->HasFocus());
  ASSERT_TRUE(observer.destroyed());

  widget_host_ = NULL;
  view_ = NULL;
}

// Checks that IME-composition-event state is maintained correctly.
TEST_F(RenderWidgetHostViewAuraTest, SetCompositionText) {
  view_->InitAsChild(NULL);
  view_->Show();

  ui::CompositionText composition_text;
  composition_text.text = ASCIIToUTF16("|a|b");

  // Focused segment
  composition_text.underlines.push_back(
      ui::CompositionUnderline(0, 3, 0xff000000, true));

  // Non-focused segment
  composition_text.underlines.push_back(
      ui::CompositionUnderline(3, 4, 0xff000000, false));

  const ui::CompositionUnderlines& underlines = composition_text.underlines;

  // Caret is at the end. (This emulates Japanese MSIME 2007 and later)
  composition_text.selection = ui::Range(4);

  sink_->ClearMessages();
  view_->SetCompositionText(composition_text);
  EXPECT_TRUE(view_->has_composition_text_);
  {
    const IPC::Message* msg =
      sink_->GetFirstMessageMatching(ViewMsg_ImeSetComposition::ID);
    ASSERT_TRUE(msg != NULL);

    ViewMsg_ImeSetComposition::Param params;
    ViewMsg_ImeSetComposition::Read(msg, &params);
    // composition text
    EXPECT_EQ(composition_text.text, params.a);
    // underlines
    ASSERT_EQ(underlines.size(), params.b.size());
    for (size_t i = 0; i < underlines.size(); ++i) {
      EXPECT_EQ(underlines[i].start_offset, params.b[i].startOffset);
      EXPECT_EQ(underlines[i].end_offset, params.b[i].endOffset);
      EXPECT_EQ(underlines[i].color, params.b[i].color);
      EXPECT_EQ(underlines[i].thick, params.b[i].thick);
    }
    // highlighted range
    EXPECT_EQ(4, params.c) << "Should be the same to the caret pos";
    EXPECT_EQ(4, params.d) << "Should be the same to the caret pos";
  }

  view_->ImeCancelComposition();
  EXPECT_FALSE(view_->has_composition_text_);
}

// Checks that touch-event state is maintained correctly.
TEST_F(RenderWidgetHostViewAuraTest, TouchEventState) {
  view_->InitAsChild(NULL);
  view_->Show();

  // Start with no touch-event handler in the renderer.
  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));
  EXPECT_FALSE(widget_host_->ShouldForwardTouchEvent());

  ui::TouchEvent press(ui::ET_TOUCH_PRESSED,
                       gfx::Point(30, 30),
                       0,
                       ui::EventTimeForNow());
  ui::TouchEvent move(ui::ET_TOUCH_MOVED,
                      gfx::Point(20, 20),
                      0,
                      ui::EventTimeForNow());
  ui::TouchEvent release(ui::ET_TOUCH_RELEASED,
                         gfx::Point(20, 20),
                         0,
                         ui::EventTimeForNow());

  view_->OnTouchEvent(&press);
  EXPECT_FALSE(press.handled());
  EXPECT_EQ(WebKit::WebInputEvent::TouchStart, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(WebKit::WebTouchPoint::StatePressed,
            view_->touch_event_.touches[0].state);

  view_->OnTouchEvent(&move);
  EXPECT_FALSE(move.handled());
  EXPECT_EQ(WebKit::WebInputEvent::TouchMove, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(WebKit::WebTouchPoint::StateMoved,
            view_->touch_event_.touches[0].state);

  view_->OnTouchEvent(&release);
  EXPECT_FALSE(release.handled());
  EXPECT_EQ(WebKit::WebInputEvent::TouchEnd, view_->touch_event_.type);
  EXPECT_EQ(0U, view_->touch_event_.touchesLength);

  // Now install some touch-event handlers and do the same steps. The touch
  // events should now be consumed. However, the touch-event state should be
  // updated as before.
  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_TRUE(widget_host_->ShouldForwardTouchEvent());

  view_->OnTouchEvent(&press);
  EXPECT_TRUE(press.stopped_propagation());
  EXPECT_EQ(WebKit::WebInputEvent::TouchStart, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(WebKit::WebTouchPoint::StatePressed,
            view_->touch_event_.touches[0].state);

  view_->OnTouchEvent(&move);
  EXPECT_TRUE(move.stopped_propagation());
  EXPECT_EQ(WebKit::WebInputEvent::TouchMove, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(WebKit::WebTouchPoint::StateMoved,
            view_->touch_event_.touches[0].state);

  view_->OnTouchEvent(&release);
  EXPECT_TRUE(release.stopped_propagation());
  EXPECT_EQ(WebKit::WebInputEvent::TouchEnd, view_->touch_event_.type);
  EXPECT_EQ(0U, view_->touch_event_.touchesLength);

  // Now start a touch event, and remove the event-handlers before the release.
  view_->OnTouchEvent(&press);
  EXPECT_TRUE(press.stopped_propagation());
  EXPECT_EQ(WebKit::WebInputEvent::TouchStart, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(WebKit::WebTouchPoint::StatePressed,
            view_->touch_event_.touches[0].state);

  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));
  EXPECT_FALSE(widget_host_->ShouldForwardTouchEvent());

  ui::TouchEvent move2(ui::ET_TOUCH_MOVED, gfx::Point(20, 20), 0,
      base::Time::NowFromSystemTime() - base::Time());
  view_->OnTouchEvent(&move2);
  EXPECT_FALSE(move2.handled());
  EXPECT_EQ(WebKit::WebInputEvent::TouchMove, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(WebKit::WebTouchPoint::StateMoved,
            view_->touch_event_.touches[0].state);

  ui::TouchEvent release2(ui::ET_TOUCH_RELEASED, gfx::Point(20, 20), 0,
      base::Time::NowFromSystemTime() - base::Time());
  view_->OnTouchEvent(&release2);
  EXPECT_FALSE(release2.handled());
  EXPECT_EQ(WebKit::WebInputEvent::TouchEnd, view_->touch_event_.type);
  EXPECT_EQ(0U, view_->touch_event_.touchesLength);
}

// Checks that touch-events are queued properly when there is a touch-event
// handler on the page.
TEST_F(RenderWidgetHostViewAuraTest, TouchEventSyncAsync) {
  view_->InitAsChild(NULL);
  view_->Show();

  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_TRUE(widget_host_->ShouldForwardTouchEvent());

  ui::TouchEvent press(ui::ET_TOUCH_PRESSED,
                       gfx::Point(30, 30),
                       0,
                       ui::EventTimeForNow());
  ui::TouchEvent move(ui::ET_TOUCH_MOVED,
                      gfx::Point(20, 20),
                      0,
                      ui::EventTimeForNow());
  ui::TouchEvent release(ui::ET_TOUCH_RELEASED,
                         gfx::Point(20, 20),
                         0,
                         ui::EventTimeForNow());

  view_->OnTouchEvent(&press);
  EXPECT_TRUE(press.stopped_propagation());
  EXPECT_EQ(WebKit::WebInputEvent::TouchStart, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(WebKit::WebTouchPoint::StatePressed,
            view_->touch_event_.touches[0].state);

  view_->OnTouchEvent(&move);
  EXPECT_TRUE(move.stopped_propagation());
  EXPECT_EQ(WebKit::WebInputEvent::TouchMove, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(WebKit::WebTouchPoint::StateMoved,
            view_->touch_event_.touches[0].state);

  // Send the same move event. Since the point hasn't moved, it won't affect the
  // queue. However, the view should consume the event.
  view_->OnTouchEvent(&move);
  EXPECT_TRUE(move.stopped_propagation());
  EXPECT_EQ(WebKit::WebInputEvent::TouchMove, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(WebKit::WebTouchPoint::StateMoved,
            view_->touch_event_.touches[0].state);

  view_->OnTouchEvent(&release);
  EXPECT_TRUE(release.stopped_propagation());
  EXPECT_EQ(WebKit::WebInputEvent::TouchEnd, view_->touch_event_.type);
  EXPECT_EQ(0U, view_->touch_event_.touchesLength);
}

TEST_F(RenderWidgetHostViewAuraTest, PhysicalBackingSizeWithScale) {
  view_->InitAsChild(NULL);
  view_->GetNativeView()->SetDefaultParentByRootWindow(
      parent_view_->GetNativeView()->GetRootWindow(), gfx::Rect());
  sink_->ClearMessages();
  view_->SetSize(gfx::Size(100, 100));
  EXPECT_EQ("100x100", view_->GetPhysicalBackingSize().ToString());
  EXPECT_EQ(1u, sink_->message_count());
  EXPECT_EQ(ViewMsg_Resize::ID, sink_->GetMessageAt(0)->type());
  {
    const IPC::Message* msg = sink_->GetMessageAt(0);
    EXPECT_EQ(ViewMsg_Resize::ID, msg->type());
    ViewMsg_Resize::Param params;
    ViewMsg_Resize::Read(msg, &params);
    EXPECT_EQ("100x100", params.a.new_size.ToString());  // dip size
    EXPECT_EQ("100x100",
        params.a.physical_backing_size.ToString());  // backing size
  }

  widget_host_->ResetSizeAndRepaintPendingFlags();
  sink_->ClearMessages();

  aura_test_helper_->test_screen()->SetDeviceScaleFactor(2.0f);
  EXPECT_EQ("200x200", view_->GetPhysicalBackingSize().ToString());
  // Extra ScreenInfoChanged message for |parent_view_|.
  EXPECT_EQ(1u, sink_->message_count());
  {
    const IPC::Message* msg = sink_->GetMessageAt(0);
    EXPECT_EQ(ViewMsg_Resize::ID, msg->type());
    ViewMsg_Resize::Param params;
    ViewMsg_Resize::Read(msg, &params);
    EXPECT_EQ(2.0f, params.a.screen_info.deviceScaleFactor);
    EXPECT_EQ("100x100", params.a.new_size.ToString());  // dip size
    EXPECT_EQ("200x200",
        params.a.physical_backing_size.ToString());  // backing size
  }

  widget_host_->ResetSizeAndRepaintPendingFlags();
  sink_->ClearMessages();

  aura_test_helper_->test_screen()->SetDeviceScaleFactor(1.0f);
  // Extra ScreenInfoChanged message for |parent_view_|.
  EXPECT_EQ(1u, sink_->message_count());
  EXPECT_EQ("100x100", view_->GetPhysicalBackingSize().ToString());
  {
    const IPC::Message* msg = sink_->GetMessageAt(0);
    EXPECT_EQ(ViewMsg_Resize::ID, msg->type());
    ViewMsg_Resize::Param params;
    ViewMsg_Resize::Read(msg, &params);
    EXPECT_EQ(1.0f, params.a.screen_info.deviceScaleFactor);
    EXPECT_EQ("100x100", params.a.new_size.ToString());  // dip size
    EXPECT_EQ("100x100",
        params.a.physical_backing_size.ToString());  // backing size
  }
}

// Checks that InputMsg_CursorVisibilityChange IPC messages are dispatched
// to the renderer at the correct times.
TEST_F(RenderWidgetHostViewAuraTest, CursorVisibilityChange) {
  view_->InitAsChild(NULL);
  view_->GetNativeView()->SetDefaultParentByRootWindow(
      parent_view_->GetNativeView()->GetRootWindow(), gfx::Rect());
  view_->SetSize(gfx::Size(100, 100));

  aura::test::TestCursorClient cursor_client(
      parent_view_->GetNativeView()->GetRootWindow());

  cursor_client.AddObserver(view_);

  // Expect a message the first time the cursor is shown.
  view_->WasShown();
  sink_->ClearMessages();
  cursor_client.ShowCursor();
  EXPECT_EQ(1u, sink_->message_count());
  EXPECT_TRUE(sink_->GetUniqueMessageMatching(
      InputMsg_CursorVisibilityChange::ID));

  // No message expected if the renderer already knows the cursor is visible.
  sink_->ClearMessages();
  cursor_client.ShowCursor();
  EXPECT_EQ(0u, sink_->message_count());

  // Hiding the cursor should send a message.
  sink_->ClearMessages();
  cursor_client.HideCursor();
  EXPECT_EQ(1u, sink_->message_count());
  EXPECT_TRUE(sink_->GetUniqueMessageMatching(
      InputMsg_CursorVisibilityChange::ID));

  // No message expected if the renderer already knows the cursor is invisible.
  sink_->ClearMessages();
  cursor_client.HideCursor();
  EXPECT_EQ(0u, sink_->message_count());

  // No messages should be sent while the view is invisible.
  view_->WasHidden();
  sink_->ClearMessages();
  cursor_client.ShowCursor();
  EXPECT_EQ(0u, sink_->message_count());
  cursor_client.HideCursor();
  EXPECT_EQ(0u, sink_->message_count());

  // Show the view. Since the cursor was invisible when the view was hidden,
  // no message should be sent.
  sink_->ClearMessages();
  view_->WasShown();
  EXPECT_FALSE(sink_->GetUniqueMessageMatching(
      InputMsg_CursorVisibilityChange::ID));

  // No message expected if the renderer already knows the cursor is invisible.
  sink_->ClearMessages();
  cursor_client.HideCursor();
  EXPECT_EQ(0u, sink_->message_count());

  // Showing the cursor should send a message.
  sink_->ClearMessages();
  cursor_client.ShowCursor();
  EXPECT_EQ(1u, sink_->message_count());
  EXPECT_TRUE(sink_->GetUniqueMessageMatching(
      InputMsg_CursorVisibilityChange::ID));

  // No messages should be sent while the view is invisible.
  view_->WasHidden();
  sink_->ClearMessages();
  cursor_client.HideCursor();
  EXPECT_EQ(0u, sink_->message_count());

  // Show the view. Since the cursor was visible when the view was hidden,
  // a message is expected to be sent.
  sink_->ClearMessages();
  view_->WasShown();
  EXPECT_TRUE(sink_->GetUniqueMessageMatching(
      InputMsg_CursorVisibilityChange::ID));

  cursor_client.RemoveObserver(view_);
}

// Resizing in fullscreen mode should send the up-to-date screen info.
TEST_F(RenderWidgetHostViewAuraTest, FullscreenResize) {
  aura::RootWindow* root_window = aura_test_helper_->root_window();
  root_window->SetLayoutManager(new FullscreenLayoutManager(root_window));
  view_->InitAsFullscreen(parent_view_);
  view_->WasShown();
  widget_host_->ResetSizeAndRepaintPendingFlags();
  sink_->ClearMessages();

  // Call WasResized to flush the old screen info.
  view_->GetRenderWidgetHost()->WasResized();
  {
    // 0 is CreatingNew message.
    const IPC::Message* msg = sink_->GetMessageAt(0);
    EXPECT_EQ(ViewMsg_Resize::ID, msg->type());
    ViewMsg_Resize::Param params;
    ViewMsg_Resize::Read(msg, &params);
    EXPECT_EQ("0,0 800x600",
              gfx::Rect(params.a.screen_info.availableRect).ToString());
    EXPECT_EQ("800x600", params.a.new_size.ToString());
  }

  widget_host_->ResetSizeAndRepaintPendingFlags();
  sink_->ClearMessages();

  // Make sure the corrent screen size is set along in the resize
  // request when the screen size has changed.
  aura_test_helper_->test_screen()->SetUIScale(0.5);
  EXPECT_EQ(1u, sink_->message_count());
  {
    const IPC::Message* msg = sink_->GetMessageAt(0);
    EXPECT_EQ(ViewMsg_Resize::ID, msg->type());
    ViewMsg_Resize::Param params;
    ViewMsg_Resize::Read(msg, &params);
    EXPECT_EQ("0,0 1600x1200",
              gfx::Rect(params.a.screen_info.availableRect).ToString());
    EXPECT_EQ("1600x1200", params.a.new_size.ToString());
  }
}

}  // namespace content
