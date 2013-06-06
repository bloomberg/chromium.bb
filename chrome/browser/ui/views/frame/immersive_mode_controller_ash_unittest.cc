// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include "ash/test/ash_test_base.h"
#include "chrome/browser/ui/immersive_fullscreen_configuration.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"

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
    params.bounds = gfx::Rect(0, 0, 100, 500);
    widget_->Init(params);
    widget_->Show();

    top_container_ = new views::View();
    top_container_->SetBounds(0, 0, 50, 500);
    top_container_->set_focusable(true);

    widget_->GetContentsView()->AddChildView(top_container_);

    controller_->Init(delegate_.get(), widget_, top_container_);
    controller_->DisableAnimationsForTest();
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

  // Move the mouse to |position|. |position| should be in |top_container_|
  // coordinates.
  void MoveMouse(const gfx::Point& position) {
    aura::client::GetCursorClient(CurrentContext())->EnableMouseEvents();

    gfx::Point position_in_screen = position;
    views::View::ConvertPointToScreen(top_container_, &position_in_screen);
    aura::Env::GetInstance()->set_last_mouse_location(position_in_screen);

    gfx::Point position_in_root_window = position_in_screen;
    aura::client::ScreenPositionClient* position_client =
        aura::client::GetScreenPositionClient(CurrentContext());
    position_client->ConvertPointFromScreen(CurrentContext(),
        &position_in_root_window);
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, position_in_root_window,
        position_in_root_window, ui::EF_NONE);
    ui::Event::DispatcherApi(&event).set_target(CurrentContext());
    controller()->OnMouseEvent(&event);

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
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(CurrentContext());
    if (modality == MODALITY_MOUSE)
      cursor_client->EnableMouseEvents();
    else
      cursor_client->DisableMouseEvents();

    switch(modality) {
      case MODALITY_MOUSE:
        MoveMouse(GetPosition(revealed));
        break;
      case MODALITY_TOUCH: {
        gfx::Point position = GetPosition(revealed);
        views::View::ConvertPointToScreen(top_container_, &position);
        aura::client::ScreenPositionClient* position_client =
            aura::client::GetScreenPositionClient(CurrentContext());
        position_client->ConvertPointFromScreen(CurrentContext(), &position);

        ui::TouchEvent event(ui::ET_TOUCH_PRESSED,
                             position,
                             0,
                             base::TimeDelta());
        ui::Event::DispatcherApi(&event).set_target(CurrentContext());
        controller_->OnTouchEvent(&event);
        break;
      }
      case MODALITY_GESTURE: {
        ImmersiveModeControllerAsh::SwipeType swipe_type = revealed ?
            ImmersiveModeControllerAsh::SWIPE_OPEN :
            ImmersiveModeControllerAsh::SWIPE_CLOSE;
        controller_->UpdateRevealedLocksForSwipe(swipe_type);
        break;
      }
    }
  }

  // Returns the position of an event which would reveal / unreveal the
  // top-of-window views. The returned position is in coordinates of
  // |top_container_|.
  gfx::Point GetPosition(bool revealed) const {
    return gfx::Point(0, revealed ? 0 : top_container_->height() + 100);
  }

  scoped_ptr<ImmersiveModeControllerAsh> controller_;
  scoped_ptr<MockImmersiveModeControllerDelegate> delegate_;
  views::Widget* widget_;  // Owned by the native widget.
  views::View* top_container_;  // Owned by |root_view_|.
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
  MoveMouse(gfx::Point(1, 1));
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

#endif  // defined(OS_CHROMEOS)
