// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/video_detector.h"

#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/rect.h"

namespace ash {

const int VideoDetector::kMinUpdateWidth = 300;
const int VideoDetector::kMinUpdateHeight = 225;
const int VideoDetector::kMinFramesPerSecond = 15;
const double VideoDetector::kNotifyIntervalSec = 1.0;

// Stores information about updates to a window and determines whether it's
// likely that a video is playing in it.
class VideoDetector::WindowInfo {
 public:
  WindowInfo() : num_video_updates_in_second_(0) {}

  // Handles an update within a window, returning true if this update made us
  // believe that a video is playing in the window.  true is returned at most
  // once per second.
  bool RecordUpdateAndCheckForVideo(const gfx::Rect& region,
                                    base::TimeTicks now) {
    if (region.width() < kMinUpdateWidth || region.height() < kMinUpdateHeight)
      return false;

    if (second_start_time_.is_null() ||
        (now - second_start_time_).InSecondsF() >= 1.0) {
      second_start_time_ = now;
      num_video_updates_in_second_ = 0;
    }
    num_video_updates_in_second_++;

    return num_video_updates_in_second_ == kMinFramesPerSecond;
  }

 private:
  // Number of video-sized updates that we've seen in the second starting at
  // |second_start_time_|.  (Keeping a rolling window is overkill here.)
  int num_video_updates_in_second_;

  base::TimeTicks second_start_time_;

  DISALLOW_COPY_AND_ASSIGN(WindowInfo);
};

VideoDetector::VideoDetector()
    : ALLOW_THIS_IN_INITIALIZER_LIST(observer_manager_(this)) {
  aura::Env::GetInstance()->AddObserver(this);
}

VideoDetector::~VideoDetector() {
  aura::Env::GetInstance()->RemoveObserver(this);
}

void VideoDetector::AddObserver(VideoDetectorObserver* observer) {
  observers_.AddObserver(observer);
}

void VideoDetector::RemoveObserver(VideoDetectorObserver* observer) {
  observers_.RemoveObserver(observer);
}

void VideoDetector::OnWindowInitialized(aura::Window* window) {
  observer_manager_.Add(window);
}

void VideoDetector::OnWindowPaintScheduled(aura::Window* window,
                                           const gfx::Rect& region) {
  linked_ptr<WindowInfo>& info = window_infos_[window];
  if (!info.get())
    info.reset(new WindowInfo);

  base::TimeTicks now =
      !now_for_test_.is_null() ? now_for_test_ : base::TimeTicks::Now();
  if (info->RecordUpdateAndCheckForVideo(region, now))
    MaybeNotifyObservers(window, now);
}

void VideoDetector::OnWindowDestroyed(aura::Window* window) {
  window_infos_.erase(window);
  observer_manager_.Remove(window);
}

void VideoDetector::MaybeNotifyObservers(aura::Window* window,
                                         base::TimeTicks now) {
  if (!last_observer_notification_time_.is_null() &&
      (now - last_observer_notification_time_).InSecondsF() <
      kNotifyIntervalSec)
    return;

  if (!window->IsVisible())
    return;

  gfx::Rect root_bounds = window->GetRootWindow()->bounds();
  if (!window->GetBoundsInRootWindow().Intersects(root_bounds))
    return;

  bool is_fullscreen = wm::IsWindowFullscreen(window);
  FOR_EACH_OBSERVER(VideoDetectorObserver,
                    observers_,
                    OnVideoDetected(is_fullscreen));
  last_observer_notification_time_ = now;
}

}  // namespace ash
