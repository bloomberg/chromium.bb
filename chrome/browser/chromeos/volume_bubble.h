// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VOLUME_BUBBLE_H_
#define CHROME_BROWSER_CHROMEOS_VOLUME_BUBBLE_H_

#include "app/active_window_watcher_x.h"
#include "app/slide_animation.h"
#include "base/singleton.h"
#include "chrome/browser/views/info_bubble.h"

namespace chromeos {

class VolumeBubbleView;

// Singleton class controlling volume bubble.
class VolumeBubble : public InfoBubbleDelegate,
                     public AnimationDelegate {
 public:
  static VolumeBubble* instance() {
    return Singleton<VolumeBubble>::get();
  }

  void ShowVolumeBubble(int percent);

 private:
  friend struct DefaultSingletonTraits<VolumeBubble>;

  VolumeBubble();
  void OnTimeout();

  // Overridden from InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape() { return true; }
  virtual bool FadeInOnShow() { return false; }

  // Overridden from AnimationDelegate.
  virtual void AnimationEnded(const Animation* animation);
  virtual void AnimationProgressed(const Animation* animation);

  // Previous and current volume percentages, -1 if not yet shown.
  int previous_percent_;
  int current_percent_;

  // Currently shown bubble or NULL.
  InfoBubble* bubble_;

  // Its contents view, owned by InfoBubble.
  VolumeBubbleView* view_;

  SlideAnimation animation_;
  base::OneShotTimer<VolumeBubble> timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(VolumeBubble);
};

}  // namespace

#endif  // CHROME_BROWSER_CHROMEOS_VOLUME_BUBBLE_H_
