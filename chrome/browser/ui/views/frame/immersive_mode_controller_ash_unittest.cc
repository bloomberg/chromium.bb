// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller_test.h"
#include "chrome/browser/ui/immersive_fullscreen_configuration.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/webview/webview.h"

// For now, immersive fullscreen is Chrome OS only.
#if defined(OS_CHROMEOS)

/////////////////////////////////////////////////////////////////////////////

class MockImmersiveModeControllerDelegate
    : public ImmersiveModeController::Delegate {
 public:
  MockImmersiveModeControllerDelegate() : immersive_style_(false) {}
  virtual ~MockImmersiveModeControllerDelegate() {}

  bool immersive_style() const { return immersive_style_; }

  // ImmersiveModeController::Delegate overrides:
  virtual BookmarkBarView* GetBookmarkBar() OVERRIDE { return NULL; }
  virtual FullscreenController* GetFullscreenController() OVERRIDE {
    return NULL;
  }
  virtual void FullscreenStateChanged() OVERRIDE {}
  virtual void SetImmersiveStyle(bool immersive) OVERRIDE {
    immersive_style_ = immersive;
  }
  virtual content::WebContents* GetWebContents() OVERRIDE {
    return NULL;
  }

 private:
  bool immersive_style_;

  DISALLOW_COPY_AND_ASSIGN(MockImmersiveModeControllerDelegate);
};

/////////////////////////////////////////////////////////////////////////////

class ImmersiveModeControllerAshTest : public ash::test::AshTestBase {
 public:
  enum Modality {
    MODALITY_MOUSE,
    MODALITY_TOUCH,
    MODALITY_GESTURE
  };

  ImmersiveModeControllerAshTest() : widget_(NULL), top_container_(NULL) {}
  virtual ~ImmersiveModeControllerAshTest() {}

  ImmersiveModeControllerAsh* controller() { return controller_.get(); }
  views::View* top_container() { return top_container_; }
  MockImmersiveModeControllerDelegate* delegate() { return delegate_.get(); }

  // Access to private data from the controller.
  bool top_edge_hover_timer_running() const {
    return controller_->top_edge_hover_timer_.IsRunning();
  }
  int mouse_x_when_hit_top() const {
    return controller_->mouse_x_when_hit_top_in_screen_;
  }

  // ash::test::AshTestBase overrides:
  virtual void SetUp() OVERRIDE {
    ash::test::AshTestBase::SetUp();

    ImmersiveFullscreenConfiguration::EnableImmersiveFullscreenForTest();
    ASSERT_TRUE(ImmersiveFullscreenConfiguration::UseImmersiveFullscreen());

    controller_.reset(new ImmersiveModeControllerAsh);
    delegate_.reset(new MockImmersiveModeControllerDelegate);

    widget_ = new views::Widget();
    views::Widget::InitParams params;
    params.context = CurrentContext();
    params.bounds = gfx::Rect(0, 0, 500, 500);
    widget_->Init(params);
    widget_->Show();

    top_container_ = new views::View();
    top_container_->SetBounds(0, 0, 500, 100);
    top_container_->set_focusable(true);

    widget_->GetContentsView()->AddChildView(top_container_);

    controller_->Init(delegate_.get(), widget_, top_container_);
    SetAnimationsDisabled(true);

    // The mouse is moved so that it is not over |top_container_| by
    // AshTestBase.
  }

  // Enable or disable the immersive mode controller's animations. When the
  // immersive mode controller's animations are disabled, some behavior is
  // slightly different. In particular, the behavior is different when there
  // is a transfer in which lock keeps the top-of-window views revealed (eg
  // bubble keeps top-of-window views revealed -> mouse keeps top-of-window
  // views revealed). It is necessary to temparily enable the immersive
  // controller's animations to get the correct behavior in tests.
  void SetAnimationsDisabled(bool disabled) {
    controller_->animations_disabled_for_test_ = disabled;
    // Force any in progress animations to finish.
    if (disabled)
      controller_->animation_->End();
  }

  // Attempt to reveal the top-of-window views via |modality|.
  // The top-of-window views can only be revealed via mouse hover or a gesture.
  void AttemptReveal(Modality modality) {
    ASSERT_NE(modality, MODALITY_TOUCH);
    AttemptRevealStateChange(true, modality);
  }

  // Attempt to unreveal the top-of-window views via |modality|. The
  // top-of-window views can be unrevealed via any modality.
  void AttemptUnreveal(Modality modality) {
    AttemptRevealStateChange(false, modality);
  }

