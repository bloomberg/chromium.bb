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
#include "ui/views/mouse_watcher.h"

class BrowserView;
class RevealView;

namespace views {
class MouseWatcher;
}

// Controller for an "immersive mode" similar to MacOS presentation mode where
// the top-of-window views are hidden until the mouse hits the top of the screen
// and the tab strip is painted in a rectangular "light-bar" style.
class ImmersiveModeController : public ui::EventHandler,
                                public ui::ImplicitAnimationObserver,
                                public views::MouseWatcherListener {
 public:
  explicit ImmersiveModeController(BrowserView* browser_view);
  virtual ~ImmersiveModeController();

  // Enables or disables immersive mode.
  void SetEnabled(bool enabled);
  bool enabled() const { return enabled_; }

  // True when we are hiding the top views due to immersive mode.
  bool ShouldHideTopViews() const { return enabled_ && !revealed_; }

  bool IsRevealed() const { return enabled_ && revealed_; }

  // ui::EventHandler overrides:
  virtual ui::EventResult OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual ui::EventResult OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual ui::EventResult OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual ui::EventResult OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  // views::MouseWatcherListener overrides:
  virtual void MouseMovedOutOfHost() OVERRIDE;

  // Testing interface.
  void StartRevealForTest();
  void EndRevealForTest();

 private:
  // Temporarily reveals the top-of-window views while in immersive mode,
  // hiding them when the cursor exits the area of the top views.
  void StartReveal();

  // Slide in the reveal view.
  void AnimateShowRevealView();

  // Starts watching the mouse for when it leaves the RevealView.
  void StartMouseWatcher();

  // Hides the top-of-window views.
  void EndReveal(bool animate);

  // Slide out the reveal view. Deletes the view when complete.
  void AnimateHideRevealView();

  // Cleans up the reveal view when the slide-out completes.
  void ResetRevealView();

  // Browser view holding the views to be shown and hidden. Not owned.
  BrowserView* browser_view_;

  // True when in immersive mode.
  bool enabled_;

  // True when the top-of-window views are being shown in a temporary reveal.
  bool revealed_;

  // View holding the tabstrip and toolbar during a reveal.
  scoped_ptr<RevealView> reveal_view_;

  // Timer to track cursor being held at the top.
  base::OneShotTimer<ImmersiveModeController> top_timer_;

  // Mouse watcher to detect when the cursor leaves the top-of-window views
  // in immersive mode, indicating that we should hide the views.
  scoped_ptr<views::MouseWatcher> mouse_watcher_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IMMERSIVE_MODE_CONTROLLER_H_
