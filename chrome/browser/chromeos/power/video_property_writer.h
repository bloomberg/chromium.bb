// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_VIDEO_PROPERTY_WRITER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_VIDEO_PROPERTY_WRITER_H_
#pragma once

#include "ash/wm/video_detector.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"

namespace chromeos {

// Writes a property on the X root window to inform the power manager that a
// video is playing.
class VideoPropertyWriter : public ash::VideoDetectorObserver {
 public:
  VideoPropertyWriter();
  virtual ~VideoPropertyWriter();

  // ash::VideoDetectorObserver implementation.
  virtual void OnVideoDetected() OVERRIDE;

 private:
  // Last time that the X property was updated.
  base::TimeTicks last_update_time_;

  DISALLOW_COPY_AND_ASSIGN(VideoPropertyWriter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_VIDEO_PROPERTY_WRITER_H_
