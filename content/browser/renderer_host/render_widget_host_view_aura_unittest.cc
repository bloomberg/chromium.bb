// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/basictypes.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/copy_output_request.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/compositor/resize_lock.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

using testing::_;

namespace content {
namespace {

// Simple screen position client to test coordinate system conversion.
class TestScreenPositionClient
    : public aura::client::ScreenPositionClient {
 public:
  TestScreenPositionClient() {}
  virtual ~TestScreenPositionClient() {}

  // aura::client::ScreenPositionClient overrides:
  virtual void ConvertPointToScreen(const aura::Window* window,
      gfx::Point* point) OVERRIDE {
    point->Offset(-1, -1);
  }

  virtual void ConvertPointFromScreen(const aura::Window* window,
      gfx::Point* point) OVERRIDE {
    point->Offset(1, 1);
  }

  virtual void ConvertHostPointToScreen(aura::Window* window,
      gfx::Point* point) OVERRIDE {
    ConvertPointToScreen(window, point);
  }

  virtual void SetBounds(aura::Window* window,
      const gfx::Rect& bounds,
      const gfx::Display& display) OVERRIDE {
  }
};

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

class FakeFrameSubscriber : public RenderWidgetHostViewFrameSubscriber {
 public:
  FakeFrameSubscriber(gfx::Size size, base::Callback<void(bool)> callback)
      : size_(size), callback_(callback) {}

  virtual bool ShouldCaptureFrame(base::TimeTicks present_time,
                                  scoped_refptr<media::VideoFrame>* storage,
                                  DeliverFrameCallback* callback) OVERRIDE {
    *storage = media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                              size_,
                                              gfx::Rect(size_),
                                              size_,
                                              base::TimeDelta());
    *callback = base::Bind(&FakeFrameSubscriber::CallbackMethod, callback_);
    return true;
  }

  static void CallbackMethod(base::Callback<void(bool)> callback,
                             base::TimeTicks timestamp,
                             bool success) {
    callback.Run(success);
  }

 private:
  gfx::Size size_;
  base::Callback<void(bool)> callback_;
};

class FakeRenderWidgetHostViewAura : public RenderWidgetHostViewAura {
 public:
  FakeRenderWidgetHostViewAura(RenderWidgetHost* widget)
      : RenderWidgetHostViewAura(widget), has_resize_lock_(false) {}

  virtual ~FakeRenderWidgetHostViewAura() {}

  virtual bool ShouldCreateResizeLock() OVERRIDE {
    gfx::Size desired_size = window()->bounds().size();
    return desired_size != current_frame_size_in_dip();
  }

  virtual scoped_ptr<ResizeLock> CreateResizeLock(bool defer_compositor_lock)
      OVERRIDE {
    gfx::Size desired_size = window()->bounds().size();
    return scoped_ptr<ResizeLock>(
        new FakeResizeLock(desired_size, defer_compositor_lock));
  }

  virtual void RequestCopyOfOutput(scoped_ptr<cc::CopyOutputRequest> request)
      OVERRIDE {
    last_copy_request_ = request.Pass();
    if (last_copy_request_->has_texture_mailbox()) {
      // Give the resulting texture a size.
      GLHelper* gl_helper = ImageTransportFactory::GetInstance()->GetGLHelper();
      GLuint texture = gl_helper->ConsumeMailboxToTexture(
          last_copy_request_->texture_mailbox().mailbox(),
          last_copy_request_->texture_mailbox().sync_point());
      gl_helper->ResizeTexture(texture, window()->bounds().size());
      gl_helper->DeleteTexture(texture);
    }
  }

  void RunOnCompositingDidCommit() {
    OnCompositingDidCommit(window()->GetHost()->compositor());
  }

  // A lock that doesn't actually do anything to the compositor, and does not
  // time out.
  class FakeResizeLock : public ResizeLock {
   public:
    FakeResizeLock(const gfx::Size new_size, bool defer_compositor_lock)
        : ResizeLock(new_size, defer_compositor_lock) {}
  };

  bool has_resize_lock_;
  gfx::Size last_frame_size_;
  scoped_ptr<cc::CopyOutputRequest> last_copy_request_;
};

class RenderWidgetHostViewAuraTest : public testing::Test {
 public:
  RenderWidgetHostViewAuraTest()
      : browser_thread_for_ui_(BrowserThread::UI, &message_loop_) {}

