// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_helper.h"
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
  virtual void OnWindowDestroyed(aura::Window* window) {
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

    parent_host_ = new RenderWidgetHostImpl(
        &delegate_, process_host, MSG_ROUTING_NONE);
    parent_view_ = static_cast<RenderWidgetHostViewAura*>(
        RenderWidgetHostView::CreateViewForWidget(parent_host_));
    parent_view_->InitAsChild(NULL);
    parent_view_->GetNativeView()->SetDefaultParentByRootWindow(
        aura_test_helper_->root_window(), gfx::Rect());

    widget_host_ = new RenderWidgetHostImpl(
        &delegate_, process_host, MSG_ROUTING_NONE);
    view_ = static_cast<RenderWidgetHostViewAura*>(
        RenderWidgetHostView::CreateViewForWidget(widget_host_));
  }

  virtual void TearDown() {
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
  MessageLoopForUI message_loop_;
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

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAuraTest);
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

}  // namespace content