  // Sets whether the mouse is hovered above |top_container_|.
  // SetHovered(true) moves the mouse over the |top_container_| but does not
  // move it to the top of the screen so will not initiate a reveal.
  void SetHovered(bool is_mouse_hovered) {
    MoveMouse(0, is_mouse_hovered ? 1 : top_container_->height() + 100);
  }

  // Move the mouse to the given coordinates. The coordinates should be in
  // |top_container_| coordinates.
  void MoveMouse(int x, int y) {
    gfx::Point screen_position(x, y);
    views::View::ConvertPointToScreen(top_container_, &screen_position);
    GetEventGenerator().MoveMouseTo(screen_position.x(), screen_position.y());

    // If the top edge timer started running as a result of the mouse move, run
    // the task which occurs after the timer delay. This reveals the
    // top-of-window views synchronously if the mouse is hovered at the top of
    // the screen.
    if (controller()->top_edge_hover_timer_.IsRunning()) {
      controller()->top_edge_hover_timer_.user_task().Run();
      controller()->top_edge_hover_timer_.Stop();
    }
  }

 private:
  // Attempt to change the revealed state to |revealed| via |modality|.
  void AttemptRevealStateChange(bool revealed, Modality modality) {
    // Compute the event position in |top_container_| coordinates.
    gfx::Point event_position(0, revealed ? 0 : top_container_->height() + 100);
    switch (modality) {
      case MODALITY_MOUSE: {
        MoveMouse(event_position.x(), event_position.y());
        break;
      }
      case MODALITY_TOUCH: {
        gfx::Point screen_position = event_position;
        views::View::ConvertPointToScreen(top_container_, &screen_position);

        aura::test::EventGenerator& event_generator(GetEventGenerator());
        event_generator.MoveTouch(event_position);
        event_generator.PressTouch();
        event_generator.ReleaseTouch();
        break;
      }
      case MODALITY_GESTURE: {
        aura::client::GetCursorClient(CurrentContext())->DisableMouseEvents();
        ImmersiveModeControllerAsh::SwipeType swipe_type = revealed ?
            ImmersiveModeControllerAsh::SWIPE_OPEN :
            ImmersiveModeControllerAsh::SWIPE_CLOSE;
        controller_->UpdateRevealedLocksForSwipe(swipe_type);
        break;
      }
    }
  }

  scoped_ptr<ImmersiveModeControllerAsh> controller_;
  scoped_ptr<MockImmersiveModeControllerDelegate> delegate_;
  views::Widget* widget_;  // Owned by the native widget.
  views::View* top_container_;  // Owned by |root_view_|.
  scoped_ptr<aura::test::EventGenerator> event_generator_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAshTest);
};

// Test of initial state and basic functionality.
TEST_F(ImmersiveModeControllerAshTest, ImmersiveModeControllerAsh) {
  // Initial state.
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->ShouldHideTopViews());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(delegate()->immersive_style());

  // Enabling hides the top views.
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_TRUE(controller()->ShouldHideTopViews());
  EXPECT_FALSE(controller()->ShouldHideTabIndicators());
  EXPECT_TRUE(delegate()->immersive_style());

  // Revealing shows the top views.
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  EXPECT_FALSE(controller()->ShouldHideTopViews());
  // Tabs are painting in the normal style during a reveal.
  EXPECT_FALSE(delegate()->immersive_style());

  // Disabling immersive fullscreen keeps the top views shown.
  controller()->SetEnabled(false);
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(controller()->ShouldHideTopViews());
  EXPECT_FALSE(delegate()->immersive_style());

  // Test disabling immersive fullscreen when the top views are hidden.
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_TRUE(controller()->ShouldHideTopViews());
  EXPECT_TRUE(delegate()->immersive_style());

  controller()->SetEnabled(false);
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(controller()->ShouldHideTopViews());
  EXPECT_FALSE(delegate()->immersive_style());
}

