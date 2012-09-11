// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_VIDEO_DETECTOR_H_
#define ASH_WM_VIDEO_DETECTOR_H_

#include <map>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/time.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ash {

class ASH_EXPORT VideoDetectorObserver {
 public:
  // Invoked periodically while a video is being played onscreen.
  virtual void OnVideoDetected(bool is_fullscreen) = 0;

 protected:
  virtual ~VideoDetectorObserver() {}
};

// Watches for updates to windows and tries to detect when a video is playing.
// We err on the side of false positives and can be fooled by things like
// continuous scrolling of a page.
class ASH_EXPORT VideoDetector : public aura::EnvObserver,
                                 public aura::WindowObserver {
 public:
  // Minimum dimensions in pixels that a window update must have to be
  // considered a potential video frame.
  static const int kMinUpdateWidth;
  static const int kMinUpdateHeight;

  // Number of video-sized updates that we must see within a second in a window
  // before we assume that a video is playing.
  static const int kMinFramesPerSecond;

  // Minimum amount of time between notifications to observers that a video is
  // playing.
  static const double kNotifyIntervalSec;

  VideoDetector();
  virtual ~VideoDetector();

  void set_now_for_test(base::TimeTicks now) { now_for_test_ = now; }

  void AddObserver(VideoDetectorObserver* observer);
  void RemoveObserver(VideoDetectorObserver* observer);

  // EnvObserver overrides.
  virtual void OnWindowInitialized(aura::Window* window) OVERRIDE;

  // WindowObserver overrides.
  virtual void OnWindowPaintScheduled(aura::Window* window,
                                      const gfx::Rect& region) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 private:
  class WindowInfo;
  typedef std::map<aura::Window*, linked_ptr<WindowInfo> > WindowInfoMap;

  // Possibly notifies observers in response to detection of a video in
  // |window|.  Notifications are rate-limited and don't get sent if the window
  // is invisible or offscreen.
  void MaybeNotifyObservers(aura::Window* window, base::TimeTicks now);

  // Maps from a window that we're tracking to information about it.
  WindowInfoMap window_infos_;

  ObserverList<VideoDetectorObserver> observers_;

  // Last time at which we notified observers that a video was playing.
  base::TimeTicks last_observer_notification_time_;

  // If set, used when the current time is needed.  This can be set by tests to
  // simulate the passage of time.
  base::TimeTicks now_for_test_;

  ScopedObserver<aura::Window, aura::WindowObserver> observer_manager_;

  DISALLOW_COPY_AND_ASSIGN(VideoDetector);
};

}  // namespace ash

#endif  // ASH_WM_VIDEO_DETECTOR_H_
