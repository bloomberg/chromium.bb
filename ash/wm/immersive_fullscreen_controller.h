// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_IMMERSIVE_FULLSCREEN_CONTROLLER_H_
#define ASH_WM_IMMERSIVE_FULLSCREEN_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/immersive_revealed_lock.h"
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
    : public gfx::AnimationDelegate,
      public ui::EventHandler,
      public ::wm::TransientWindowObserver,
      public views::FocusChangeListener,
      public views::WidgetObserver,
      public ImmersiveRevealedLock::Delegate {
 public:
  static const int kMouseRevealBoundsHeight;

  // The enum is used for an enumerated histogram. New items should be only
  // added to the end.
  enum WindowType {
    WINDOW_TYPE_OTHER,
    WINDOW_TYPE_BROWSER,
    WINDOW_TYPE_HOSTED_APP,
    WINDOW_TYPE_PACKAGED_APP,
    WINDOW_TYPE_COUNT
  };

  class Delegate {
   public:
    // Called when a reveal of the top-of-window views starts.
    virtual void OnImmersiveRevealStarted() = 0;

    // Called when the top-of-window views have finished closing. This call
    // implies a visible fraction of 0. SetVisibleFraction(0) may not be called
    // prior to OnImmersiveRevealEnded().
    virtual void OnImmersiveRevealEnded() = 0;

    // Called as a result of disabling immersive fullscreen via SetEnabled().
    virtual void OnImmersiveFullscreenExited() = 0;

    // Called to update the fraction of the top-of-window views height which is
    // visible.
    virtual void SetVisibleFraction(double visible_fraction) = 0;

    // Returns a list of rects whose union makes up the top-of-window views.
    // The returned list is used for hittesting when the top-of-window views
    // are revealed. GetVisibleBoundsInScreen() must return a valid value when
    // not in immersive fullscreen for the sake of SetupForTest().
    virtual std::vector<gfx::Rect> GetVisibleBoundsInScreen() const = 0;

   protected:
    virtual ~Delegate() {}
  };

  ImmersiveFullscreenController();
  virtual ~ImmersiveFullscreenController();

  // Initializes the controller. Must be called prior to enabling immersive
  // fullscreen via SetEnabled(). |top_container| is used to keep the
  // top-of-window views revealed when a child of |top_container| has focus.
  // |top_container| does not affect which mouse and touch events keep the
  // top-of-window views revealed.
  void Init(Delegate* delegate,
            views::Widget* widget,
            views::View* top_container);

  // Enables or disables immersive fullscreen.
  // |window_type| is the type of window which is put in immersive fullscreen.
  // It is only used for histogramming.
  void SetEnabled(WindowType window_type, bool enable);

  // Returns true if |native_window_| is in immersive fullscreen.
  bool IsEnabled() const;

  // Returns true if |native_window_| is in immersive fullscreen and the
  // top-of-window views are fully or partially visible.
  bool IsRevealed() const;

  // Returns a lock which will keep the top-of-window views revealed for its
  // lifetime. Several locks can be obtained. When all of the locks are
  // destroyed, if immersive fullscreen is enabled and there is nothing else
  // keeping the top-of-window views revealed, the top-of-window views will be
  // closed. This method always returns a valid lock regardless of whether
  // immersive fullscreen is enabled. The lock's lifetime can span immersive
  // fullscreen being enabled / disabled. If acquiring the lock causes a reveal,
  // the top-of-window views will animate according to |animate_reveal|. The
  // caller takes ownership of the returned lock.
  ImmersiveRevealedLock* GetRevealedLock(
      AnimateReveal animate_reveal) WARN_UNUSED_RESULT;

  // Disables animations and moves the mouse so that it is not over the
  // top-of-window views for the sake of testing.
  void SetupForTest();

  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // views::FocusChangeObserver overrides:
  virtual void OnWillChangeFocus(views::View* focused_before,
                                 views::View* focused_now) OVERRIDE;
  virtual void OnDidChangeFocus(views::View* focused_before,
                                views::View* focused_now) OVERRIDE;

  // views::WidgetObserver overrides:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) OVERRIDE;

  // gfx::AnimationDelegate overrides:
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

  // ::wm::TransientWindowObserver overrides:
  virtual void OnTransientChildAdded(aura::Window* window,
                                     aura::Window* transient) OVERRIDE;
  virtual void OnTransientChildRemoved(aura::Window* window,
                                       aura::Window* transient) OVERRIDE;

  // ash::ImmersiveRevealedLock::Delegate overrides:
  virtual void LockRevealedState(AnimateReveal animate_reveal) OVERRIDE;
  virtual void UnlockRevealedState() OVERRIDE;

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
  enum SwipeType {
    SWIPE_OPEN,
    SWIPE_CLOSE,
    SWIPE_NONE
  };

  // Enables or disables observers for mouse, touch, focus, and activation.
  void EnableWindowObservers(bool enable);

  // Updates |top_edge_hover_timer_| based on a mouse |event|. If the mouse is
  // hovered at the top of the screen the timer is started. If the mouse moves
  // away from the top edge, or moves too much in the x direction, the timer is
  // stopped.
  void UpdateTopEdgeHoverTimer(ui::MouseEvent* event);

  // Updates |located_event_revealed_lock_| based on the current mouse state and
  // the current touch state.
  // |event| is NULL if the source event is not known.
  void UpdateLocatedEventRevealedLock(ui::LocatedEvent* event);

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
  SwipeType GetSwipeType(ui::GestureEvent* event) const;

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

  // Recreate |bubble_manager_| and start observing any bubbles anchored to a
  // child of |top_container_|.
  void RecreateBubbleManager();

  // Not owned.
  Delegate* delegate_;
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
  base::OneShotTimer<ImmersiveFullscreenController> top_edge_hover_timer_;

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
  scoped_ptr<ImmersiveRevealedLock> located_event_revealed_lock_;

  // Lock which keeps the top-of-window views revealed based on the focused view
  // and the active widget. Acquiring the lock never triggers a reveal because
  // a view is not focusable till a reveal has made it visible.
  scoped_ptr<ImmersiveRevealedLock> focus_revealed_lock_;

  // The animation which controls sliding the top-of-window views in and out.
  scoped_ptr<gfx::SlideAnimation> animation_;

  // Whether the animations are disabled for testing.
  bool animations_disabled_for_test_;

  // Manages bubbles which are anchored to a child of |top_container_|.
  class BubbleManager;
  scoped_ptr<BubbleManager> bubble_manager_;

  base::WeakPtrFactory<ImmersiveFullscreenController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveFullscreenController);
};

}  // namespace ash

#endif  // ASH_WM_IMMERSIVE_FULLSCREEN_CONTROLLER_H_
