// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_audio_indicator.h"

#include "base/message_loop.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/animation/linear_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"

class TabAudioIndicatorTest : public TabAudioIndicator::Delegate,
                              public testing::Test {
 protected:
  TabAudioIndicatorTest() : schedule_paint_count_(0) {}

  virtual void ScheduleAudioIndicatorPaint() OVERRIDE {
    ++schedule_paint_count_;
  }

  int schedule_paint_count_;
  MessageLoopForUI message_loop_;  // Needed for ui::LinearAnimation.

 private:
  DISALLOW_COPY_AND_ASSIGN(TabAudioIndicatorTest);
};

TEST_F(TabAudioIndicatorTest, AnimationState) {
  // Start animating.
  TabAudioIndicator indicator(this);
  indicator.SetIsPlayingAudio(true);
  EXPECT_EQ(TabAudioIndicator::STATE_ANIMATING, indicator.state_);
  EXPECT_TRUE(indicator.IsAnimating());

  // Once the audio stops the indicator should switch to ending animation.
  indicator.SetIsPlayingAudio(false);
  EXPECT_EQ(TabAudioIndicator::STATE_ANIMATION_ENDING, indicator.state_);
  EXPECT_TRUE(indicator.IsAnimating());

  // Once the ending animation is complete animation should stop.
  indicator.animation_->End();
  EXPECT_EQ(TabAudioIndicator::STATE_NOT_ANIMATING, indicator.state_);
  EXPECT_FALSE(indicator.IsAnimating());
}

TEST_F(TabAudioIndicatorTest, Paint) {
  TabAudioIndicator indicator(this);
  indicator.SetIsPlayingAudio(true);

  gfx::Rect rect(0, 0, 16, 16);
  gfx::Canvas canvas(rect.size(), ui::SCALE_FACTOR_100P, true);

  // Nothing to test here. Just exercise the paint code to verify that nothing
  // leaks or crashes.
  indicator.Paint(&canvas, rect);

  // Paint with a favicon.
  ui::ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  indicator.set_favicon(*rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_16));
  indicator.Paint(&canvas, rect);
}

TEST_F(TabAudioIndicatorTest, SchedulePaint) {
  TabAudioIndicator indicator(this);
  indicator.SetIsPlayingAudio(true);

  indicator.animation_->SetCurrentValue(1.0);
  schedule_paint_count_ = 0;
  indicator.AnimationProgressed(NULL);
  EXPECT_EQ(1, schedule_paint_count_);
}
