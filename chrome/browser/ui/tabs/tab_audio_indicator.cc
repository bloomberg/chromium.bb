// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_audio_indicator.h"

#include "grit/theme_resources.h"
#include "ui/base/animation/animation_container.h"
#include "ui/base/animation/linear_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"

namespace {

// The number of columns to draw for the equalizer graphic.
const size_t kEqualizerColumnCount = 3;

// The maximum level for the equalizer.
const size_t kEqualizerMaxLevel = 8;

// The equalizer cycles between these frames. An equalizer frame is 3 columns
// where each column ranges from 0 to |kEqualizerMaxLevel|. TODO(sail): Replace
// this with levels from the actual audio source.
const size_t kEqualizerFrames[][kEqualizerColumnCount] = {
  { 7, 5, 3 },
  { 4, 5, 7 },
  { 4, 2, 5 },
  { 3, 7, 4 },
  { 2, 3, 2 },
  { 3, 4, 3 },
};

// The duration of each equalizer frame.
const size_t kAnimationCycleDurationMs = 300;

// The duration of the "ending" animation once audio stops playing.
const size_t kAnimationEndingDurationMs = 1000;

// Target frames per second. In reality fewer frames are drawn because the
// equalizer levels change slowly.
const int kFPS = 15;

}  // namespace

TabAudioIndicator::TabAudioIndicator(Delegate* delegate)
    : delegate_(delegate),
      frame_index_(0),
      state_(STATE_NOT_ANIMATING) {
}

TabAudioIndicator::~TabAudioIndicator() {
}

void TabAudioIndicator::SetAnimationContainer(
    ui::AnimationContainer* animation_container) {
  animation_container_ = animation_container;
}

void TabAudioIndicator::SetIsPlayingAudio(bool is_playing_audio) {
  if (is_playing_audio && state_ != STATE_ANIMATING) {
    state_ = STATE_ANIMATING;
    animation_.reset(
        new ui::LinearAnimation(kAnimationCycleDurationMs, kFPS, this));
    animation_->SetContainer(animation_container_);
    animation_->Start();
  } else if (!is_playing_audio && state_ == STATE_ANIMATING) {
    state_ = STATE_ANIMATION_ENDING;
    animation_.reset(
        new ui::LinearAnimation(kAnimationEndingDurationMs, kFPS, this));
    animation_->SetContainer(animation_container_);
    animation_->Start();
  }
}

bool TabAudioIndicator::IsAnimating() {
  return state_ != STATE_NOT_ANIMATING;
}

void TabAudioIndicator::Paint(gfx::Canvas* canvas, const gfx::Rect& rect) {
  if (state_ == STATE_NOT_ANIMATING)
    return;

  canvas->Save();
  canvas->ClipRect(rect);

  // Draw 3 equalizer columns. |IDR_AUDIO_EQUALIZER_COLUMN| is a column of the
  // equalizer with 8 levels. The current level is between 0 and 8 so the
  // image is shifted down and then drawn.
  ui::ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image(rb.GetImageSkiaNamed(IDR_AUDIO_EQUALIZER_COLUMN));
  int x = rect.x();
  std::vector<int> levels = GetCurrentEqualizerLevels();
  for (size_t i = 0; i < levels.size(); ++i) {
    // Shift the image down by the level. For example, for level 8 draw the
    // image at rect.y(), For level 7, draw the image at rect.y() - 2, etc...
    int y = rect.y() + (kEqualizerMaxLevel - levels[i]) * 2;
    canvas->DrawImageInt(*image, x, y);
    x += image->width() - 1;
  }

  canvas->Restore();

  // Cache the levels that were just drawn. This is used to prevent unnecessary
  // drawing when animation progress doesn't result in equalizer levels
  // changing.
  last_displayed_equalizer_levels_ = levels;
}

void TabAudioIndicator::AnimationProgressed(const ui::Animation* animation) {
  std::vector<int> levels = GetCurrentEqualizerLevels();
  if (last_displayed_equalizer_levels_ != levels)
    delegate_->ScheduleAudioIndicatorPaint();
}

void TabAudioIndicator::AnimationEnded(const ui::Animation* animation) {
  if (state_ == STATE_ANIMATING) {
    // The current equalizer frame animation has finished. Start animating the
    // next frame.
    frame_index_ = (frame_index_ + 1) % arraysize(kEqualizerFrames);
    animation_->Start();
  } else if (state_ == STATE_ANIMATION_ENDING) {
    // The "ending" animation has stopped. Update the tab state so that the UI
    // can update the tab icon.
    state_ = STATE_NOT_ANIMATING;
    delegate_->ScheduleAudioIndicatorPaint();
  }
}

std::vector<int> TabAudioIndicator::GetCurrentEqualizerLevels() const {
  int next_frame_index = (frame_index_ + 1) % arraysize(kEqualizerFrames);
  std::vector<int> levels;
  // For all 3 columsn of the equalizer, tween between the current equalizer
  // level and the target equalizer level.
  for (size_t i = 0; i < kEqualizerColumnCount; ++i) {
    int start = kEqualizerFrames[frame_index_][i];
    int end = state_ == STATE_ANIMATION_ENDING
              ? 0
              : kEqualizerFrames[next_frame_index][i];
    levels.push_back(animation_->CurrentValueBetween(start, end));
  }
  return levels;
}
