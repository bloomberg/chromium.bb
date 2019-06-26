// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_ANIMATION_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_ANIMATION_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"

// Interpolates between TabAnimationStates. Apply the current state to a tab
// to animate that tab.
class TabAnimation {
 public:
  static constexpr base::TimeDelta kAnimationDuration =
      base::TimeDelta::FromMilliseconds(200);

  ~TabAnimation();

  TabAnimation(TabAnimation&&) noexcept;
  TabAnimation& operator=(TabAnimation&&) noexcept;

  // Creates a TabAnimation for a tab with no active animations.
  static TabAnimation ForStaticState(TabAnimationState static_state,
                                     base::OnceClosure tab_removed_callback);

  // Animates this tab from its current state to |target_state|.
  // If an animation is already running, the duration is reset.
  void AnimateTo(TabAnimationState target_state);

  // Animates this tab from its current state to |target_state|.
  // Keeps the current remaining animation duration.
  void RetargetTo(TabAnimationState target_state);

  void CompleteAnimation();
  void CancelAnimation();

  // Notifies the owner of the animated tab that the close animation
  // has completed and the tab can be cleaned up.
  void NotifyCloseCompleted();

  TabAnimationState GetCurrentState() const;
  TabAnimationState target_state() const { return target_state_; }
  base::TimeDelta GetTimeRemaining() const;

 private:
  TabAnimation(TabAnimationState initial_state,
               TabAnimationState target_state,
               base::TimeDelta duration,
               base::OnceClosure tab_removed_callback);

  TabAnimationState initial_state_;
  TabAnimationState target_state_;
  base::TimeTicks start_time_;
  base::TimeDelta duration_;
  base::OnceClosure tab_removed_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_ANIMATION_H_
