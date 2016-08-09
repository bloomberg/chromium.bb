// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_IMMERSIVE_FULLSCREEN_CONTROLLER_H_
#define ASH_WM_IMMERSIVE_FULLSCREEN_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/common/wm/immersive/wm_immersive_fullscreen_controller.h"
#include "ash/common/wm/immersive_revealed_lock.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/transient_window_observer.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
class Rect;
class SlideAnimation;
}

namespace ui {
class LocatedEvent;
}

namespace views {
class View;
class Widget;
}

namespace ash {

class ASH_EXPORT ImmersiveFullscreenController
    : public WmImmersiveFullscreenController,
      public gfx::AnimationDelegate,
      public ui::EventHandler,
      public ::wm::TransientWindowObserver,
      public views::FocusChangeListener,
      public views::WidgetObserver,
      public ImmersiveRevealedLock::Delegate {
 public:
  static const int kMouseRevealBoundsHeight;

  ImmersiveFullscreenController();
  ~ImmersiveFullscreenController() override;

  // WmImmersiveFullscreenController overrides:
  void Init(WmImmersiveFullscreenControllerDelegate* delegate,
            views::Widget* widget,
            views::View* top_container) override;
  void SetEnabled(WindowType window_type, bool enable) override;
  bool IsEnabled() const override;
  bool IsRevealed() const override;

  // Returns a lock which will keep the top-of-window views revealed for its
  // lifetime. Several locks can be obtained. When all of the locks are
  // destroyed, if immersive fullscreen is enabled and there is nothing else
  // keeping the top-of-window views revealed, the top-of-window views will be
  // closed. This method always returns a valid lock regardless of whether
  // immersive fullscreen is enabled. The lock's lifetime can span immersive
  // fullscreen being enabled / disabled. If acquiring the lock causes a reveal,
  // the top-of-window views will animate according to |animate_reveal|. The
  // caller takes ownership of the returned lock.
  ImmersiveRevealedLock* GetRevealedLock(AnimateReveal animate_reveal)
      WARN_UNUSED_RESULT;

  // Disables animations and moves the mouse so that it is not over the
  // top-of-window views for the sake of testing.
  void SetupForTest();

  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::FocusChangeObserver overrides:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // views::WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // gfx::AnimationDelegate overrides:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // ::wm::TransientWindowObserver overrides:
  void OnTransientChildAdded(aura::Window* window,
                             aura::Window* transient) override;
  void OnTransientChildRemoved(aura::Window* window,
                               aura::Window* transient) override;

  // ash::ImmersiveRevealedLock::Delegate overrides:
  void LockRevealedState(AnimateReveal animate_reveal) override;
  void UnlockRevealedState() override;

 private:
  friend class ImmersiveFullscreenControllerTest;

  enum Animate {
    ANIMATE_NO,
    ANIMATE_SLOW,
    ANIMATE_FAST,
  };
  enum RevealState {
    CLOSED,
    SLIDING_OPEN,
    REVEALED,
    SLIDING_CLOSED,
  };
  enum SwipeType { SWIPE_OPEN, SWIPE_CLOSE, SWIPE_NONE };

  // Enables or disables observers for mouse, touch, focus, and activation.
  void EnableWindowObservers(bool enable);

  // Updates |top_edge_hover_timer_| based on a mouse |event|. If the mouse is
  // hovered at the top of the screen the timer is started. If the mouse moves
  // away from the top edge, or moves too much in the x direction, the timer is
  // stopped.
  void UpdateTopEdgeHoverTimer(const ui::MouseEvent& event);

  // Updates |located_event_revealed_lock_| based on the current mouse state and
  // the current touch state.
  // |event| is NULL if the source event is not known.
  void UpdateLocatedEventRevealedLock(const ui::LocatedEvent* event);

  // Acquires |located_event_revealed_lock_| if it is not already held.
  void AcquireLocatedEventRevealedLock();

  // Updates |focus_revealed_lock_| based on the currently active view and the
  // currently active widget.
  void UpdateFocusRevealedLock();

  // Update |located_event_revealed_lock_| and |focus_revealed_lock_| as a
  // result of a gesture of |swipe_type|. Returns true if any locks were
  // acquired or released.
  bool UpdateRevealedLocksForSwipe(SwipeType swipe_type);

  // Returns the animation duration given |animate|.
  int GetAnimationDuration(Animate animate) const;

  // Temporarily reveals the top-of-window views while in immersive mode,
  // hiding them when the cursor exits the area of the top views. If |animate|
  // is not ANIMATE_NO, slides in the view, otherwise shows it immediately.
  void MaybeStartReveal(Animate animate);

  // Called when the animation to slide open the top-of-window views has
  // completed.
  void OnSlideOpenAnimationCompleted();

  // Hides the top-of-window views if immersive mode is enabled and nothing is
  // keeping them revealed. Optionally animates.
  void MaybeEndReveal(Animate animate);

  // Called when the animation to slide out the top-of-window views has
  // completed.
  void OnSlideClosedAnimationCompleted();

  // Returns the type of swipe given |event|.
  SwipeType GetSwipeType(const ui::GestureEvent& event) const;

  // Returns true if a mouse event at |location_in_screen| should be ignored.
  // Ignored mouse events should not contribute to revealing or unrevealing the
  // top-of-window views.
  bool ShouldIgnoreMouseEventAtLocation(
      const gfx::Point& location_in_screen) const;

  // True when |location| is "near" to the top container. When the top container
  // is not closed "near" means within the displayed bounds or above it. When
  // the top container is closed "near" means either within the displayed
  // bounds, above it, or within a few pixels below it. This allow the container
  // to steal enough pixels to detect a swipe in and handles the case that there
  // is a bezel sensor above the top container.
  bool ShouldHandleGestureEvent(const gfx::Point& location) const;

  // Recreate |bubble_observer_| and start observing any bubbles anchored to a
  // child of |top_container_|.
  void RecreateBubbleObserver();

  // Not owned.
  WmImmersiveFullscreenControllerDelegate* delegate_;
  views::View* top_container_;
  views::Widget* widget_;
  aura::Window* native_window_;

  // True if the observers have been enabled.
  bool observers_enabled_;

  // True when in immersive fullscreen.
  bool enabled_;

  // State machine for the revealed/closed animations.
  RevealState reveal_state_;

  int revealed_lock_count_;

  // Timer to track cursor being held at the top edge of the screen.
  base::OneShotTimer top_edge_hover_timer_;

  // The cursor x position in screen coordinates when the cursor first hit the
  // top edge of the screen.
  int mouse_x_when_hit_top_in_screen_;

  // Tracks if the controller has seen a ET_GESTURE_SCROLL_BEGIN, without the
  // following events.
  bool gesture_begun_;

  // Lock which keeps the top-of-window views revealed based on the current
  // mouse state and the current touch state. Acquiring the lock is used to
  // trigger a reveal when the user moves the mouse to the top of the screen
  // and when the user does a SWIPE_OPEN edge gesture.
  std::unique_ptr<ImmersiveRevealedLock> located_event_revealed_lock_;

  // Lock which keeps the top-of-window views revealed based on the focused view
  // and the active widget. Acquiring the lock never triggers a reveal because
  // a view is not focusable till a reveal has made it visible.
  std::unique_ptr<ImmersiveRevealedLock> focus_revealed_lock_;

  // The animation which controls sliding the top-of-window views in and out.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // Whether the animations are disabled for testing.
  bool animations_disabled_for_test_;

  // Manages bubbles which are anchored to a child of |top_container_|.
  class BubbleObserver;
  std::unique_ptr<BubbleObserver> bubble_observer_;

  base::WeakPtrFactory<ImmersiveFullscreenController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveFullscreenController);
};

}  // namespace ash

#endif  // ASH_WM_IMMERSIVE_FULLSCREEN_CONTROLLER_H_
