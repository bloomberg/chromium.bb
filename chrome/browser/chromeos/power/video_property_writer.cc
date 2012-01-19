// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/video_property_writer.h"

#include <ctime>

#include "ash/shell.h"
#include "ui/base/x/x11_util.h"

namespace {

// Minimum number of seconds between updates to the X property.
const int kUpdateIntervalSec = 5;

// Name of the X property on the root window.  Also used as the property's type.
const char kPropertyName[] = "_CHROME_VIDEO_TIME";

}  // namespace

namespace chromeos {

VideoPropertyWriter::VideoPropertyWriter() {
  ash::Shell::GetInstance()->video_detector()->AddObserver(this);
}

VideoPropertyWriter::~VideoPropertyWriter() {
  ash::Shell::GetInstance()->video_detector()->RemoveObserver(this);
}

void VideoPropertyWriter::OnVideoDetected() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (last_update_time_.is_null() ||
      (now - last_update_time_).InSecondsF() >= kUpdateIntervalSec) {
    last_update_time_ = now;
    if (!ui::SetIntProperty(
            ui::GetX11RootWindow(),
            kPropertyName,
            kPropertyName,
            time(NULL))) {
      LOG(WARNING) << "Failed setting " << kPropertyName << " on root window";
    }
  }
}

}  // namespace chromeos
