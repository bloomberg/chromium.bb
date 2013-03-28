// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_AUDIO_INDICATOR_H_
#define CHROME_BROWSER_UI_TABS_TAB_AUDIO_INDICATOR_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
class Canvas;
class Rect;
}
namespace ui {
class Animation;
class AnimationContainer;
class LinearAnimation;
}

// This class is used to draw an animating tab audio indicator.
class TabAudioIndicator : public ui::AnimationDelegate {
 public:
  class Delegate {
   public:
    virtual void ScheduleAudioIndicatorPaint() = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit TabAudioIndicator(Delegate* delegate);
  virtual ~TabAudioIndicator();

  void set_favicon(const gfx::ImageSkia& favicon) { favicon_ = favicon; }

  void SetAnimationContainer(ui::AnimationContainer* animation_container);
  void SetIsPlayingAudio(bool is_playing_audio);
  bool IsAnimating();

  void Paint(gfx::Canvas* canvas, const gfx::Rect& rect);

 private:
  enum State {
    STATE_NOT_ANIMATING,
    STATE_ANIMATING,
    STATE_ANIMATION_ENDING,
  };

  FRIEND_TEST_ALL_PREFIXES(TabAudioIndicatorTest, AnimationState);
  FRIEND_TEST_ALL_PREFIXES(TabAudioIndicatorTest, SchedulePaint);

  // AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  // Gets the equalizer levels for all 3 columns. The values are tweened between
  // the current and target frame.
  std::vector<int> GetCurrentEqualizerLevels() const;

  Delegate* delegate_;
  scoped_ptr<ui::LinearAnimation> animation_;
  scoped_refptr<ui::AnimationContainer> animation_container_;
  gfx::ImageSkia favicon_;

  // The equalizer frame that's currently being displayed.
  size_t frame_index_;

  // The equalizer levels that were last displayed. This is used to prevent
  // unnecessary drawing when animation progress doesn't result in equalizer
  // levels changing.
  std::vector<int> last_displayed_equalizer_levels_;

  State state_;

  DISALLOW_COPY_AND_ASSIGN(TabAudioIndicator);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_AUDIO_INDICATOR_H_