  void SetUpEnvironment() {
    ImageTransportFactory::InitializeForUnitTests(
        scoped_ptr<ui::ContextFactory>(new ui::InProcessContextFactory));
    aura_test_helper_.reset(new aura::test::AuraTestHelper(&message_loop_));
    aura_test_helper_->SetUp();

    browser_context_.reset(new TestBrowserContext);
    process_host_ = new MockRenderProcessHost(browser_context_.get());

    sink_ = &process_host_->sink();

    parent_host_ = new RenderWidgetHostImpl(
        &delegate_, process_host_, MSG_ROUTING_NONE, false);
    parent_view_ = static_cast<RenderWidgetHostViewAura*>(
        RenderWidgetHostView::CreateViewForWidget(parent_host_));
    parent_view_->InitAsChild(NULL);
    aura::client::ParentWindowWithContext(parent_view_->GetNativeView(),
                                          aura_test_helper_->root_window(),
                                          gfx::Rect());

    widget_host_ = new RenderWidgetHostImpl(
        &delegate_, process_host_, MSG_ROUTING_NONE, false);
    widget_host_->Init();
    widget_host_->OnMessageReceived(
        ViewHostMsg_DidActivateAcceleratedCompositing(0, true));
    view_ = new FakeRenderWidgetHostViewAura(widget_host_);
  }

  void TearDownEnvironment() {
    sink_ = NULL;
    process_host_ = NULL;
    if (view_)
      view_->Destroy();
    delete widget_host_;

    parent_view_->Destroy();
    delete parent_host_;

    browser_context_.reset();
    aura_test_helper_->TearDown();

    message_loop_.DeleteSoon(FROM_HERE, browser_context_.release());
    message_loop_.RunUntilIdle();
    ImageTransportFactory::Terminate();
  }

  virtual void SetUp() { SetUpEnvironment(); }

  virtual void TearDown() { TearDownEnvironment(); }

 protected:
  base::MessageLoopForUI message_loop_;
  BrowserThreadImpl browser_thread_for_ui_;
  scoped_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  scoped_ptr<BrowserContext> browser_context_;
  MockRenderWidgetHostDelegate delegate_;
  MockRenderProcessHost* process_host_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  RenderWidgetHostImpl* parent_host_;
  RenderWidgetHostViewAura* parent_view_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  RenderWidgetHostImpl* widget_host_;
  FakeRenderWidgetHostViewAura* view_;

  IPC::TestSink* sink_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAuraTest);
};

