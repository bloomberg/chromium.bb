// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/test/mock_render_process_host.h"
#include "content/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/single_monitor_manager.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_stacking_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"

class MockRenderWidgetHostDelegate : public content::RenderWidgetHostDelegate {
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
    aura::Env::GetInstance()->SetMonitorManager(new aura::SingleMonitorManager);
    root_window_.reset(
        aura::MonitorManager::CreateRootWindowForPrimaryMonitor());
    gfx::Screen::SetInstance(new aura::TestScreen(root_window_.get()));
    test_stacking_client_.reset(
        new aura::test::TestStackingClient(root_window_.get()));

    browser_context_.reset(new TestBrowserContext);
    content::MockRenderProcessHost* process_host =
        new content::MockRenderProcessHost(browser_context_.get());
    widget_host_ = new content::RenderWidgetHostImpl(
        &delegate_, process_host, MSG_ROUTING_NONE);
    view_ = static_cast<RenderWidgetHostViewAura*>(
        content::RenderWidgetHostView::CreateViewForWidget(widget_host_));
  }

  virtual void TearDown() {
    if (view_)
      view_->Destroy();
    delete widget_host_;
    message_loop_.DeleteSoon(FROM_HERE, browser_context_.release());
    message_loop_.RunAllPending();
  }

 protected:
  MessageLoopForUI message_loop_;
  scoped_ptr<aura::RootWindow> root_window_;
  scoped_ptr<aura::test::TestStackingClient> test_stacking_client_;
  scoped_ptr<content::BrowserContext> browser_context_;
  MockRenderWidgetHostDelegate delegate_;

  // Tests should set these to NULL if they've already triggered their
  // destruction.
  content::RenderWidgetHostImpl* widget_host_;
  RenderWidgetHostViewAura* view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAuraTest);
};

// Checks that a fullscreen view has the correct show-state and receives the
// focus.
TEST_F(RenderWidgetHostViewAuraTest, FocusFullscreen) {
  view_->InitAsFullscreen(NULL);
  aura::Window* window = view_->GetNativeView();
  ASSERT_TRUE(window != NULL);
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN,
            window->GetProperty(aura::client::kShowStateKey));

  // Check that we requested and received the focus.
  EXPECT_TRUE(window->HasFocus());

  // Check that we'll also say it's okay to activate the window when there's an
  // ActivationClient defined.
  EXPECT_TRUE(view_->ShouldActivate(NULL));
}

// Checks that a fullscreen view is destroyed when it loses the focus.
TEST_F(RenderWidgetHostViewAuraTest, DestroyFullscreenOnBlur) {
  view_->InitAsFullscreen(NULL);
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
