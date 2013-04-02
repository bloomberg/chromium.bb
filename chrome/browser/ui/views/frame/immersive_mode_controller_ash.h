// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_

#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"

#include "base/timer.h"
#include "ui/base/events/event_handler.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget_observer.h"

class BrowserView;

namespace views {
class View;
}

class ImmersiveModeControllerAsh : public ImmersiveModeController,
                                   public ui::EventHandler,
                                   public views::FocusChangeListener,
                                   public views::WidgetObserver {
 public:
  ImmersiveModeControllerAsh();
  virtual ~ImmersiveModeControllerAsh();

  // These methods are used to increment and decrement |revealed_lock_count_|.
  // If immersive mode is enabled, a transition from 1 to 0 in
  // |revealed_lock_count_| closes the top-of-window views and a transition
  // from 0 to 1 in |revealed_lock_count_| reveals the top-of-window views.
  void LockRevealedState();
  void UnlockRevealedState();

  // ImmersiveModeController overrides:
  virtual void Init(BrowserView* browser_view) OVERRIDE;
  virtual void SetEnabled(bool enabled) OVERRIDE;
  virtual bool IsEnabled() const OVERRIDE;
  virtual bool ShouldHideTabIndicators() const OVERRIDE;
  virtual bool ShouldHideTopViews() const OVERRIDE;
  virtual bool IsRevealed() const OVERRIDE;
  virtual void MaybeStackViewAtTop() OVERRIDE;
  virtual void MaybeStartReveal() OVERRIDE;
  virtual void CancelReveal() OVERRIDE;
  virtual ImmersiveModeController::RevealedLock*
      GetRevealedLock() OVERRIDE WARN_UNUSED_RESULT;

  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;

  // views::FocusChangeObserver overrides:
  virtual void OnWillChangeFocus(views::View* focused_before,
                                 views::View* focused_now) OVERRIDE;
  virtual void OnDidChangeFocus(views::View* focused_before,
                                views::View* focused_now) OVERRIDE;

  // views::WidgetObserver overrides:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) OVERRIDE;

  // Testing interface.
  void SetHideTabIndicatorsForTest(bool hide);
  void StartRevealForTest(bool hovered);
  void SetMouseHoveredForTest(bool hovered);

 private:
  enum Animate {
    ANIMATE_NO,
    ANIMATE_SLOW,
    ANIMATE_FAST,
  };
  enum RevealState {
    CLOSED,          // Top container only showing tabstrip, y = 0.
    SLIDING_OPEN,    // All views showing, y animating from -height to 0.
    REVEALED,        // All views showing, y = 0.
    SLIDING_CLOSED,  // All views showing, y animating from 0 to -height.
  };

  // Enables or disables observers for mouse move, focus, and window restore.
  void EnableWindowObservers(bool enable);

  // Update |mouse_revealed_lock_| based on the current mouse state and the
  // currently active widget.
  // |maybe_drag| is true if the user may be in the middle of a drag.
  void UpdateMouseRevealedLock(bool maybe_drag);

  // Acquire the mouse revealed lock if it is not already held.
  void AcquireMouseRevealedLock();

  // Update |focus_revealed_lock_| based on the currently active view and the
  // currently active widget.
  void UpdateFocusRevealedLock();

  // Returns the animation duration given |animate|.
  int GetAnimationDuration(Animate animate) const;

  // Temporarily reveals the top-of-window views while in immersive mode,
  // hiding them when the cursor exits the area of the top views. If |animate|
  // is not ANIMATE_NO, slides in the view, otherwise shows it immediately.
  void StartReveal(Animate animate);

  // Updates layout for |browser_view_| including window caption controls and
  // tab strip style |immersive_style|.
  void LayoutBrowserView(bool immersive_style);

  // Slides open the reveal view at the top of the screen.
  void AnimateSlideOpen(int duration_ms);
  void OnSlideOpenAnimationCompleted();

  // Hides the top-of-window views if immersive mode is enabled and nothing is
  // keeping them revealed. Optionally animates.
  void MaybeEndReveal(Animate animate);

  // Hides the top-of-window views. Optionally animates.
  void EndReveal(Animate animate);

  // Slide out the reveal view.
  void AnimateSlideClosed(int duration_ms);
  void OnSlideClosedAnimationCompleted();

  // Browser view holding the views to be shown and hidden. Not owned.
  BrowserView* browser_view_;

  // True when in immersive mode.
  bool enabled_;

  // State machine for the revealed/closed animations.
  RevealState reveal_state_;

  int revealed_lock_count_;

  // True if the miniature "tab indicators" should be hidden in the main browser
  // view when immersive mode is enabled.
  bool hide_tab_indicators_;

  // Timer to track cursor being held at the top.
  base::OneShotTimer<ImmersiveModeController> top_timer_;

  // Lock which keeps the top-of-window views revealed based on the current
  // mouse state.
  scoped_ptr<RevealedLock> mouse_revealed_lock_;

  // Lock which keeps the top-of-window views revealed based on the focused view
  // and the active widget.
  scoped_ptr<RevealedLock> focus_revealed_lock_;

  // Native window for the browser, needed to clean up observers.
  gfx::NativeWindow native_window_;

#if defined(USE_AURA)
  // Observer to disable immersive mode when window leaves the maximized state.
  class WindowObserver;
  scoped_ptr<WindowObserver> window_observer_;
#endif

  // Animation observers. They must be separate because animations can be
  // aborted and have their observers triggered at any time and the state
  // machine needs to know which animation completed.
  class AnimationObserver;
  scoped_ptr<AnimationObserver> slide_open_observer_;
  scoped_ptr<AnimationObserver> slide_closed_observer_;

  base::WeakPtrFactory<ImmersiveModeControllerAsh> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_
