// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_

#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"

#include "base/timer.h"
#include "ui/base/events/event_handler.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget_observer.h"

class BrowserView;

namespace aura {
class Window;
}

namespace gfx {
class Transform;
}

namespace ui {
class Layer;
}

namespace views {
class View;
}

class ImmersiveModeControllerAsh : public ImmersiveModeController,
                                   public ui::EventHandler,
                                   public ui::ImplicitAnimationObserver,
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

  // Shows the reveal view without any animations if immersive mode is enabled.
  void MaybeRevealWithoutAnimation();

  // ImmersiveModeController overrides:
  virtual void Init(BrowserView* browser_view) OVERRIDE;
  virtual void SetEnabled(bool enabled) OVERRIDE;
  virtual bool IsEnabled() const OVERRIDE;
  virtual bool ShouldHideTabIndicators() const OVERRIDE;
  virtual bool ShouldHideTopViews() const OVERRIDE;
  virtual bool IsRevealed() const OVERRIDE;
  virtual void MaybeStackViewAtTop() OVERRIDE;
  virtual ImmersiveModeController::RevealedLock*
      GetRevealedLock() OVERRIDE WARN_UNUSED_RESULT;
  virtual void AnchorWidgetToTopContainer(views::Widget* widget,
                                          int y_offset) OVERRIDE;
  virtual void UnanchorWidgetFromTopContainer(views::Widget* widget) OVERRIDE;
  virtual void OnTopContainerBoundsChanged() OVERRIDE;

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

  // ui::ImplicitAnimationObserver override:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

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
  void MaybeStartReveal(Animate animate);

  // Enables or disables layer-based painting to allow smooth animations.
  void EnablePaintToLayer(bool enable);

  // Updates layout for |browser_view_| including window caption controls and
  // tab strip style |immersive_style|.
  void LayoutBrowserView(bool immersive_style);

  // Called when the animation to slide open the top-of-window views has
  // completed.
  void OnSlideOpenAnimationCompleted();

  // Hides the top-of-window views if immersive mode is enabled and nothing is
  // keeping them revealed. Optionally animates.
  void MaybeEndReveal(Animate animate);

  // Called when the animation to slide out the top-of-window views has
  // completed.
  void OnSlideClosedAnimationCompleted();

  // Starts an animation for the top-of-window views and any anchored widgets
  // of |duration_ms| to |target_transform|.
  void DoAnimation(const gfx::Transform& target_transform, int duration_ms);

  // Starts an animation for |layer| of |duration_ms| to |target_transform|.
  // If non-NULL, sets |observer| to be notified when the animation completes.
  void DoLayerAnimation(ui::Layer* layer,
                        const gfx::Transform& target_transform,
                        int duration_ms,
                        ui::ImplicitAnimationObserver* observer);

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
  aura::Window* native_window_;

  // Observer to disable immersive mode when window leaves the maximized state.
  class WindowObserver;
  scoped_ptr<WindowObserver> window_observer_;

  // Manages widgets which are anchored to the top-of-window views.
  class AnchoredWidgetManager;
  scoped_ptr<AnchoredWidgetManager> anchored_widget_manager_;

  base::WeakPtrFactory<ImmersiveModeControllerAsh> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_
