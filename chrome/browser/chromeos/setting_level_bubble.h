// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_H_
#define CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "base/timer.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "views/widget/widget.h"

class SkBitmap;

namespace chromeos {

class SettingLevelBubbleView;

// Controls a bubble displaying a level-based setting like
// volume or brightness.
class SettingLevelBubble : public views::Widget::Observer {
 public:
  // Shows the bubble with the target |percent| and |enabled| setting.
  // |percent| should be in the range [0.0, 100.0].
  void ShowBubble(double percent, bool enabled);

  // Hides the Bubble, closing the view.
  void HideBubble();

  // Updates the bubble's current level without showing the bubble onscreen.
  // We _do_ still animate the level moving to |percent| in case the bubble is
  // still visible from a previous call to ShowBubble().
  //
  // This can be used when the setting has been changed automatically and we
  // want to make sure that it's animated from the correct position the next
  // time that the bubble is shown.  For example:
  //
  // 1. Brightness is at 50%.
  // 2. Power manager dims brightness to 25% automatically.
  // 3. User hits the "increase brightness" button, setting brightness to 30%.
  //
  // If we didn't update our internal state to 25% after 2), then the animation
  // displayed in response to 3) would show the bubble animating from 50% down
  // to 30%, rather than from 25% up to 30%.
  void UpdateWithoutShowingBubble(double percent, bool enabled);

 protected:
  SettingLevelBubble(SkBitmap* increase_icon,
                     SkBitmap* decrease_icon,
                     SkBitmap* zero_icon);

  virtual ~SettingLevelBubble();

 private:
  FRIEND_TEST_ALL_PREFIXES(SettingLevelBubbleTest, CreateAndUpdate);
  FRIEND_TEST_ALL_PREFIXES(SettingLevelBubbleTest, ShowBubble);
  FRIEND_TEST_ALL_PREFIXES(BrightnessBubbleTest, UpdateWithoutShowing);
  FRIEND_TEST_ALL_PREFIXES(VolumeBubbleTest, GetInstanceAndShow);

  // views::Widget::Observer overrides:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // Creates the bubble content view.
  // Caller should call Init() on the returned SettingLevelBubbleView.
  SettingLevelBubbleView* CreateView();

  // Callback for |hide_timer_|.  Starts fading out.
  void OnHideTimeout();

  // Callback for |animation_timer_|.  Updates the level displayed by the view,
  // also stopping the animation if we've reached the target.
  void OnAnimationTimeout();

  // Animates towards |percent|.  Updates |target_percent_| and starts
  // |animation_timer_| if it's not already running.  If this is the first time
  // that the level is being set, we just update |view_| immediately and don't
  // animate.
  void UpdateTargetPercent(double percent);

  // Stops |animation_timer_| if it's running.
  void StopAnimation();

  // Current and target percentages for the progress bar.  In the range
  // [0.0, 100.0], or -1.0 if not yet shown.
  double current_percent_;
  double target_percent_;

  // Time at which we'll reach |target_percent_|.
  base::TimeTicks target_time_;

  // Time at which we last updated |current_percent_|.
  base::TimeTicks last_animation_update_time_;

  // Time at which |target_percent_| was last updated.
  base::TimeTicks last_target_update_time_;

  // Icons displayed in the bubble when increasing or decreasing the level or
  // when it's disabled.  Not owned by us.
  SkBitmap* increase_icon_;
  SkBitmap* decrease_icon_;
  SkBitmap* disabled_icon_;

  // Contents view owned by Bubble.
  SettingLevelBubbleView* view_;

  // Timer to hide the bubble.
  base::OneShotTimer<SettingLevelBubble> hide_timer_;

  // Timer to animate the currently-shown percent.  We use a timer instead of
  // ui::Animation since our animations are frequently interrupted by additional
  // changes to the level, and ui::Animation doesn't provide much control over
  // in-progress animations, leading to mega-jank.
  base::RepeatingTimer<SettingLevelBubble> animation_timer_;

  // Is |animation_timer_| currently running?
  bool is_animating_;

  DISALLOW_COPY_AND_ASSIGN(SettingLevelBubble);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_H_