class RenderWidgetHostViewAuraShutdownTest
    : public RenderWidgetHostViewAuraTest {
 public:
  RenderWidgetHostViewAuraShutdownTest() {}

  virtual void TearDown() OVERRIDE {
    // No TearDownEnvironment here, we do this explicitly during the test.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAuraShutdownTest);
};

// A layout manager that always resizes a child to the root window size.
class FullscreenLayoutManager : public aura::LayoutManager {
 public:
  explicit FullscreenLayoutManager(aura::Window* owner)
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
  aura::Window* owner_;
  DISALLOW_COPY_AND_ASSIGN(FullscreenLayoutManager);
};

class MockWindowObserver : public aura::WindowObserver {
 public:
  MOCK_METHOD2(OnWindowPaintScheduled, void(aura::Window*, const gfx::Rect&));
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

// Checks that a popup is positioned correctly relative to its parent using
// screen coordinates.
TEST_F(RenderWidgetHostViewAuraTest, PositionChildPopup) {
  TestScreenPositionClient screen_position_client;

  aura::Window* window = parent_view_->GetNativeView();
  aura::Window* root = window->GetRootWindow();
  aura::client::SetScreenPositionClient(root, &screen_position_client);

  parent_view_->SetBounds(gfx::Rect(10, 10, 800, 600));
  gfx::Rect bounds_in_screen = parent_view_->GetViewBounds();
  int horiz = bounds_in_screen.width() / 4;
  int vert = bounds_in_screen.height() / 4;
  bounds_in_screen.Inset(horiz, vert);

  // Verify that when the popup is initialized for the first time, it correctly
  // treats the input bounds as screen coordinates.
  view_->InitAsPopup(parent_view_, bounds_in_screen);

  gfx::Rect final_bounds_in_screen = view_->GetViewBounds();
  EXPECT_EQ(final_bounds_in_screen.ToString(), bounds_in_screen.ToString());

  // Verify that directly setting the bounds via SetBounds() treats the input
  // as screen coordinates.
  bounds_in_screen = gfx::Rect(60, 60, 100, 100);
  view_->SetBounds(bounds_in_screen);
  final_bounds_in_screen = view_->GetViewBounds();
  EXPECT_EQ(final_bounds_in_screen.ToString(), bounds_in_screen.ToString());

  // Verify that setting the size does not alter the origin.
  gfx::Point original_origin = window->bounds().origin();
  view_->SetSize(gfx::Size(120, 120));
  gfx::Point new_origin = window->bounds().origin();
  EXPECT_EQ(original_origin.ToString(), new_origin.ToString());

  aura::client::SetScreenPositionClient(root, NULL);
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
  sibling->Init(aura::WINDOW_LAYER_TEXTURED);
  sibling->Show();
  window->parent()->AddChild(sibling.get());
  sibling->Focus();
  ASSERT_TRUE(sibling->HasFocus());
  ASSERT_TRUE(observer.destroyed());

  widget_host_ = NULL;
  view_ = NULL;
}

// Checks that a popup view is destroyed when a user clicks outside of the popup
// view and focus does not change. This is the case when the user clicks on the
// desktop background on Chrome OS.
TEST_F(RenderWidgetHostViewAuraTest, DestroyPopupClickOutsidePopup) {
  parent_view_->SetBounds(gfx::Rect(10, 10, 400, 400));
  parent_view_->Focus();
  EXPECT_TRUE(parent_view_->HasFocus());

  view_->InitAsPopup(parent_view_, gfx::Rect(10, 10, 100, 100));
  aura::Window* window = view_->GetNativeView();
  ASSERT_TRUE(window != NULL);

  gfx::Point click_point;
  EXPECT_FALSE(window->GetBoundsInRootWindow().Contains(click_point));
  aura::Window* parent_window = parent_view_->GetNativeView();
  EXPECT_FALSE(parent_window->GetBoundsInRootWindow().Contains(click_point));

  TestWindowObserver observer(window);
  aura::test::EventGenerator generator(window->GetRootWindow(), click_point);
  generator.ClickLeftButton();
  ASSERT_TRUE(parent_view_->HasFocus());
  ASSERT_TRUE(observer.destroyed());

  widget_host_ = NULL;
  view_ = NULL;
}

// Checks that a popup view is destroyed when a user taps outside of the popup
// view and focus does not change. This is the case when the user taps the
// desktop background on Chrome OS.
TEST_F(RenderWidgetHostViewAuraTest, DestroyPopupTapOutsidePopup) {
  parent_view_->SetBounds(gfx::Rect(10, 10, 400, 400));
  parent_view_->Focus();
  EXPECT_TRUE(parent_view_->HasFocus());

  view_->InitAsPopup(parent_view_, gfx::Rect(10, 10, 100, 100));
  aura::Window* window = view_->GetNativeView();
  ASSERT_TRUE(window != NULL);

  gfx::Point tap_point;
  EXPECT_FALSE(window->GetBoundsInRootWindow().Contains(tap_point));
  aura::Window* parent_window = parent_view_->GetNativeView();
  EXPECT_FALSE(parent_window->GetBoundsInRootWindow().Contains(tap_point));

  TestWindowObserver observer(window);
  aura::test::EventGenerator generator(window->GetRootWindow(), tap_point);
  generator.GestureTapAt(tap_point);
  ASSERT_TRUE(parent_view_->HasFocus());
  ASSERT_TRUE(observer.destroyed());

  widget_host_ = NULL;
  view_ = NULL;
}

// Checks that IME-composition-event state is maintained correctly.
TEST_F(RenderWidgetHostViewAuraTest, SetCompositionText) {
  view_->InitAsChild(NULL);
  view_->Show();

  ui::CompositionText composition_text;
  composition_text.text = base::ASCIIToUTF16("|a|b");

  // Focused segment
  composition_text.underlines.push_back(
      ui::CompositionUnderline(0, 3, 0xff000000, true));

  // Non-focused segment
  composition_text.underlines.push_back(
      ui::CompositionUnderline(3, 4, 0xff000000, false));

  const ui::CompositionUnderlines& underlines = composition_text.underlines;

  // Caret is at the end. (This emulates Japanese MSIME 2007 and later)
  composition_text.selection = gfx::Range(4);

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
  EXPECT_EQ(blink::WebInputEvent::TouchStart, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(blink::WebTouchPoint::StatePressed,
            view_->touch_event_.touches[0].state);

  view_->OnTouchEvent(&move);
  EXPECT_FALSE(move.handled());
  EXPECT_EQ(blink::WebInputEvent::TouchMove, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(blink::WebTouchPoint::StateMoved,
            view_->touch_event_.touches[0].state);

  view_->OnTouchEvent(&release);
  EXPECT_FALSE(release.handled());
  EXPECT_EQ(blink::WebInputEvent::TouchEnd, view_->touch_event_.type);
  EXPECT_EQ(0U, view_->touch_event_.touchesLength);

  // Now install some touch-event handlers and do the same steps. The touch
  // events should now be consumed. However, the touch-event state should be
  // updated as before.
  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_TRUE(widget_host_->ShouldForwardTouchEvent());

  view_->OnTouchEvent(&press);
  EXPECT_TRUE(press.stopped_propagation());
  EXPECT_EQ(blink::WebInputEvent::TouchStart, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(blink::WebTouchPoint::StatePressed,
            view_->touch_event_.touches[0].state);

  view_->OnTouchEvent(&move);
  EXPECT_TRUE(move.stopped_propagation());
  EXPECT_EQ(blink::WebInputEvent::TouchMove, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(blink::WebTouchPoint::StateMoved,
            view_->touch_event_.touches[0].state);

  view_->OnTouchEvent(&release);
  EXPECT_TRUE(release.stopped_propagation());
  EXPECT_EQ(blink::WebInputEvent::TouchEnd, view_->touch_event_.type);
  EXPECT_EQ(0U, view_->touch_event_.touchesLength);

  // Now start a touch event, and remove the event-handlers before the release.
  view_->OnTouchEvent(&press);
  EXPECT_TRUE(press.stopped_propagation());
  EXPECT_EQ(blink::WebInputEvent::TouchStart, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(blink::WebTouchPoint::StatePressed,
            view_->touch_event_.touches[0].state);

  widget_host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));
  EXPECT_FALSE(widget_host_->ShouldForwardTouchEvent());

  ui::TouchEvent move2(ui::ET_TOUCH_MOVED, gfx::Point(20, 20), 0,
      base::Time::NowFromSystemTime() - base::Time());
  view_->OnTouchEvent(&move2);
  EXPECT_FALSE(move2.handled());
  EXPECT_EQ(blink::WebInputEvent::TouchMove, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(blink::WebTouchPoint::StateMoved,
            view_->touch_event_.touches[0].state);

  ui::TouchEvent release2(ui::ET_TOUCH_RELEASED, gfx::Point(20, 20), 0,
      base::Time::NowFromSystemTime() - base::Time());
  view_->OnTouchEvent(&release2);
  EXPECT_FALSE(release2.handled());
  EXPECT_EQ(blink::WebInputEvent::TouchEnd, view_->touch_event_.type);
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
  EXPECT_EQ(blink::WebInputEvent::TouchStart, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(blink::WebTouchPoint::StatePressed,
            view_->touch_event_.touches[0].state);

  view_->OnTouchEvent(&move);
  EXPECT_TRUE(move.stopped_propagation());
  EXPECT_EQ(blink::WebInputEvent::TouchMove, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(blink::WebTouchPoint::StateMoved,
            view_->touch_event_.touches[0].state);

  // Send the same move event. Since the point hasn't moved, it won't affect the
  // queue. However, the view should consume the event.
  view_->OnTouchEvent(&move);
  EXPECT_TRUE(move.stopped_propagation());
  EXPECT_EQ(blink::WebInputEvent::TouchMove, view_->touch_event_.type);
  EXPECT_EQ(1U, view_->touch_event_.touchesLength);
  EXPECT_EQ(blink::WebTouchPoint::StateMoved,
            view_->touch_event_.touches[0].state);

  view_->OnTouchEvent(&release);
  EXPECT_TRUE(release.stopped_propagation());
  EXPECT_EQ(blink::WebInputEvent::TouchEnd, view_->touch_event_.type);
  EXPECT_EQ(0U, view_->touch_event_.touchesLength);
}

TEST_F(RenderWidgetHostViewAuraTest, PhysicalBackingSizeWithScale) {
  view_->InitAsChild(NULL);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
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
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
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

TEST_F(RenderWidgetHostViewAuraTest, UpdateCursorIfOverSelf) {
  view_->InitAsChild(NULL);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());

  // Note that all coordinates in this test are screen coordinates.
  view_->SetBounds(gfx::Rect(60, 60, 100, 100));
  view_->Show();

  aura::test::TestCursorClient cursor_client(
      parent_view_->GetNativeView()->GetRootWindow());

  // Cursor is in the middle of the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->set_last_mouse_location(gfx::Point(110, 110));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(1, cursor_client.calls_to_set_cursor());

  // Cursor is near the top of the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->set_last_mouse_location(gfx::Point(80, 65));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(1, cursor_client.calls_to_set_cursor());

  // Cursor is near the bottom of the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->set_last_mouse_location(gfx::Point(159, 159));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(1, cursor_client.calls_to_set_cursor());

  // Cursor is above the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->set_last_mouse_location(gfx::Point(67, 59));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(0, cursor_client.calls_to_set_cursor());

  // Cursor is below the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->set_last_mouse_location(gfx::Point(161, 161));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(0, cursor_client.calls_to_set_cursor());
}

scoped_ptr<cc::CompositorFrame> MakeDelegatedFrame(float scale_factor,
                                                   gfx::Size size,
                                                   gfx::Rect damage) {
  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  frame->metadata.device_scale_factor = scale_factor;
  frame->delegated_frame_data.reset(new cc::DelegatedFrameData);

  scoped_ptr<cc::RenderPass> pass = cc::RenderPass::Create();
  pass->SetNew(
      cc::RenderPass::Id(1, 1), gfx::Rect(size), damage, gfx::Transform());
  frame->delegated_frame_data->render_pass_list.push_back(pass.Pass());
  return frame.Pass();
}

// Resizing in fullscreen mode should send the up-to-date screen info.
TEST_F(RenderWidgetHostViewAuraTest, FullscreenResize) {
  aura::Window* root_window = aura_test_helper_->root_window();
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
    // Resizes are blocked until we swapped a frame of the correct size, and
    // we've committed it.
    view_->OnSwapCompositorFrame(
        0,
        MakeDelegatedFrame(
            1.f, params.a.new_size, gfx::Rect(params.a.new_size)));
    ui::DrawWaiterForTest::WaitForCommit(
        root_window->GetHost()->compositor());
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
    view_->OnSwapCompositorFrame(
        0,
        MakeDelegatedFrame(
            1.f, params.a.new_size, gfx::Rect(params.a.new_size)));
    ui::DrawWaiterForTest::WaitForCommit(
        root_window->GetHost()->compositor());
  }
}

// Swapping a frame should notify the window.
TEST_F(RenderWidgetHostViewAuraTest, SwapNotifiesWindow) {
  gfx::Size view_size(100, 100);
  gfx::Rect view_rect(view_size);

  view_->InitAsChild(NULL);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_size);
  view_->WasShown();

  MockWindowObserver observer;
  view_->window_->AddObserver(&observer);

  // Delegated renderer path
  EXPECT_CALL(observer, OnWindowPaintScheduled(view_->window_, view_rect));
  view_->OnSwapCompositorFrame(
      0, MakeDelegatedFrame(1.f, view_size, view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnWindowPaintScheduled(view_->window_,
                                               gfx::Rect(5, 5, 5, 5)));
  view_->OnSwapCompositorFrame(
      0, MakeDelegatedFrame(1.f, view_size, gfx::Rect(5, 5, 5, 5)));
  testing::Mock::VerifyAndClearExpectations(&observer);

  view_->window_->RemoveObserver(&observer);
}

// Skipped frames should not drop their damage.
TEST_F(RenderWidgetHostViewAuraTest, SkippedDelegatedFrames) {
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();

  view_->InitAsChild(NULL);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_rect.size());

  MockWindowObserver observer;
  view_->window_->AddObserver(&observer);

  // A full frame of damage.
  EXPECT_CALL(observer, OnWindowPaintScheduled(view_->window_, view_rect));
  view_->OnSwapCompositorFrame(
      0, MakeDelegatedFrame(1.f, frame_size, view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  // A partial damage frame.
  gfx::Rect partial_view_rect(30, 30, 20, 20);
  EXPECT_CALL(observer,
              OnWindowPaintScheduled(view_->window_, partial_view_rect));
  view_->OnSwapCompositorFrame(
      0, MakeDelegatedFrame(1.f, frame_size, partial_view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  // Lock the compositor. Now we should drop frames.
  view_rect = gfx::Rect(150, 150);
  view_->SetSize(view_rect.size());
  view_->MaybeCreateResizeLock();

  // This frame is dropped.
  gfx::Rect dropped_damage_rect_1(10, 20, 30, 40);
  EXPECT_CALL(observer, OnWindowPaintScheduled(_, _)).Times(0);
  view_->OnSwapCompositorFrame(
      0, MakeDelegatedFrame(1.f, frame_size, dropped_damage_rect_1));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  gfx::Rect dropped_damage_rect_2(40, 50, 10, 20);
  EXPECT_CALL(observer, OnWindowPaintScheduled(_, _)).Times(0);
  view_->OnSwapCompositorFrame(
      0, MakeDelegatedFrame(1.f, frame_size, dropped_damage_rect_2));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  // Unlock the compositor. This frame should damage everything.
  frame_size = view_rect.size();

  gfx::Rect new_damage_rect(5, 6, 10, 10);
  EXPECT_CALL(observer,
              OnWindowPaintScheduled(view_->window_, view_rect));
  view_->OnSwapCompositorFrame(
      0, MakeDelegatedFrame(1.f, frame_size, new_damage_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  // A partial damage frame, this should not be dropped.
  EXPECT_CALL(observer,
              OnWindowPaintScheduled(view_->window_, partial_view_rect));
  view_->OnSwapCompositorFrame(
      0, MakeDelegatedFrame(1.f, frame_size, partial_view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();


  view_->window_->RemoveObserver(&observer);
}

TEST_F(RenderWidgetHostViewAuraTest, OutputSurfaceIdChange) {
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();

  view_->InitAsChild(NULL);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_rect.size());

  MockWindowObserver observer;
  view_->window_->AddObserver(&observer);

  // Swap a frame.
  EXPECT_CALL(observer, OnWindowPaintScheduled(view_->window_, view_rect));
  view_->OnSwapCompositorFrame(
      0, MakeDelegatedFrame(1.f, frame_size, view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  // Swap a frame with a different surface id.
  EXPECT_CALL(observer, OnWindowPaintScheduled(view_->window_, view_rect));
  view_->OnSwapCompositorFrame(
      1, MakeDelegatedFrame(1.f, frame_size, view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  // Swap an empty frame, with a different surface id.
  view_->OnSwapCompositorFrame(
      2, MakeDelegatedFrame(1.f, gfx::Size(), gfx::Rect()));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  // Swap another frame, with a different surface id.
  EXPECT_CALL(observer, OnWindowPaintScheduled(view_->window_, view_rect));
  view_->OnSwapCompositorFrame(3,
                               MakeDelegatedFrame(1.f, frame_size, view_rect));
  testing::Mock::VerifyAndClearExpectations(&observer);
  view_->RunOnCompositingDidCommit();

  view_->window_->RemoveObserver(&observer);
}

TEST_F(RenderWidgetHostViewAuraTest, DiscardDelegatedFrames) {
  size_t max_renderer_frames =
      RendererFrameManager::GetInstance()->max_number_of_saved_frames();
  ASSERT_LE(2u, max_renderer_frames);
  size_t renderer_count = max_renderer_frames + 1;
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();

  scoped_ptr<RenderWidgetHostImpl * []> hosts(
      new RenderWidgetHostImpl* [renderer_count]);
  scoped_ptr<FakeRenderWidgetHostViewAura * []> views(
      new FakeRenderWidgetHostViewAura* [renderer_count]);

  // Create a bunch of renderers.
  for (size_t i = 0; i < renderer_count; ++i) {
    hosts[i] = new RenderWidgetHostImpl(
        &delegate_, process_host_, MSG_ROUTING_NONE, false);
    hosts[i]->Init();
    hosts[i]->OnMessageReceived(
        ViewHostMsg_DidActivateAcceleratedCompositing(0, true));
    views[i] = new FakeRenderWidgetHostViewAura(hosts[i]);
    views[i]->InitAsChild(NULL);
    aura::client::ParentWindowWithContext(
        views[i]->GetNativeView(),
        parent_view_->GetNativeView()->GetRootWindow(),
        gfx::Rect());
    views[i]->SetSize(view_rect.size());
  }

  // Make each renderer visible, and swap a frame on it, then make it invisible.
  for (size_t i = 0; i < renderer_count; ++i) {
    views[i]->WasShown();
    views[i]->OnSwapCompositorFrame(
        1, MakeDelegatedFrame(1.f, frame_size, view_rect));
    EXPECT_TRUE(views[i]->frame_provider_);
    views[i]->WasHidden();
  }

  // There should be max_renderer_frames with a frame in it, and one without it.
  // Since the logic is LRU eviction, the first one should be without.
  EXPECT_FALSE(views[0]->frame_provider_);
  for (size_t i = 1; i < renderer_count; ++i)
    EXPECT_TRUE(views[i]->frame_provider_);

  // LRU renderer is [0], make it visible, it shouldn't evict anything yet.
  views[0]->WasShown();
  EXPECT_FALSE(views[0]->frame_provider_);
  EXPECT_TRUE(views[1]->frame_provider_);

  // Swap a frame on it, it should evict the next LRU [1].
  views[0]->OnSwapCompositorFrame(
      1, MakeDelegatedFrame(1.f, frame_size, view_rect));
  EXPECT_TRUE(views[0]->frame_provider_);
  EXPECT_FALSE(views[1]->frame_provider_);
  views[0]->WasHidden();

  // LRU renderer is [1], still hidden. Swap a frame on it, it should evict
  // the next LRU [2].
  views[1]->OnSwapCompositorFrame(
      1, MakeDelegatedFrame(1.f, frame_size, view_rect));
  EXPECT_TRUE(views[0]->frame_provider_);
  EXPECT_TRUE(views[1]->frame_provider_);
  EXPECT_FALSE(views[2]->frame_provider_);
  for (size_t i = 3; i < renderer_count; ++i)
    EXPECT_TRUE(views[i]->frame_provider_);

  // Make all renderers but [0] visible and swap a frame on them, keep [0]
  // hidden, it becomes the LRU.
  for (size_t i = 1; i < renderer_count; ++i) {
    views[i]->WasShown();
    views[i]->OnSwapCompositorFrame(
        1, MakeDelegatedFrame(1.f, frame_size, view_rect));
    EXPECT_TRUE(views[i]->frame_provider_);
  }
  EXPECT_FALSE(views[0]->frame_provider_);

  // Swap a frame on [0], it should be evicted immediately.
  views[0]->OnSwapCompositorFrame(
      1, MakeDelegatedFrame(1.f, frame_size, view_rect));
  EXPECT_FALSE(views[0]->frame_provider_);

  // Make [0] visible, and swap a frame on it. Nothing should be evicted
  // although we're above the limit.
  views[0]->WasShown();
  views[0]->OnSwapCompositorFrame(
      1, MakeDelegatedFrame(1.f, frame_size, view_rect));
  for (size_t i = 0; i < renderer_count; ++i)
    EXPECT_TRUE(views[i]->frame_provider_);

  // Make [0] hidden, it should evict its frame.
  views[0]->WasHidden();
  EXPECT_FALSE(views[0]->frame_provider_);

  for (size_t i = 0; i < renderer_count; ++i) {
    views[i]->Destroy();
    delete hosts[i];
  }
}

TEST_F(RenderWidgetHostViewAuraTest, DiscardDelegatedFramesWithLocking) {
  size_t max_renderer_frames =
      RendererFrameManager::GetInstance()->max_number_of_saved_frames();
  ASSERT_LE(2u, max_renderer_frames);
  size_t renderer_count = max_renderer_frames + 1;
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size = view_rect.size();

  scoped_ptr<RenderWidgetHostImpl * []> hosts(
      new RenderWidgetHostImpl* [renderer_count]);
  scoped_ptr<FakeRenderWidgetHostViewAura * []> views(
      new FakeRenderWidgetHostViewAura* [renderer_count]);

  // Create a bunch of renderers.
  for (size_t i = 0; i < renderer_count; ++i) {
    hosts[i] = new RenderWidgetHostImpl(
        &delegate_, process_host_, MSG_ROUTING_NONE, false);
    hosts[i]->Init();
    hosts[i]->OnMessageReceived(
        ViewHostMsg_DidActivateAcceleratedCompositing(0, true));
    views[i] = new FakeRenderWidgetHostViewAura(hosts[i]);
    views[i]->InitAsChild(NULL);
    aura::client::ParentWindowWithContext(
        views[i]->GetNativeView(),
        parent_view_->GetNativeView()->GetRootWindow(),
        gfx::Rect());
    views[i]->SetSize(view_rect.size());
  }

  // Make each renderer visible and swap a frame on it. No eviction should
  // occur because all frames are visible.
  for (size_t i = 0; i < renderer_count; ++i) {
    views[i]->WasShown();
    views[i]->OnSwapCompositorFrame(
        1, MakeDelegatedFrame(1.f, frame_size, view_rect));
    EXPECT_TRUE(views[i]->frame_provider_);
  }

  // If we hide [0], then [0] should be evicted.
  views[0]->WasHidden();
  EXPECT_FALSE(views[0]->frame_provider_);

  // If we lock [0] before hiding it, then [0] should not be evicted.
  views[0]->WasShown();
  views[0]->OnSwapCompositorFrame(
        1, MakeDelegatedFrame(1.f, frame_size, view_rect));
  EXPECT_TRUE(views[0]->frame_provider_);
  views[0]->LockResources();
  views[0]->WasHidden();
  EXPECT_TRUE(views[0]->frame_provider_);

  // If we unlock [0] now, then [0] should be evicted.
  views[0]->UnlockResources();
  EXPECT_FALSE(views[0]->frame_provider_);

  for (size_t i = 0; i < renderer_count; ++i) {
    views[i]->Destroy();
    delete hosts[i];
  }
}

TEST_F(RenderWidgetHostViewAuraTest, SoftwareDPIChange) {
  gfx::Rect view_rect(100, 100);
  gfx::Size frame_size(100, 100);

  view_->InitAsChild(NULL);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_rect.size());
  view_->WasShown();

  // With a 1x DPI UI and 1x DPI Renderer.
  view_->OnSwapCompositorFrame(
      1, MakeDelegatedFrame(1.f, frame_size, gfx::Rect(frame_size)));

  // Save the frame provider.
  scoped_refptr<cc::DelegatedFrameProvider> frame_provider =
      view_->frame_provider_;

  // This frame will have the same number of physical pixels, but has a new
  // scale on it.
  view_->OnSwapCompositorFrame(
      1, MakeDelegatedFrame(2.f, frame_size, gfx::Rect(frame_size)));

  // When we get a new frame with the same frame size in physical pixels, but a
  // different scale, we should generate a new frame provider, as the final
  // result will need to be scaled differently to the screen.
  EXPECT_NE(frame_provider.get(), view_->frame_provider_.get());
}

class RenderWidgetHostViewAuraCopyRequestTest
    : public RenderWidgetHostViewAuraShutdownTest {
 public:
  RenderWidgetHostViewAuraCopyRequestTest()
      : callback_count_(0), result_(false) {}

  void CallbackMethod(const base::Closure& quit_closure, bool result) {
    result_ = result;
    callback_count_++;
    quit_closure.Run();
  }

  int callback_count_;
  bool result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAuraCopyRequestTest);
};

TEST_F(RenderWidgetHostViewAuraCopyRequestTest, DestroyedAfterCopyRequest) {
  base::RunLoop run_loop;

  gfx::Rect view_rect(100, 100);
  scoped_ptr<cc::CopyOutputRequest> request;

  view_->InitAsChild(NULL);
  aura::client::ParentWindowWithContext(
      view_->GetNativeView(),
      parent_view_->GetNativeView()->GetRootWindow(),
      gfx::Rect());
  view_->SetSize(view_rect.size());
  view_->WasShown();

  scoped_ptr<FakeFrameSubscriber> frame_subscriber(new FakeFrameSubscriber(
      view_rect.size(),
      base::Bind(&RenderWidgetHostViewAuraCopyRequestTest::CallbackMethod,
                 base::Unretained(this),
                 run_loop.QuitClosure())));

  EXPECT_EQ(0, callback_count_);
  EXPECT_FALSE(view_->last_copy_request_);

  view_->BeginFrameSubscription(
      frame_subscriber.PassAs<RenderWidgetHostViewFrameSubscriber>());
  view_->OnSwapCompositorFrame(
      1, MakeDelegatedFrame(1.f, view_rect.size(), gfx::Rect(view_rect)));

  EXPECT_EQ(0, callback_count_);
  EXPECT_TRUE(view_->last_copy_request_);
  EXPECT_TRUE(view_->last_copy_request_->has_texture_mailbox());
  request = view_->last_copy_request_.Pass();

  // There should be one subscriber texture in flight.
  EXPECT_EQ(1u, view_->active_frame_subscriber_textures_.size());

  // Send back the mailbox included in the request. There's no release callback
  // since the mailbox came from the RWHVA originally.
  request->SendTextureResult(view_rect.size(),
                             request->texture_mailbox(),
                             scoped_ptr<cc::SingleReleaseCallback>());

  // This runs until the callback happens.
  run_loop.Run();

  // The callback should succeed.
  EXPECT_EQ(0u, view_->active_frame_subscriber_textures_.size());
  EXPECT_EQ(1, callback_count_);
  EXPECT_TRUE(result_);

  view_->OnSwapCompositorFrame(
      1, MakeDelegatedFrame(1.f, view_rect.size(), gfx::Rect(view_rect)));

  EXPECT_EQ(1, callback_count_);
  request = view_->last_copy_request_.Pass();

  // There should be one subscriber texture in flight again.
  EXPECT_EQ(1u, view_->active_frame_subscriber_textures_.size());

  // Destroy the RenderWidgetHostViewAura and ImageTransportFactory.
  TearDownEnvironment();

  // Send back the mailbox included in the request. There's no release callback
  // since the mailbox came from the RWHVA originally.
  request->SendTextureResult(view_rect.size(),
                             request->texture_mailbox(),
                             scoped_ptr<cc::SingleReleaseCallback>());

  // Because the copy request callback may be holding state within it, that
  // state must handle the RWHVA and ImageTransportFactory going away before the
  // callback is called. This test passes if it does not crash as a result of
  // these things being destroyed.
  EXPECT_EQ(2, callback_count_);
  EXPECT_FALSE(result_);
}

}  // namespace content