// Test mouse event processing for top-of-screen reveal triggering.
TEST_F(ImmersiveModeControllerAshTest, OnMouseEvent) {
  // Set up initial state.
  controller()->SetEnabled(true);
  ASSERT_TRUE(controller()->IsEnabled());
  ASSERT_FALSE(controller()->IsRevealed());

  aura::test::EventGenerator& event_generator(GetEventGenerator());

  gfx::Rect top_container_bounds_in_screen =
      top_container()->GetBoundsInScreen();
  // A position along the top edge of TopContainerView in screen coordinates.
  gfx::Point top_edge_pos(top_container_bounds_in_screen.x() + 100,
                          top_container_bounds_in_screen.y());

  // Mouse wheel event does nothing.
  ui::MouseEvent wheel(
      ui::ET_MOUSEWHEEL, top_edge_pos, top_edge_pos, ui::EF_NONE);
  event_generator.Dispatch(&wheel);
  EXPECT_FALSE(top_edge_hover_timer_running());

  // Move to top edge of screen starts hover timer running. We cannot use
  // MoveMouse() because MoveMouse() stops the timer if it started running.
  event_generator.MoveMouseTo(top_edge_pos);
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_EQ(top_edge_pos.x(), mouse_x_when_hit_top());

  // Moving |ImmersiveModeControllerAsh::kMouseRevealBoundsHeight| down from
  // the top edge stops it.
  event_generator.MoveMouseBy(0, 3);
  EXPECT_FALSE(top_edge_hover_timer_running());

  // Moving back to the top starts the timer again.
  event_generator.MoveMouseTo(top_edge_pos);
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_EQ(top_edge_pos.x(), mouse_x_when_hit_top());

  // Slight move to the right keeps the timer running for the same hit point.
  event_generator.MoveMouseBy(1, 0);
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_EQ(top_edge_pos.x(), mouse_x_when_hit_top());

  // Moving back to the left also keeps the timer running.
  event_generator.MoveMouseBy(-1, 0);
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_EQ(top_edge_pos.x(), mouse_x_when_hit_top());

  // Large move right restarts the timer (so it is still running) and considers
  // this a new hit at the top.
  event_generator.MoveMouseTo(top_edge_pos.x() + 100, top_edge_pos.y());
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_EQ(top_edge_pos.x() + 100, mouse_x_when_hit_top());

  // Moving off the top edge horizontally stops the timer.
  EXPECT_GT(CurrentContext()->bounds().width(), top_container()->width());
  event_generator.MoveMouseTo(top_container_bounds_in_screen.right(),
                              top_container_bounds_in_screen.y());
  EXPECT_FALSE(top_edge_hover_timer_running());

  // Once revealed, a move just a little below the top container doesn't end a
  // reveal.
  AttemptReveal(MODALITY_MOUSE);
  event_generator.MoveMouseTo(top_container_bounds_in_screen.x(),
                              top_container_bounds_in_screen.bottom() + 1);
  EXPECT_TRUE(controller()->IsRevealed());

  // Once revealed, clicking just below the top container ends the reveal.
  event_generator.ClickLeftButton();
  EXPECT_FALSE(controller()->IsRevealed());

  // Moving a lot below the top container ends a reveal.
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  event_generator.MoveMouseTo(top_container_bounds_in_screen.x(),
                              top_container_bounds_in_screen.bottom() + 50);
  EXPECT_FALSE(controller()->IsRevealed());

  // The mouse position cannot cause a reveal when TopContainerView's widget
  // has capture.
  views::Widget* widget = top_container()->GetWidget();
  widget->SetCapture(top_container());
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_FALSE(controller()->IsRevealed());
  widget->ReleaseCapture();

  // The mouse position cannot end the reveal while TopContainerView's widget
  // has capture.
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  widget->SetCapture(top_container());
  event_generator.MoveMouseTo(top_container_bounds_in_screen.x(),
                              top_container_bounds_in_screen.bottom() + 51);
  EXPECT_TRUE(controller()->IsRevealed());

  // Releasing capture should end the reveal.
  widget->ReleaseCapture();
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test mouse event processing for top-of-screen reveal triggering when the user
// has a vertical display layout (primary display above/below secondary display)
// and the immersive fullscreen window is on the bottom display.
TEST_F(ImmersiveModeControllerAshTest, MouseEventsVerticalDisplayLayout) {
  if (!SupportsMultipleDisplays())
    return;

  // Set up initial state.
  UpdateDisplay("800x600,800x600");
  ash::DisplayLayout display_layout(ash::DisplayLayout::TOP, 0);
  ash::Shell::GetInstance()->display_manager()->SetLayoutForCurrentDisplays(
      display_layout);

  controller()->SetEnabled(true);
  ASSERT_TRUE(controller()->IsEnabled());
  ASSERT_FALSE(controller()->IsRevealed());

  ash::Shell::RootWindowList root_windows = ash::Shell::GetAllRootWindows();
  ASSERT_EQ(root_windows[0],
            top_container()->GetWidget()->GetNativeWindow()->GetRootWindow());

  gfx::Rect primary_root_window_bounds_in_screen =
      root_windows[0]->GetBoundsInScreen();
  // Do not set |x| to the root window's x position because the display's
  // corners have special behavior.
  int x = primary_root_window_bounds_in_screen.x() + 10;
  // The y position of the top edge of the primary display.
  int y_top_edge = primary_root_window_bounds_in_screen.y();

  aura::test::EventGenerator& event_generator(GetEventGenerator());

  // Moving right below the top edge starts the hover timer running. We
  // cannot use MoveMouse() because MoveMouse() stops the timer if it started
  // running.
  event_generator.MoveMouseTo(x, y_top_edge + 1);
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_EQ(y_top_edge + 1,
            aura::Env::GetInstance()->last_mouse_location().y());

  // The timer should continue running if the user moves the mouse to the top
  // edge even though the mouse is warped to the secondary display.
  event_generator.MoveMouseTo(x, y_top_edge);
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_NE(y_top_edge,
            aura::Env::GetInstance()->last_mouse_location().y());

  // The timer should continue running if the user overshoots the top edge
  // a bit.
  event_generator.MoveMouseTo(x, y_top_edge - 2);
  EXPECT_TRUE(top_edge_hover_timer_running());

  // The timer should stop running if the user overshoots the top edge by
  // a lot.
  event_generator.MoveMouseTo(x, y_top_edge - 20);
  EXPECT_FALSE(top_edge_hover_timer_running());

  // The timer should not start if the user moves the mouse to the bottom of the
  // secondary display without crossing the top edge first.
  event_generator.MoveMouseTo(x, y_top_edge - 2);

  // Reveal the top-of-window views by overshooting the top edge slightly.
  event_generator.MoveMouseTo(x, y_top_edge + 1);
  // MoveMouse() runs the timer task.
  MoveMouse(x, y_top_edge - 2);
  EXPECT_TRUE(controller()->IsRevealed());

  // The top-of-window views should stay revealed if the user moves the mouse
  // around in the bottom region of the secondary display.
  event_generator.MoveMouseTo(x + 10, y_top_edge - 3);
  EXPECT_TRUE(controller()->IsRevealed());

  // The top-of-window views should hide if the user moves the mouse away from
  // the bottom region of the secondary display.
  event_generator.MoveMouseTo(x, y_top_edge - 20);
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test that hovering the mouse over the find bar does not end a reveal.
TEST_F(ImmersiveModeControllerAshTest, FindBar) {
  // Set up initial state.
  controller()->SetEnabled(true);
  ASSERT_TRUE(controller()->IsEnabled());
  ASSERT_FALSE(controller()->IsRevealed());

  // Compute the find bar bounds relative to TopContainerView. The find
  // bar is aligned with the bottom right of the TopContainerView.
  gfx::Rect find_bar_bounds(top_container()->bounds().right() - 100,
                            top_container()->bounds().bottom(),
                            100,
                            50);

  gfx::Point find_bar_position_in_screen = find_bar_bounds.origin();
  views::View::ConvertPointToScreen(top_container(),
      &find_bar_position_in_screen);
  gfx::Rect find_bar_bounds_in_screen(find_bar_position_in_screen,
      find_bar_bounds.size());
  controller()->OnFindBarVisibleBoundsChanged(find_bar_bounds_in_screen);

  // Moving the mouse over the find bar does not end the reveal.
  gfx::Point over_find_bar(find_bar_bounds.x() + 25, find_bar_bounds.y() + 25);
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  MoveMouse(over_find_bar.x(), over_find_bar.y());
  EXPECT_TRUE(controller()->IsRevealed());

  // Moving the mouse off of the find bar horizontally ends the reveal.
  MoveMouse(find_bar_bounds.x() - 25, find_bar_bounds.y() + 25);
  EXPECT_FALSE(controller()->IsRevealed());

  // Moving the mouse off of the find bar vertically ends the reveal.
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  MoveMouse(find_bar_bounds.x() + 25, find_bar_bounds.bottom() + 25);

  // Similar to the TopContainerView, moving the mouse slightly off vertically
  // of the find bar does not end the reveal.
  AttemptReveal(MODALITY_MOUSE);
  MoveMouse(find_bar_bounds.x() + 25, find_bar_bounds.bottom() + 1);
  EXPECT_TRUE(controller()->IsRevealed());

  // Similar to the TopContainerView, clicking the mouse even slightly off of
  // the find bar ends the reveal.
  GetEventGenerator().ClickLeftButton();
  EXPECT_FALSE(controller()->IsRevealed());

  // Set the find bar bounds to empty. Hovering over the position previously
  // occupied by the find bar, |over_find_bar|, should end the reveal.
  controller()->OnFindBarVisibleBoundsChanged(gfx::Rect());
  AttemptReveal(MODALITY_MOUSE);
  MoveMouse(over_find_bar.x(), over_find_bar.y());
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test revealing the top-of-window views using one modality and ending
// the reveal via another. For instance, initiating the reveal via a SWIPE_OPEN
// edge gesture, switching to using the mouse and ending the reveal by moving
// the mouse off of the top-of-window views.
TEST_F(ImmersiveModeControllerAshTest, DifferentModalityEnterExit) {
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  // Initiate reveal via gesture, end reveal via mouse.
  AttemptReveal(MODALITY_GESTURE);
  EXPECT_TRUE(controller()->IsRevealed());
  MoveMouse(1, 1);
  EXPECT_TRUE(controller()->IsRevealed());
  AttemptUnreveal(MODALITY_MOUSE);
  EXPECT_FALSE(controller()->IsRevealed());

  // Initiate reveal via gesture, end reveal via touch.
  AttemptReveal(MODALITY_GESTURE);
  EXPECT_TRUE(controller()->IsRevealed());
  AttemptUnreveal(MODALITY_TOUCH);
  EXPECT_FALSE(controller()->IsRevealed());

  // Initiate reveal via mouse, end reveal via gesture.
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  AttemptUnreveal(MODALITY_GESTURE);
  EXPECT_FALSE(controller()->IsRevealed());

  // Initiate reveal via mouse, end reveal via touch.
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  AttemptUnreveal(MODALITY_TOUCH);
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test when the SWIPE_CLOSE edge gesture closes the top-of-window views.
TEST_F(ImmersiveModeControllerAshTest, EndRevealViaGesture) {
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  // A gesture should be able to close the top-of-window views when
  // top-of-window views have focus.
  AttemptReveal(MODALITY_MOUSE);
  top_container()->RequestFocus();
  EXPECT_TRUE(controller()->IsRevealed());
  AttemptUnreveal(MODALITY_GESTURE);
  EXPECT_FALSE(controller()->IsRevealed());
  top_container()->GetFocusManager()->ClearFocus();

  // If some other code is holding onto a lock, a gesture should not be able to
  // end the reveal.
  AttemptReveal(MODALITY_MOUSE);
  scoped_ptr<ImmersiveRevealedLock> lock(controller()->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(controller()->IsRevealed());
  AttemptUnreveal(MODALITY_GESTURE);
  EXPECT_TRUE(controller()->IsRevealed());
  lock.reset();
  EXPECT_FALSE(controller()->IsRevealed());
}

// Do not test under windows because focus testing is not reliable on
// Windows. (crbug.com/79493)
#if !defined(OS_WIN)

// Test how focus and activation affects whether the top-of-window views are
// revealed.
TEST_F(ImmersiveModeControllerAshTest, Focus) {
  // Add views to the view hierarchy which we will focus and unfocus during the
  // test.
  views::View* child_view = new views::View();
  child_view->SetBounds(0, 0, 10, 10);
  child_view->set_focusable(true);
  top_container()->AddChildView(child_view);
  views::View* unrelated_view = new views::View();
  unrelated_view->SetBounds(0, 100, 10, 10);
  unrelated_view->set_focusable(true);
  top_container()->parent()->AddChildView(unrelated_view);
  views::FocusManager* focus_manager =
      top_container()->GetWidget()->GetFocusManager();

  controller()->SetEnabled(true);

  // 1) Test that the top-of-window views stay revealed as long as either a
  // |child_view| has focus or the mouse is hovered above the top-of-window
  // views.
  AttemptReveal(MODALITY_MOUSE);
  child_view->RequestFocus();
  focus_manager->ClearFocus();
  EXPECT_TRUE(controller()->IsRevealed());
  child_view->RequestFocus();
  SetHovered(false);
  EXPECT_TRUE(controller()->IsRevealed());
  focus_manager->ClearFocus();
  EXPECT_FALSE(controller()->IsRevealed());

  // 2) Test that focusing |unrelated_view| hides the top-of-window views.
  // Note: In this test we can cheat and trigger a reveal via focus because
  // the top container does not hide when the top-of-window views are not
  // revealed.
  child_view->RequestFocus();
  EXPECT_TRUE(controller()->IsRevealed());
  unrelated_view->RequestFocus();
  EXPECT_FALSE(controller()->IsRevealed());

  // 3) Test that a loss of focus of |child_view| to |unrelated_view|
  // while immersive mode is disabled is properly registered.
  child_view->RequestFocus();
  EXPECT_TRUE(controller()->IsRevealed());
  controller()->SetEnabled(false);
  EXPECT_FALSE(controller()->IsRevealed());
  unrelated_view->RequestFocus();
  controller()->SetEnabled(true);
  EXPECT_FALSE(controller()->IsRevealed());

  // Repeat test but with a revealed lock acquired when immersive mode is
  // disabled because the code path is different.
  child_view->RequestFocus();
  EXPECT_TRUE(controller()->IsRevealed());
  controller()->SetEnabled(false);
  scoped_ptr<ImmersiveRevealedLock> lock(controller()->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_FALSE(controller()->IsRevealed());
  unrelated_view->RequestFocus();
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->IsRevealed());
  lock.reset();
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test how activation affects whether the top-of-window views are revealed.
// The behavior when a bubble is activated is tested in
// ImmersiveModeControllerAshTest.Bubbles.
TEST_F(ImmersiveModeControllerAshTest, Activation) {
  views::Widget* top_container_widget = top_container()->GetWidget();

  controller()->SetEnabled(true);
  ASSERT_FALSE(controller()->IsRevealed());

  // 1) Test that a transient window which is not a bubble does not trigger a
  // reveal but does keep the top-of-window views revealed if they are already
  // revealed.
  views::Widget::InitParams transient_params;
  transient_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  transient_params.parent = top_container_widget->GetNativeView();
  transient_params.bounds = gfx::Rect(0, 0, 100, 100);
  scoped_ptr<views::Widget> transient_widget(new views::Widget());
  transient_widget->Init(transient_params);
  transient_widget->Show();

  EXPECT_FALSE(controller()->IsRevealed());
  top_container_widget->Activate();
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  transient_widget->Activate();
  SetHovered(false);
  EXPECT_TRUE(controller()->IsRevealed());
  transient_widget.reset();
  EXPECT_FALSE(controller()->IsRevealed());

  // 2) Test that activating a non-transient window ends the reveal if any.
  views::Widget::InitParams non_transient_params;
  non_transient_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  non_transient_params.context = top_container_widget->GetNativeView();
  non_transient_params.bounds = gfx::Rect(0, 0, 100, 100);
  scoped_ptr<views::Widget> non_transient_widget(new views::Widget());
  non_transient_widget->Init(non_transient_params);
  non_transient_widget->Show();

  EXPECT_FALSE(controller()->IsRevealed());
  top_container_widget->Activate();
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  non_transient_widget->Activate();
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test how bubbles affect whether the top-of-window views are revealed.
TEST_F(ImmersiveModeControllerAshTest, Bubbles) {
  scoped_ptr<ImmersiveRevealedLock> revealed_lock;
  views::Widget* top_container_widget = top_container()->GetWidget();

  // Add views to the view hierarchy to which we will anchor bubbles.
  views::View* child_view = new views::View();
  child_view->SetBounds(0, 0, 10, 10);
  top_container()->AddChildView(child_view);
  views::View* unrelated_view = new views::View();
  unrelated_view->SetBounds(0, 100, 10, 10);
  top_container()->parent()->AddChildView(unrelated_view);

  controller()->SetEnabled(true);
  ASSERT_FALSE(controller()->IsRevealed());

  // 1) Test that a bubble anchored to a child of the top container triggers
  // a reveal and keeps the top-of-window views revealed for the duration of
  // its visibility.
  views::Widget* bubble_widget1(views::BubbleDelegateView::CreateBubble(
      new views::BubbleDelegateView(child_view, views::BubbleBorder::NONE)));
  bubble_widget1->Show();
  EXPECT_TRUE(controller()->IsRevealed());

  // Activating |top_container_widget| will close |bubble_widget1|.
  top_container_widget->Activate();
  AttemptReveal(MODALITY_MOUSE);
  revealed_lock.reset(controller()->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(controller()->IsRevealed());

  views::Widget* bubble_widget2 = views::BubbleDelegateView::CreateBubble(
      new views::BubbleDelegateView(child_view, views::BubbleBorder::NONE));
  bubble_widget2->Show();
  EXPECT_TRUE(controller()->IsRevealed());
  revealed_lock.reset();
  SetHovered(false);
  EXPECT_TRUE(controller()->IsRevealed());
  bubble_widget2->Close();
  EXPECT_FALSE(controller()->IsRevealed());

  // 2) Test that transitioning from keeping the top-of-window views revealed
  // because of a bubble to keeping the top-of-window views revealed because of
  // mouse hover by activating |top_container_widget| works.
  views::Widget* bubble_widget3 = views::BubbleDelegateView::CreateBubble(
      new views::BubbleDelegateView(child_view, views::BubbleBorder::NONE));
  bubble_widget3->Show();
  SetHovered(true);
  EXPECT_TRUE(controller()->IsRevealed());

  SetAnimationsDisabled(false);
  // Activating |top_container_widget| will close |bubble_widget3|.
  top_container_widget->Activate();
  SetAnimationsDisabled(true);
  EXPECT_TRUE(controller()->IsRevealed());

  // 3) Test that the top-of-window views stay revealed as long as at least one
  // bubble anchored to a child of the top container is visible.
  SetHovered(false);
  EXPECT_FALSE(controller()->IsRevealed());

  views::BubbleDelegateView* bubble_delegate4(new views::BubbleDelegateView(
      child_view, views::BubbleBorder::NONE));
  bubble_delegate4->set_use_focusless(true);
  views::Widget* bubble_widget4(views::BubbleDelegateView::CreateBubble(
      bubble_delegate4));
  bubble_widget4->Show();

  views::BubbleDelegateView* bubble_delegate5(new views::BubbleDelegateView(
      child_view, views::BubbleBorder::NONE));
  bubble_delegate5->set_use_focusless(true);
  views::Widget* bubble_widget5(views::BubbleDelegateView::CreateBubble(
      bubble_delegate5));
  bubble_widget5->Show();

  EXPECT_TRUE(controller()->IsRevealed());
  bubble_widget4->Hide();
  EXPECT_TRUE(controller()->IsRevealed());
  bubble_widget5->Hide();
  EXPECT_FALSE(controller()->IsRevealed());
  bubble_widget5->Show();
  EXPECT_TRUE(controller()->IsRevealed());

  // 4) Test that visibility changes which occur while immersive fullscreen is
  // disabled are handled upon reenabling immersive fullscreen.
  controller()->SetEnabled(false);
  bubble_widget5->Hide();
  controller()->SetEnabled(true);
  EXPECT_FALSE(controller()->IsRevealed());

  // We do not need |bubble_widget4| or |bubble_widget5| anymore, close them.
  bubble_widget4->Close();
  bubble_widget5->Close();

  // 5) Test that a bubble added while immersive fullscreen is disabled is
  // handled upon reenabling immersive fullscreen.
  controller()->SetEnabled(false);

  views::Widget* bubble_widget6 = views::BubbleDelegateView::CreateBubble(
      new views::BubbleDelegateView(child_view, views::BubbleBorder::NONE));
  bubble_widget6->Show();

  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->IsRevealed());

  bubble_widget6->Close();

  // 6) Test that a bubble which is not anchored to a child of the
  // TopContainerView does not trigger a reveal or keep the
  // top-of-window views revealed if they are already revealed.
  views::Widget* bubble_widget7 = views::BubbleDelegateView::CreateBubble(
      new views::BubbleDelegateView(unrelated_view, views::BubbleBorder::NONE));
  bubble_widget7->Show();
  EXPECT_FALSE(controller()->IsRevealed());

  // Activating |top_container_widget| will close |bubble_widget6|.
  top_container_widget->Activate();
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());

  views::Widget* bubble_widget8 = views::BubbleDelegateView::CreateBubble(
      new views::BubbleDelegateView(unrelated_view, views::BubbleBorder::NONE));
  bubble_widget8->Show();
  SetHovered(false);
  EXPECT_FALSE(controller()->IsRevealed());
  bubble_widget8->Close();
}

#endif  // defined(OS_WIN)

class ImmersiveModeControllerAshTestWithBrowserView
    : public TestWithBrowserView {
 public:
  ImmersiveModeControllerAshTestWithBrowserView() {}
  virtual ~ImmersiveModeControllerAshTestWithBrowserView() {}

  // TestWithBrowserView override:
  virtual void SetUp() OVERRIDE {
    TestWithBrowserView::SetUp();

    browser()->window()->Show();

    ImmersiveFullscreenConfiguration::EnableImmersiveFullscreenForTest();
    ASSERT_TRUE(ImmersiveFullscreenConfiguration::UseImmersiveFullscreen());

    controller_ = static_cast<ImmersiveModeControllerAsh*>(
        browser_view()->immersive_mode_controller());
    controller_->DisableAnimationsForTest();

    // Move the mouse so that it is not over the top-of-window views. The mouse
    // position matters because entering immersive fullscreen causes synthesized
    // mouse moves. (If the mouse is at the very top of the screen when entering
    // immersive fullscreen, the top-of-window views will hide, then reveal as
    // a result of the synthesized mouse moves).
    controller()->SetMouseHoveredForTest(false);
  }

  // Returns the bounds of |view| in widget coordinates.
  gfx::Rect GetBoundsInWidget(views::View* view) {
    return view->ConvertRectToWidget(view->GetLocalBounds());
  }

  ImmersiveModeControllerAsh* controller() { return controller_; }

 private:
  // Not owned.
  ImmersiveModeControllerAsh* controller_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAshTestWithBrowserView);
};

// Test the layout and visibility of the tabstrip, toolbar and TopContainerView
// in immersive fullscreen.
TEST_F(ImmersiveModeControllerAshTestWithBrowserView, Layout) {
  AddTab(browser(), GURL("about:blank"));

  TabStrip* tabstrip = browser_view()->tabstrip();
  ToolbarView* toolbar = browser_view()->toolbar();
  views::WebView* contents_web_view =
      browser_view()->GetContentsWebViewForTest();

  // Immersive fullscreen starts out disabled.
  ASSERT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  ASSERT_FALSE(controller()->IsEnabled());

  // By default, the tabstrip and toolbar should be visible.
  EXPECT_TRUE(tabstrip->visible());
  EXPECT_TRUE(toolbar->visible());

  // Enter immersive fullscreen.
  {
    // NOTIFICATION_FULLSCREEN_CHANGED is sent asynchronously.
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    chrome::ToggleFullscreenMode(browser());
    waiter->Wait();
  }
  EXPECT_TRUE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  // Entering immersive fullscreen should make the tab strip use the immersive
  // style and hide the toolbar.
  EXPECT_TRUE(tabstrip->visible());
  EXPECT_TRUE(tabstrip->IsImmersiveStyle());
  EXPECT_FALSE(toolbar->visible());

  // The tab indicators should be flush with the top of the widget.
  EXPECT_EQ(0, GetBoundsInWidget(tabstrip).y());

  // The web contents should be immediately below the tab indicators.
  EXPECT_EQ(Tab::GetImmersiveHeight(),
            GetBoundsInWidget(contents_web_view).y());

  // Revealing the top-of-window views should set the tab strip back to the
  // normal style and show the toolbar.
  controller()->StartRevealForTest(true);
  EXPECT_TRUE(controller()->IsRevealed());
  EXPECT_TRUE(tabstrip->visible());
  EXPECT_FALSE(tabstrip->IsImmersiveStyle());
  EXPECT_TRUE(toolbar->visible());

  // The TopContainerView should be flush with the top edge of the widget. If
  // it is not flush with the top edge the immersive reveal animation looks
  // wonky.
  EXPECT_EQ(0, GetBoundsInWidget(browser_view()->top_container()).y());

  // The web contents should be at the same y position as they were when the
  // top-of-window views were hidden.
  EXPECT_EQ(Tab::GetImmersiveHeight(),
            GetBoundsInWidget(contents_web_view).y());

  // Repeat the test for when in both immersive fullscreen and tab fullscreen.
  {
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    browser()->fullscreen_controller()->ToggleFullscreenModeForTab(
        contents_web_view->GetWebContents(), true);
    waiter->Wait();
  }
  // Hide and reveal the top-of-window views so that they get relain out.
  controller()->SetMouseHoveredForTest(false);
  controller()->StartRevealForTest(true);

  // The tab strip and toolbar should still be visible and the TopContainerView
  // should still be flush with the top edge of the widget.
  EXPECT_TRUE(controller()->IsRevealed());
  EXPECT_TRUE(tabstrip->visible());
  EXPECT_FALSE(tabstrip->IsImmersiveStyle());
  EXPECT_TRUE(toolbar->visible());
  EXPECT_EQ(0, GetBoundsInWidget(browser_view()->top_container()).y());

  // The web contents should be flush with the top edge of the widget when in
  // both immersive and tab fullscreen.
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // Hide the top-of-window views. Both the tab strip and the toolbar should
  // hide when in both immersive and tab fullscreen.
  controller()->SetMouseHoveredForTest(false);
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(tabstrip->visible());
  EXPECT_FALSE(toolbar->visible());

  // The web contents should still be flush with the edge of the widget.
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // Exiting both immersive and tab fullscreen should show the tab strip and
  // toolbar.
  {
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    chrome::ToggleFullscreenMode(browser());
    waiter->Wait();
  }
  EXPECT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_TRUE(tabstrip->visible());
  EXPECT_FALSE(tabstrip->IsImmersiveStyle());
  EXPECT_TRUE(toolbar->visible());
}

// Test that the browser commands which are usually disabled in fullscreen are
// are enabled in immersive fullscreen.
TEST_F(ImmersiveModeControllerAshTestWithBrowserView, EnabledCommands) {
  ASSERT_FALSE(controller()->IsEnabled());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));

  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));
}

// Test that restoring a window properly exits immersive fullscreen.
TEST_F(ImmersiveModeControllerAshTestWithBrowserView, ExitUponRestore) {
  ASSERT_FALSE(controller()->IsEnabled());
  chrome::ToggleFullscreenMode(browser());
  controller()->StartRevealForTest(true);
  ASSERT_TRUE(controller()->IsEnabled());
  ASSERT_TRUE(controller()->IsRevealed());
  ASSERT_TRUE(browser_view()->GetWidget()->IsFullscreen());

  browser_view()->GetWidget()->Restore();
  // Exiting immersive fullscreen occurs as a result of a task posted to the
  // message loop.
  content::RunAllPendingInMessageLoop();

  EXPECT_FALSE(controller()->IsEnabled());
}

#endif  // defined(OS_CHROMEOS)
