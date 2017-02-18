// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_VIDEO_DETECTOR_H_
#define ASH_WM_VIDEO_DETECTOR_H_

#include <map>
#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ash/common/shell_observer.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ash {

// Watches for updates to windows and tries to detect when a video is playing.
// We err on the side of false positives and can be fooled by things like
// continuous scrolling of a page.
class ASH_EXPORT VideoDetector : public aura::EnvObserver,
                                 public aura::WindowObserver,
                                 public ShellObserver {
 public:
  // State of detected video activity.
  enum class State {
    // Video activity has been detected recently and there are no fullscreen
    // windows.
    PLAYING_WINDOWED,
    // Video activity has been detected recently and there is at least one
    // fullscreen window.
    PLAYING_FULLSCREEN,
    // Video activity has not been detected recently.
    NOT_PLAYING,
  };

  class Observer {
   public:
    // Invoked when the video playback state has changed.
    virtual void OnVideoStateChanged(VideoDetector::State state) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Minimum dimensions in pixels that a window update must have to be
  // considered a potential video frame.
  static const int kMinUpdateWidth;
  static const int kMinUpdateHeight;

  // Number of video-sized updates that we must see within a second in a window
  // before we assume that a video is playing.
  static const int kMinFramesPerSecond;

  // Timeout after which video is no longer considered to be playing.
  static const int kVideoTimeoutMs;

  // Duration video must be playing in a window before it is reported to
  // observers.
  static const int kMinVideoDurationMs;

  VideoDetector();
  ~VideoDetector() override;

  State state() const { return state_; }

  void set_now_for_test(base::TimeTicks now) { now_for_test_ = now; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Runs HandleVideoInactive() and returns true, or returns false if
  // |video_inactive_timer_| wasn't running.
  bool TriggerTimeoutForTest() WARN_UNUSED_RESULT;

  // EnvObserver overrides.
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver overrides.
  void OnDelegatedFrameDamage(aura::Window* window,
                              const gfx::Rect& region) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  // ShellObserver overrides.
  void OnAppTerminating() override;
  void OnFullscreenStateChanged(bool is_fullscreen,
                                WmWindow* root_window) override;

 private:
  // Called when video activity is observed in |window|.
  void HandleVideoActivity(aura::Window* window, base::TimeTicks now);

  // Called by |inactive_timer_| |kVideoTimeoutMs| after the last-observed video
  // activity.
  void HandleVideoInactive();

  // Updates |state_| and notifies |observers_| if it changed.
  void UpdateState();

  // Current playback state.
  State state_;

  // True if video has been observed in the last |kVideoTimeoutMs|.
  bool video_is_playing_;

  // Currently-fullscreen root windows.
  std::set<aura::Window*> fullscreen_root_windows_;

  // Maps from a window that we're tracking to information about it.
  class WindowInfo;
  using WindowInfoMap = std::map<aura::Window*, std::unique_ptr<WindowInfo>>;
  WindowInfoMap window_infos_;

  base::ObserverList<Observer> observers_;

  // Calls HandleVideoInactive().
  base::OneShotTimer video_inactive_timer_;

  // If set, used when the current time is needed.  This can be set by tests to
  // simulate the passage of time.
  base::TimeTicks now_for_test_;

  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_manager_;

  bool is_shutting_down_;

  DISALLOW_COPY_AND_ASSIGN(VideoDetector);
};

}  // namespace ash

#endif  // ASH_WM_VIDEO_DETECTOR_H_
