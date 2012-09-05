// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_VIDEO_ACTIVITY_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_VIDEO_ACTIVITY_NOTIFIER_H_

#include "ash/wm/video_detector.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"

namespace chromeos {

// Notifies the power manager when a video is playing.
class VideoActivityNotifier : public ash::VideoDetectorObserver {
 public:
  VideoActivityNotifier();
  virtual ~VideoActivityNotifier();

  // ash::VideoDetectorObserver implementation.
  virtual void OnVideoDetected(bool is_fullscreen) OVERRIDE;

 private:
  // Last time that the power manager was notified.
  base::TimeTicks last_notify_time_;

  DISALLOW_COPY_AND_ASSIGN(VideoActivityNotifier);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_VIDEO_ACTIVITY_NOTIFIER_H_
