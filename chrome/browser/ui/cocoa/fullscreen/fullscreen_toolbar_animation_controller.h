// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_TOOLBAR_ANIMATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_TOOLBAR_ANIMATION_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/timer/timer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"

@class FullscreenToolbarController;

// This class provides a controller that manages the fullscreen toolbar's
// animation.
class FullscreenToolbarAnimationController : public gfx::AnimationDelegate {
 public:
  explicit FullscreenToolbarAnimationController(
      FullscreenToolbarController* owner);

  // Called by |owner_| when the fullscreen toolbar layout is updated.
  void ToolbarDidUpdate();

  // Stops the animation and cancels |hide_toolbar_timer_|.
  void StopAnimationAndTimer();

  // Animates the toolbar in and out to show changes with the tabstrip.
  void AnimateToolbarForTabstripChanges();

  // Animates the toolbar in if it's not fully shown.
  void AnimateToolbarIn();

  // Animates the toolbar out if it's not focused.
  void AnimateToolbarOutIfPossible();

  // Returns the fraction of the toolbar exposed at the top according to the
  // animation's progress.
  CGFloat GetToolbarFractionFromProgress() const;

  // Returns true if |animation_| is running.
  bool IsAnimationRunning() const;

  // Only for testing. Sets the duration of |animation_|.
  void SetAnimationDuration(CGFloat duration);

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  // Our owner.
  FullscreenToolbarController* owner_;  // weak

  // The animation of the decoration.
  gfx::SlideAnimation animation_;

  // Timer that will start the scrollbar's hiding animation when it reaches 0.
  base::Timer hide_toolbar_timer_;

  // The value that the animation should start from.
  CGFloat animation_start_value_;

  // True when the toolbar is dropped to show tabstrip changes.
  BOOL should_hide_toolbar_after_delay_;
};

#endif  // CHROME_BROWSER_UI_COCOA_FULLSCREEN_FULLSCREEN_TOOLBAR_ANIMATION_CONTROLLER_H_