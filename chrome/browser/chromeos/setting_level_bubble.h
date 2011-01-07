// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_H_
#define CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/timer.h"
#include "chrome/browser/views/info_bubble.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"

class SkBitmap;

namespace chromeos {

class SettingLevelBubbleView;

// Singleton class controlling a bubble displaying a level-based setting like
// volume or brightness.
class SettingLevelBubble : public InfoBubbleDelegate,
                           public ui::AnimationDelegate {
 public:
  void ShowBubble(int percent);
  void HideBubble();

 protected:
  explicit SettingLevelBubble(SkBitmap* increase_icon,
                              SkBitmap* decrease_icon,
                              SkBitmap* zero_icon);
  virtual ~SettingLevelBubble() {}

 private:
  void OnTimeout();

  // Overridden from InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
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
  InfoBubble* bubble_;

  // Its contents view, owned by InfoBubble.
  SettingLevelBubbleView* view_;

  ui::SlideAnimation animation_;
  base::OneShotTimer<SettingLevelBubble> timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(SettingLevelBubble);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTING_LEVEL_BUBBLE_H_
