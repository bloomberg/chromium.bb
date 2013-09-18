// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_

#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"

#include "base/timer/timer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/rect.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget_observer.h"

class BrowserView;
class BookmarkBarView;

namespace aura {
class Window;
}

namespace gfx {
class SlideAnimation;
class Transform;
}

namespace ui {
class Layer;
class LocatedEvent;
}

namespace views {
class View;
}

class ImmersiveModeControllerAsh : public ImmersiveModeController,
                                   public content::NotificationObserver,
                                   public gfx::AnimationDelegate,
                                   public ui::EventHandler,
                                   public views::FocusChangeListener,
                                   public views::WidgetObserver,
                                   public aura::WindowObserver {
 public:
  ImmersiveModeControllerAsh();
  virtual ~ImmersiveModeControllerAsh();

  // These methods are used to increment and decrement |revealed_lock_count_|.
  // If immersive mode is enabled, a transition from 1 to 0 in
  // |revealed_lock_count_| closes the top-of-window views and a transition
  // from 0 to 1 in |revealed_lock_count_| reveals the top-of-window views.
  void LockRevealedState(AnimateReveal animate_reveal);
  void UnlockRevealedState();

  // Exits immersive fullscreen based on |native_window_|'s show state.
  void MaybeExitImmersiveFullscreen();

  // ImmersiveModeController overrides:
  virtual void Init(Delegate* delegate,
                    views::Widget* widget,
                    views::View* top_container) OVERRIDE;
  virtual void SetEnabled(bool enabled) OVERRIDE;
  virtual bool IsEnabled() const OVERRIDE;
  virtual bool ShouldHideTabIndicators() const OVERRIDE;
  virtual bool ShouldHideTopViews() const OVERRIDE;
  virtual bool IsRevealed() const OVERRIDE;
  virtual int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const OVERRIDE;
  virtual ImmersiveRevealedLock* GetRevealedLock(
      AnimateReveal animate_reveal) OVERRIDE WARN_UNUSED_RESULT;
  virtual void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds_in_screen) OVERRIDE;

  // content::NotificationObserver override:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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

  // aura::WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnAddTransientChild(aura::Window* window,
                                   aura::Window* transient) OVERRIDE;
  virtual void OnRemoveTransientChild(aura::Window* window,
                                      aura::Window* transient) OVERRIDE;

  // Testing interface.
  void SetForceHideTabIndicatorsForTest(bool force);
  void StartRevealForTest(bool hovered);
  void SetMouseHoveredForTest(bool hovered);
  void DisableAnimationsForTest();

 private:
  friend class ImmersiveModeControllerAshTest;

  enum AllowRevealWhileClosing {
    ALLOW_REVEAL_WHILE_CLOSING_YES,
    ALLOW_REVEAL_WHILE_CLOSING_NO
  };
  enum Animate {
    ANIMATE_NO,
    ANIMATE_SLOW,
    ANIMATE_FAST,
  };
  enum Layout {
    LAYOUT_YES,
    LAYOUT_NO
  };
  enum RevealState {
    CLOSED,          // Top container only showing tabstrip, y = 0.
    SLIDING_OPEN,    // All views showing, y animating from -height to 0.
    REVEALED,        // All views showing, y = 0.
    SLIDING_CLOSED,  // All views showing, y animating from 0 to -height.
  };
  enum TabIndicatorVisibility {
    TAB_INDICATORS_FORCE_HIDE,
    TAB_INDICATORS_HIDE,
    TAB_INDICATORS_SHOW
  };
  enum SwipeType {
    SWIPE_OPEN,
    SWIPE_CLOSE,
    SWIPE_NONE
  };

  // Enables or disables observers for mouse move, focus, and window restore.
  void EnableWindowObservers(bool enable);

  // Updates |top_edge_hover_timer_| based on a mouse |event|. If the mouse is
  // hovered at the top of the screen the timer is started. If the mouse moves
  // away from the top edge, or moves too much in the x direction, the timer is
  // stopped.
  void UpdateTopEdgeHoverTimer(ui::MouseEvent* event);

  // Updates |located_event_revealed_lock_| based on the current mouse state and
  // the current touch state.
  // |event| is NULL if the source event is not known.
  // |allow_reveal_while_closing| indicates whether the mouse and touch
  // are allowed to initiate a reveal while the top-of-window views are sliding
  // closed.
  void UpdateLocatedEventRevealedLock(
      ui::LocatedEvent* event,
      AllowRevealWhileClosing allow_reveal_while_closing);

  // Acquires |located_event_revealed_lock_| if it is not already held.
  void AcquireLocatedEventRevealedLock();

  // Updates |focus_revealed_lock_| based on the currently active view and the
  // currently active widget.
  void UpdateFocusRevealedLock();

  // Update |located_event_revealed_lock_| and |focus_revealed_lock_| as a
  // result of a gesture of |swipe_type|. Returns true if any locks were
  // acquired or released.
  bool UpdateRevealedLocksForSwipe(SwipeType swipe_type);

  // Updates whether fullscreen uses any chrome at all. When using minimal
  // chrome, a 'light bar' is permanently visible for the launcher and possibly
  // for the tabstrip.
  void UpdateUseMinimalChrome(Layout layout);

  // Returns the animation duration given |animate|.
  int GetAnimationDuration(Animate animate) const;

  // Temporarily reveals the top-of-window views while in immersive mode,
  // hiding them when the cursor exits the area of the top views. If |animate|
  // is not ANIMATE_NO, slides in the view, otherwise shows it immediately.
  void MaybeStartReveal(Animate animate);

  // Enables or disables layer-based painting to allow smooth animations.
  void EnablePaintToLayer(bool enable);

  // Updates the browser root view's layout including window caption controls.
  void LayoutBrowserRootView();

  // Called when the animation to slide open the top-of-window views has
  // completed.
  void OnSlideOpenAnimationCompleted(Layout layout);

  // Hides the top-of-window views if immersive mode is enabled and nothing is
  // keeping them revealed. Optionally animates.
  void MaybeEndReveal(Animate animate);

  // Called when the animation to slide out the top-of-window views has
  // completed.
  void OnSlideClosedAnimationCompleted();

  // Returns whether immersive fullscreen should be exited based on
  // |native_window_|'s show state. This handles cases where the user has
  // exited immersive fullscreen without going through
  // FullscreenController::ToggleFullscreenMode(). This is the case if the
  // user exits fullscreen via the restore button.
  bool ShouldExitImmersiveFullscreen() const;

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

  // Shrinks or expands the touch hit test by updating insets for the render
  // window depending on if top_inset is positive or negative respectively.
  // Used to ensure that touch events at the top of the screen go to the top
  // container so a slide gesture can be generated when the content window is
  // consuming all touch events sent to it.
  void SetRenderWindowTopInsetsForTouch(int top_inset);

  // Injected dependencies. Not owned.
  Delegate* delegate_;
  views::Widget* widget_;
  views::View* top_container_;

  // True if the window observers are enabled.
  bool observers_enabled_;

  // True when in immersive mode.
  bool enabled_;

  // State machine for the revealed/closed animations.
  RevealState reveal_state_;

  int revealed_lock_count_;

  // The visibility of the miniature "tab indicators" in the main browser view
  // when immersive mode is enabled and the top-of-window views are closed.
  TabIndicatorVisibility tab_indicator_visibility_;

  // Timer to track cursor being held at the top edge of the screen.
  base::OneShotTimer<ImmersiveModeController> top_edge_hover_timer_;

  // The cursor x position in screen coordinates when the cursor first hit the
  // top edge of the screen.
  int mouse_x_when_hit_top_in_screen_;

  // Tracks if the controller has seen a ET_GESTURE_SCROLL_BEGIN, without the
  // following events.
  bool gesture_begun_;

  // The current visible bounds of the find bar, in screen coordinates. This is
  // an empty rect if the find bar is not visible.
  gfx::Rect find_bar_visible_bounds_in_screen_;

  // Lock which keeps the top-of-window views revealed based on the current
  // mouse state and the current touch state. Acquiring the lock is used to
  // trigger a reveal when the user moves the mouse to the top of the screen
  // and when the user does a SWIPE_OPEN edge gesture.
  scoped_ptr<ImmersiveRevealedLock> located_event_revealed_lock_;

  // Lock which keeps the top-of-window views revealed based on the focused view
  // and the active widget. Acquiring the lock never triggers a reveal because
  // a view is not focusable till a reveal has made it visible.
  scoped_ptr<ImmersiveRevealedLock> focus_revealed_lock_;

  // Native window for the browser.
  aura::Window* native_window_;

  // The animation which controls sliding the top-of-window views in and out.
  scoped_ptr<gfx::SlideAnimation> animation_;

  // Whether the animations are disabled for testing.
  bool animations_disabled_for_test_;

  // Manages bubbles which are anchored to a child of |top_container_|.
  class BubbleManager;
  scoped_ptr<BubbleManager> bubble_manager_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<ImmersiveModeControllerAsh> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_
