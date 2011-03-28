// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FULLSCREEN_EXIT_BUBBLE_H__
#define CHROME_BROWSER_UI_VIEWS_FULLSCREEN_EXIT_BUBBLE_H__
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/command_updater.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/controls/link.h"

namespace ui {
class SlideAnimation;
}

#if defined(OS_LINUX)
namespace views {
class WidgetGtk;
}
#endif

// FullscreenExitBubble is responsible for showing a bubble atop the screen in
// fullscreen mode, telling users how to exit and providing a click target.
// The bubble auto-hides, and re-shows when the user moves to the screen top.

class FullscreenExitBubble : public views::LinkController,
                             public ui::AnimationDelegate {
 public:
  explicit FullscreenExitBubble(
      views::Widget* frame,
      CommandUpdater::CommandUpdaterDelegate* delegate);
  virtual ~FullscreenExitBubble();

 private:
  class FullscreenExitView;

  static const double kOpacity;          // Opacity of the bubble, 0.0 - 1.0
  static const int kInitialDelayMs;      // Initial time bubble remains onscreen
  static const int kIdleTimeMs;          // Time before mouse idle triggers hide
  static const int kPositionCheckHz;     // How fast to check the mouse position
  static const int kSlideInRegionHeightPx;
                                         // Height of region triggering slide-in
  static const int kSlideInDurationMs;   // Duration of slide-in animation
  static const int kSlideOutDurationMs;  // Duration of slide-out animation

  // views::LinkController
  virtual void LinkActivated(views::Link* source, int event_flags);

  // ui::AnimationDelegate
  virtual void AnimationProgressed(const ui::Animation* animation);
  virtual void AnimationEnded(const ui::Animation* animation);

  // Called repeatedly to get the current mouse position and animate the bubble
  // on or off the screen as appropriate.
  void CheckMousePosition();

  // Hides the bubble.  This is a separate function so it can be called by a
  // timer.
  void Hide();

  // Returns the current desirable rect for the popup window.  If
  // |ignore_animation_state| is true this returns the rect assuming the popup
  // is fully onscreen.
  gfx::Rect GetPopupRect(bool ignore_animation_state) const;

  // The root view containing us.
  views::View* root_view_;

  // Someone who can toggle fullscreen mode on and off when the user requests
  // it.
  CommandUpdater::CommandUpdaterDelegate* delegate_;

  views::Widget* popup_;

  // The contents of the popup.
  FullscreenExitView* view_;

  // Animation controlling sliding into/out of the top of the screen.
  scoped_ptr<ui::SlideAnimation> size_animation_;

  // Timer to delay before allowing the bubble to hide after it's initially
  // shown.
  base::OneShotTimer<FullscreenExitBubble> initial_delay_;

  // Timer to see how long the mouse has been idle.
  base::OneShotTimer<FullscreenExitBubble> idle_timeout_;

  // Timer to poll the current mouse position.  We can't just listen for mouse
  // events without putting a non-empty HWND onscreen (or hooking Windows, which
  // has other problems), so instead we run a low-frequency poller to see if the
  // user has moved in or out of our show/hide regions.
  base::RepeatingTimer<FullscreenExitBubble> mouse_position_checker_;

  // The most recently seen mouse position, in screen coordinates.  Used to see
  // if the mouse has moved since our last check.
  gfx::Point last_mouse_pos_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenExitBubble);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FULLSCREEN_EXIT_BUBBLE_H__
