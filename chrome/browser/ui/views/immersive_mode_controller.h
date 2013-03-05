// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IMMERSIVE_MODE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_IMMERSIVE_MODE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/timer.h"
#include "ui/base/events/event_handler.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/native_widget_types.h"

class BrowserView;
class RevealView;

namespace views {
class MouseWatcher;
class View;
}

// Controller for an "immersive mode" similar to MacOS presentation mode where
// the top-of-window views are hidden until the mouse hits the top of the
// screen. The tab strip is optionally painted with miniature "tab indicator"
// rectangles.
class ImmersiveModeController : public ui::EventHandler,
                                public ui::ImplicitAnimationObserver {
 public:
  explicit ImmersiveModeController(BrowserView* browser_view);
  virtual ~ImmersiveModeController();

  // Must initialize after browser view has a Widget and native window.
  void Init();

  // Enables or disables immersive mode.
  void SetEnabled(bool enabled);
  bool enabled() const { return enabled_; }

  // See member comment below.
  bool hide_tab_indicators() const { return hide_tab_indicators_; }

  // True when the controller is hiding the top views due to immersive mode.
  bool ShouldHideTopViews() const { return enabled_ && !revealed_; }

  // True when the controller is temporarily showing the top views.
  bool IsRevealed() const { return enabled_ && revealed_; }

  // Returns the view that contains the top UI components (tabstrip, toolbar
  // etc.) during an immersive mode reveal. Can return NULL when not in a
  // revealing state.
  views::View* reveal_view();

  // If the controller is temporarily revealing the top views ensures that
  // the reveal view's layer is on top and hence visible over web contents.
  void MaybeStackViewAtTop();

  // Shows the reveal view if immersive mode is enabled. Used when focus is
  // placed in the location bar, tools menu, etc.
  void MaybeStartReveal();

  // Immediately hides the reveal view, without animating.
  void CancelReveal();

  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  // Testing interface.
  void SetHideTabIndicatorsForTest(bool hide);
  void SetEnabledForTest(bool enabled);
  void StartRevealForTest();
  void OnRevealViewLostMouseForTest();

 private:
  class RevealView;

  enum Animate {
    ANIMATE_NO,
    ANIMATE_SLOW,
    ANIMATE_FAST,
  };
  enum Layout {
    LAYOUT_NO,
    LAYOUT_YES,
  };

  // Enables or disables observers for mouse move and window restore.
  void EnableWindowObservers(bool enable);

  // Temporarily reveals the top-of-window views while in immersive mode,
  // hiding them when the cursor exits the area of the top views. If |animate|
  // is not ANIMATE_NO, slides in the view, otherwise shows it immediately.
  void StartReveal(Animate animate);

  // Slide in the reveal view.
  void AnimateShowRevealView();

  // Called when the mouse exits the reveal view area, may end the reveal.
  void OnRevealViewLostMouse();

  // Called when the reveal view's children lose focus, may end the reveal.
  void OnRevealViewLostFocus();

  // Hides the top-of-window views. Optionally animates. Optionally updates
  // the |browser_view_| layout when the reveal finishes.
  void EndReveal(Animate animate, Layout layout);

  // Updates layout for |browser_view_| including window caption controls and
  // tab strip style |immersive_style|.
  void LayoutBrowserView(bool immersive_style);

  // Slide out the reveal view. Deletes the view when complete.
  void AnimateHideRevealView(int duration_ms);

  // Cleans up the reveal view when the hide animation completes.
  void OnHideAnimationCompleted();

  // Browser view holding the views to be shown and hidden. Not owned.
  BrowserView* browser_view_;

  // True when in immersive mode.
  bool enabled_;

  // True when the top-of-window views are being shown in a temporary reveal.
  // Represents the target state, not the current animation state, so may be
  // false while the view is still animating out.
  bool revealed_;

  // True if the miniature "tab indicators" should be hidden in the main browser
  // view when immersive mode is enabled.
  bool hide_tab_indicators_;

  // View holding the tabstrip and toolbar during a reveal. Exists for a short
  // time after |revealed_| is set false to allow layer animation to finish.
  scoped_ptr<RevealView> reveal_view_;

  // Timer to track cursor being held at the top.
  base::OneShotTimer<ImmersiveModeController> top_timer_;

  // Native window for the browser, needed to clean up observers.
  gfx::NativeWindow native_window_;

#if defined(USE_AURA)
  // Observer to disable immersive mode when window leaves the maximized state.
  class WindowObserver;
  scoped_ptr<WindowObserver> window_observer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IMMERSIVE_MODE_CONTROLLER_H_
