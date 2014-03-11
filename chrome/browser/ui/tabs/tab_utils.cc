// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_utils.h"

#include "base/strings/string16.h"
#include "chrome/browser/media/audio_stream_monitor.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/multi_animation.h"

namespace chrome {

namespace {

// Interval between frame updates of the tab indicator animations.  This is not
// the usual 60 FPS because a trade-off must be made between tab UI animation
// smoothness and media recording/playback performance on low-end hardware.
const int kIndicatorFrameIntervalMs = 50;  // 20 FPS

// Fade-in/out duration for the tab indicator animations.  Fade-in is quick to
// immediately notify the user.  Fade-out is more gradual, so that the user has
// a chance of finding a tab that has quickly "blipped" on and off.
const int kIndicatorFadeInDurationMs = 200;
const int kIndicatorFadeOutDurationMs = 1000;

// Animation that throbs in (towards 1.0) and out (towards 0.0), and ends in the
// "in" state.
class TabRecordingIndicatorAnimation : public gfx::MultiAnimation {
 public:
  virtual ~TabRecordingIndicatorAnimation() {}

  // Overridden to provide alternating "towards in" and "towards out" behavior.
  virtual double GetCurrentValue() const OVERRIDE;

  static scoped_ptr<TabRecordingIndicatorAnimation> Create();

 private:
  TabRecordingIndicatorAnimation(const gfx::MultiAnimation::Parts& parts,
                                 const base::TimeDelta interval)
      : MultiAnimation(parts, interval) {}

  // Number of times to "toggle throb" the recording and tab capture indicators
  // when they first appear.
  static const int kCaptureIndicatorThrobCycles = 5;
};

double TabRecordingIndicatorAnimation::GetCurrentValue() const {
  return current_part_index() % 2 ?
      1.0 - MultiAnimation::GetCurrentValue() :
      MultiAnimation::GetCurrentValue();
}

scoped_ptr<TabRecordingIndicatorAnimation>
TabRecordingIndicatorAnimation::Create() {
  MultiAnimation::Parts parts;
  COMPILE_ASSERT(kCaptureIndicatorThrobCycles % 2 != 0,
                 must_be_odd_so_animation_finishes_in_showing_state);
  for (int i = 0; i < kCaptureIndicatorThrobCycles; ++i) {
    parts.push_back(MultiAnimation::Part(
        i % 2 ? kIndicatorFadeOutDurationMs : kIndicatorFadeInDurationMs,
        gfx::Tween::EASE_IN));
  }
  const base::TimeDelta interval =
      base::TimeDelta::FromMilliseconds(kIndicatorFrameIntervalMs);
  scoped_ptr<TabRecordingIndicatorAnimation> animation(
      new TabRecordingIndicatorAnimation(parts, interval));
  animation->set_continuous(false);
  return animation.Pass();
}

}  // namespace

bool ShouldTabShowFavicon(int capacity,
                          bool is_pinned_tab,
                          bool is_active_tab,
                          bool has_favicon,
                          TabMediaState media_state) {
  if (!has_favicon)
    return false;
  int required_capacity = 1;
  if (ShouldTabShowCloseButton(capacity, is_pinned_tab, is_active_tab))
    ++required_capacity;
  if (ShouldTabShowMediaIndicator(
          capacity, is_pinned_tab, is_active_tab, has_favicon, media_state)) {
    ++required_capacity;
  }
  return capacity >= required_capacity;
}

bool ShouldTabShowMediaIndicator(int capacity,
                                 bool is_pinned_tab,
                                 bool is_active_tab,
                                 bool has_favicon,
                                 TabMediaState media_state) {
  if (media_state == TAB_MEDIA_STATE_NONE)
    return false;
  if (ShouldTabShowCloseButton(capacity, is_pinned_tab, is_active_tab))
    return capacity >= 2;
  return capacity >= 1;
}

bool ShouldTabShowCloseButton(int capacity,
                              bool is_pinned_tab,
                              bool is_active_tab) {
  if (is_pinned_tab)
    return false;
  else if (is_active_tab)
    return true;
  else
    return capacity >= 3;
}

bool IsPlayingAudio(content::WebContents* contents) {
  AudioStreamMonitor* const audio_stream_monitor =
      AudioStreamMonitor::FromWebContents(contents);
  return audio_stream_monitor && audio_stream_monitor->WasRecentlyAudible();
}

TabMediaState GetTabMediaStateForContents(content::WebContents* contents) {
  if (!contents)
    return TAB_MEDIA_STATE_NONE;

  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  if (indicator) {
    if (indicator->IsBeingMirrored(contents))
      return TAB_MEDIA_STATE_CAPTURING;
    if (indicator->IsCapturingUserMedia(contents))
      return TAB_MEDIA_STATE_RECORDING;
  }

  if (IsPlayingAudio(contents))
    return TAB_MEDIA_STATE_AUDIO_PLAYING;

  return TAB_MEDIA_STATE_NONE;
}

const gfx::Image& GetTabMediaIndicatorImage(TabMediaState media_state) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  switch (media_state) {
    case TAB_MEDIA_STATE_AUDIO_PLAYING:
      return rb.GetNativeImageNamed(IDR_TAB_AUDIO_INDICATOR);
    case TAB_MEDIA_STATE_RECORDING:
      return rb.GetNativeImageNamed(IDR_TAB_RECORDING_INDICATOR);
    case TAB_MEDIA_STATE_CAPTURING:
      return rb.GetNativeImageNamed(IDR_TAB_CAPTURE_INDICATOR);
    case TAB_MEDIA_STATE_NONE:
      break;
  }
  NOTREACHED();
  return rb.GetNativeImageNamed(IDR_SAD_FAVICON);
}

scoped_ptr<gfx::Animation> CreateTabMediaIndicatorFadeAnimation(
    TabMediaState media_state) {
  if (media_state == TAB_MEDIA_STATE_RECORDING ||
      media_state == TAB_MEDIA_STATE_CAPTURING) {
    return TabRecordingIndicatorAnimation::Create().PassAs<gfx::Animation>();
  }

  // Note: While it seems silly to use a one-part MultiAnimation, it's the only
  // gfx::Animation implementation that lets us control the frame interval.
  gfx::MultiAnimation::Parts parts;
  const bool is_for_fade_in = (media_state != TAB_MEDIA_STATE_NONE);
  parts.push_back(gfx::MultiAnimation::Part(
      is_for_fade_in ? kIndicatorFadeInDurationMs : kIndicatorFadeOutDurationMs,
      gfx::Tween::EASE_IN));
  const base::TimeDelta interval =
      base::TimeDelta::FromMilliseconds(kIndicatorFrameIntervalMs);
  scoped_ptr<gfx::MultiAnimation> animation(
      new gfx::MultiAnimation(parts, interval));
  animation->set_continuous(false);
  return animation.PassAs<gfx::Animation>();
}

base::string16 AssembleTabTooltipText(const base::string16& title,
                                      TabMediaState media_state) {
  if (media_state == TAB_MEDIA_STATE_NONE)
    return title;

  base::string16 result = title;
  if (!result.empty())
    result.append(1, '\n');
  switch (media_state) {
    case TAB_MEDIA_STATE_AUDIO_PLAYING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_MEDIA_STATE_AUDIO_PLAYING));
      break;
    case TAB_MEDIA_STATE_RECORDING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_MEDIA_STATE_RECORDING));
      break;
    case TAB_MEDIA_STATE_CAPTURING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_MEDIA_STATE_CAPTURING));
      break;
    case TAB_MEDIA_STATE_NONE:
      NOTREACHED();
      break;
  }
  return result;
}

}  // namespace chrome
