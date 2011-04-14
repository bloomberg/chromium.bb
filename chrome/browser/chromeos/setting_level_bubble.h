// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_H_
#define CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/timer.h"
#include "chrome/browser/ui/views/bubble/bubble.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"

class SkBitmap;

namespace chromeos {

class SettingLevelBubbleView;

// Singleton class controlling a bubble displaying a level-based setting like
// volume or brightness.
class SettingLevelBubble : public BubbleDelegate,
                           public ui::AnimationDelegate {
 public:
  void ShowBubble(int percent);
  void HideBubble();

  // Update the bubble's current level without showing the bubble onscreen.
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
  void UpdateWithoutShowingBubble(int percent);

 protected:
  explicit SettingLevelBubble(SkBitmap* increase_icon,
                              SkBitmap* decrease_icon,
                              SkBitmap* zero_icon);
  virtual ~SettingLevelBubble() {}

 private:
  void OnTimeout();

  // Overridden from BubbleDelegate.
  virtual void BubbleClosing(Bubble* bubble, bool closed_by_escape);
  virtual bool CloseOnEscape() { return true; }
  virtual bool FadeInOnShow() { return false; }

  // Overridden from ui::AnimationDelegate.
  virtual void AnimationEnded(const ui::Animation* animation);
  virtual void AnimationProgressed(const ui::Animation* animation);

  // Previous and current percentages, or -1 if not yet shown.
  int previous_percent_;
  int current_percent_;

  // Icons displayed in the bubble when increasing or decreasing the level or
  // setting it to zero.  Not owned by us.
  SkBitmap* increase_icon_;
  SkBitmap* decrease_icon_;
  SkBitmap* zero_icon_;

  // Currently shown bubble or NULL.
  Bubble* bubble_;

  // Its contents view, owned by Bubble.
  SettingLevelBubbleView* view_;

  ui::SlideAnimation animation_;
  base::OneShotTimer<SettingLevelBubble> timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(SettingLevelBubble);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_H_
