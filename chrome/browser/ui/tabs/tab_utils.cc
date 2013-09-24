// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_utils.h"

#include "chrome/browser/media/audio_stream_indicator.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/animation/multi_animation.h"

namespace chrome {

namespace {

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

  // Throbbing fade in/out duration on "this web page is watching and/or
  // listening to you" favicon overlay.
  static const int kCaptureIndicatorCycleDurationMs = 1000;

  // Number of times to "toggle throb" the recording and tab capture indicators
  // when they first appear.
  static const int kCaptureIndicatorThrobCycles = 5;

  // Interval between frame updates of the recording and tab capture indicator
  // throb animations.
  static const int kCaptureIndicatorFrameIntervalMs = 50;  // 20 FPS
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
        kCaptureIndicatorCycleDurationMs, gfx::Tween::EASE_IN));
  }
  const base::TimeDelta interval =
      base::TimeDelta::FromMilliseconds(kCaptureIndicatorFrameIntervalMs);
  scoped_ptr<TabRecordingIndicatorAnimation> animation(
      new TabRecordingIndicatorAnimation(parts, interval));
  animation->set_continuous(false);
  return animation.Pass();
}

}  // namespace

bool ShouldShowProjectingIndicator(content::WebContents* contents) {
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  return indicator->IsBeingMirrored(contents);
}

bool ShouldShowRecordingIndicator(content::WebContents* contents) {
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  // The projecting indicator takes precedence over the recording indicator, but
  // if we are projecting and we don't handle the projecting case we want to
  // still show the recording indicator.
  return indicator->IsCapturingUserMedia(contents) ||
         indicator->IsBeingMirrored(contents);
}

bool IsPlayingAudio(content::WebContents* contents) {
  AudioStreamIndicator* audio_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->GetAudioStreamIndicator()
          .get();
  return audio_indicator->IsPlayingAudio(contents);
}

bool IsCapturingVideo(content::WebContents* contents) {
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  return indicator->IsCapturingVideo(contents);
}

bool IsCapturingAudio(content::WebContents* contents) {
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  return indicator->IsCapturingAudio(contents);
}

scoped_ptr<gfx::Animation> CreateTabRecordingIndicatorAnimation() {
  return TabRecordingIndicatorAnimation::Create().PassAs<gfx::Animation>();
}

}  // namespace chrome
