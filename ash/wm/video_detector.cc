// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/video_detector.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/window_util.h"

namespace ash {

const int VideoDetector::kMinUpdateWidth = 333;
const int VideoDetector::kMinUpdateHeight = 250;
const int VideoDetector::kMinFramesPerSecond = 15;
const double VideoDetector::kNotifyIntervalSec = 1.0;

// Stores information about updates to a window and determines whether it's
// likely that a video is playing in it.
class VideoDetector::WindowInfo {
 public:
  WindowInfo() : buffer_start_(0), buffer_size_(0) {}

  // Handles an update within a window, returning true if it appears that
  // video is currently playing in the window.
  bool RecordUpdateAndCheckForVideo(const gfx::Rect& region,
                                    base::TimeTicks now) {
    if (region.width() < kMinUpdateWidth || region.height() < kMinUpdateHeight)
      return false;

    // If the buffer is full, drop the first timestamp.
    if (buffer_size_ == static_cast<size_t>(kMinFramesPerSecond)) {
      buffer_start_ = (buffer_start_ + 1) % kMinFramesPerSecond;
      buffer_size_--;
    }

    update_times_[(buffer_start_ + buffer_size_) % kMinFramesPerSecond] = now;
    buffer_size_++;

    return buffer_size_ == static_cast<size_t>(kMinFramesPerSecond) &&
        (now - update_times_[buffer_start_]).InSecondsF() <= 1.0;
  }

 private:
  // Circular buffer containing update times of the last (up to
  // |kMinFramesPerSecond|) video-sized updates to this window.
  base::TimeTicks update_times_[kMinFramesPerSecond];

  // Index into |update_times_| of the oldest update.
  size_t buffer_start_;

  // Number of updates stored in |update_times_|.
  size_t buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(WindowInfo);
};

VideoDetector::VideoDetector()
    : observer_manager_(this),
      is_shutting_down_(false) {
  aura::Env::GetInstance()->AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
}

VideoDetector::~VideoDetector() {
  Shell::GetInstance()->RemoveShellObserver(this);
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

void VideoDetector::OnDelegatedFrameDamage(
    aura::Window* window,
    const gfx::Rect& damage_rect_in_dip) {
  if (is_shutting_down_)
    return;
  linked_ptr<WindowInfo>& info = window_infos_[window];
  if (!info.get())
    info.reset(new WindowInfo);

  base::TimeTicks now =
      !now_for_test_.is_null() ? now_for_test_ : base::TimeTicks::Now();
  if (info->RecordUpdateAndCheckForVideo(damage_rect_in_dip, now))
    MaybeNotifyObservers(window, now);
}

void VideoDetector::OnWindowDestroyed(aura::Window* window) {
  window_infos_.erase(window);
  observer_manager_.Remove(window);
}

void VideoDetector::OnAppTerminating() {
  // Stop checking video activity once the shutdown
  // process starts. crbug.com/231696.
  is_shutting_down_ = true;
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

  // As a relatively-cheap way to avoid flipping back and forth between
  // fullscreen and non-fullscreen notifications when one video is playing in a
  // fullscreen window and a second video is playing in a non-fullscreen window,
  // report fullscreen video whenever a fullscreen window exists on any desktop
  // regardless of whether the video is actually playing in that window:
  // http://crbug.com/340666
  bool fullscreen_window_exists = false;
  std::vector<aura::Window*> containers =
      Shell::GetContainersFromAllRootWindows(kShellWindowId_DefaultContainer,
                                             NULL);
  for (std::vector<aura::Window*>::const_iterator container =
       containers.begin(); container != containers.end(); ++container) {
    const aura::Window::Windows& windows = (*container)->children();
    for (aura::Window::Windows::const_iterator window = windows.begin();
         window != windows.end(); ++window) {
      if (wm::GetWindowState(*window)->IsFullscreen()) {
        fullscreen_window_exists = true;
        break;
      }
    }
  }

  FOR_EACH_OBSERVER(VideoDetectorObserver,
                    observers_,
                    OnVideoDetected(fullscreen_window_exists));
  last_observer_notification_time_ = now;
}

}  // namespace ash
