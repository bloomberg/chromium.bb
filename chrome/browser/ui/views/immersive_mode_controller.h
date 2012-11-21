// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IMMERSIVE_MODE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_IMMERSIVE_MODE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/timer.h"
#include "ui/base/events/event_handler.h"
#include "ui/views/mouse_watcher.h"

class BrowserView;

namespace views {
class MouseWatcher;
}

// Controller for an "immersive mode" similar to MacOS presentation mode where
// the top-of-window views are hidden until the mouse hits the top of the screen
// and the tab strip is painted in a rectangular "light-bar" style.
class ImmersiveModeController : public ui::EventHandler,
                                public views::MouseWatcherListener {
 public:
  explicit ImmersiveModeController(BrowserView* browser_view);
  virtual ~ImmersiveModeController();

  // Enables or disables immersive mode.
  void SetEnabled(bool enabled);
  bool enabled() const { return enabled_; }

  // True when we are hiding the top views due to immersive mode.
  bool ShouldHideTopViews() const { return enabled_ && hide_top_views_; }

  // Temporarily reveals the top-of-window views while in immersive mode,
  // hiding them when the cursor exits the area of the top views.
  // Visible for testing.
  void RevealTopViews();

  // ui::EventHandler overrides:
  virtual ui::EventResult OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual ui::EventResult OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual ui::EventResult OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual ui::EventResult OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // views::MouseWatcherListener overrides:
  virtual void MouseMovedOutOfHost() OVERRIDE;

 private:
  // Browser view holding the views to be shown and hidden. Not owned.
  BrowserView* browser_view_;

  // True when in immersive mode.
  bool enabled_;

  // True when the top-of-window views are being hidden by immersive mode.
  bool hide_top_views_;

  // Timer to track cursor being held at the top.
  base::OneShotTimer<ImmersiveModeController> top_timer_;

  // Mouse watcher to detect when the cursor leaves the top-of-window views
  // in immersive mode, indicating that we should hide the views.
  scoped_ptr<views::MouseWatcher> mouse_watcher_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IMMERSIVE_MODE_CONTROLLER_H_
