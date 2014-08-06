// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_USER_SWITCH_ANIMATOR_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_USER_SWITCH_ANIMATOR_CHROMEOS_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"

namespace aura {
class Window;
}  // namespace aura

namespace chrome {

class MultiUserWindowManagerChromeOS;

// A class which performs transitions animations between users. Upon creation,
// the animation gets started and upon destruction the animation gets finished
// if not done yet.
// Specifying |animation_disabled| upon creation will perform the transition
// without visible animations.
class UserSwichAnimatorChromeOS {
 public:
  // The animation step for the user change animation.
  enum AnimationStep {
    ANIMATION_STEP_HIDE_OLD_USER,  // Hiding the old user (and shelf).
    ANIMATION_STEP_SHOW_NEW_USER,  // Show the shelf of the new user.
    ANIMATION_STEP_FINALIZE,       // All animations are done - final cleanup.
    ANIMATION_STEP_ENDED           // The animation has ended.
  };

  UserSwichAnimatorChromeOS(MultiUserWindowManagerChromeOS* owner,
                            const std::string& new_user_id,
                            int animation_speed_ms);
  ~UserSwichAnimatorChromeOS();

  // Check if a window is covering the entire work area of the screen it is on.
  static bool CoversScreen(aura::Window* window);

  bool IsAnimationFinished() {
    return animation_step_ == ANIMATION_STEP_ENDED;
  }

  // Returns the user id for which the wallpaper is currently shown.
  // If a wallpaper is transitioning to B it will be returned as "->B".
  const std::string& wallpaper_user_id_for_test() { return wallpaper_user_id_; }

  // Advances the user switch animation to the next step. It reads the current
  // step from |animation_step_| and increments it thereafter. When
  // |ANIMATION_STEP_FINALIZE| gets executed, the animation is finished and the
  // timer (if one exists) will get destroyed.
  void AdvanceUserTransitionAnimation();

  // When the system is shutting down, the animation can be stopped without
  // ending it.
  void CancelAnimation();

 private:
  // The window configuration of screen covering windows before an animation.
  enum TransitioningScreenCover {
    NO_USER_COVERS_SCREEN,   // No window covers the entire screen.
    OLD_USER_COVERS_SCREEN,  // The current user has at least one window
                             // covering the entire screen.
    NEW_USER_COVERS_SCREEN,  // The user which becomes active has at least one
                             // window covering the entire screen.
    BOTH_USERS_COVER_SCREEN  // Both users have at least one window each
                             // covering the entire screen.
  };

  // Finalizes the animation and ends the timer (if there is one).
  void FinalizeAnimation();

  // Execute the user wallpaper animations for |animation_step|.
  void TransitionWallpaper(AnimationStep animtion_step);

  // Execute the user shelf animations for |animation_step|.
  void TransitionUserShelf(AnimationStep animtion_step);

  // Execute the window animations for |animation_step|.
  void TransitionWindows(AnimationStep animation_step);

  // Check if a window is maximized / fullscreen / covering the entire screen.
  // If a |root_window| is given, the screen coverage of that root_window is
  // tested, otherwise all screens.
  TransitioningScreenCover GetScreenCover(aura::Window* root_window);

  // The owning window manager.
  MultiUserWindowManagerChromeOS* owner_;

  // The new user to set.
  std::string new_user_id_;

  // The animation speed in ms. If 0, animations are disabled.
  int animation_speed_ms_;

  // The next animation step for AdvanceUserTransitionAnimation().
  AnimationStep animation_step_;

  // The screen cover status before the animation has started.
  TransitioningScreenCover screen_cover_;

  // A timer which watches to executes the second part of a "user changed"
  // animation. Note that this timer exists only during such an animation.
  scoped_ptr<base::Timer> user_changed_animation_timer_;

  // For unit tests: Check which wallpaper was set.
  std::string wallpaper_user_id_;

  DISALLOW_COPY_AND_ASSIGN(UserSwichAnimatorChromeOS);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_USER_SWITCH_ANIMATOR_CHROMEOS_H_
