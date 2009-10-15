// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/touchpad.h"

#include <stdlib.h>
#include <string>
#include <vector>

#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"

// static
void Touchpad::SetSynclientParam(const std::string& param, double value) {
  // If not running on the file thread, then re-run on the file thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::FILE)) {
    base::Thread* file_thread = g_browser_process->file_thread();
    if (file_thread)
      file_thread->message_loop()->PostTask(FROM_HERE,
          NewRunnableFunction(&Touchpad::SetSynclientParam, param, value));
  } else {
    // launch binary synclient to set the parameter
    std::vector<std::string> argv;
    argv.push_back("/usr/bin/synclient");
    argv.push_back(StringPrintf("%s=%f", param.c_str(), value));
    base::file_handle_mapping_vector no_files;
    base::ProcessHandle handle;
    if (!base::LaunchApp(argv, no_files, true, &handle))
      LOG(ERROR) << "Failed to call /usr/bin/synclient";
  }
}

// static
void Touchpad::SetTapToClick(bool value) {
  // To disable tap-to-click (i.e. a tap on the touchpad is recognized as a left
  // mouse click event), we set MaxTapTime to 0. MaxTapTime is the maximum time
  // (in milliseconds) for detecting a tap. The default is 180.
  if (value)
    SetSynclientParam("MaxTapTime", 180);
  else
    SetSynclientParam("MaxTapTime", 0);
}

// static
void Touchpad::SetVertEdgeScroll(bool value) {
  // To disable vertical edge scroll, we set VertEdgeScroll to 0. Vertical edge
  // scroll lets you use the right edge of the touchpad to control the movement
  // of the vertical scroll bar.
  if (value)
    SetSynclientParam("VertEdgeScroll", 1);
  else
    SetSynclientParam("VertEdgeScroll", 0);
}

// static
void Touchpad::SetSpeedFactor(int value) {
  // To set speed factor, we use MaxSpeed. MinSpeed is set to 0.2.
  // MaxSpeed can go from 0.2 to 1.1. The preference is an integer between
  // 1 and 10, so we divide that by 10 and add 0.1 for the value of MaxSpeed.
  if (value < 1)
    value = 1;
  if (value > 10)
    value = 10;
  // Convert from 1-10 to 0.2-1.1
  double d = static_cast<double>(value) / 10.0 + 0.1;
  SetSynclientParam("MaxSpeed", d);
}

// static
void Touchpad::SetSensitivity(int value) {
  // To set the touch sensitivity, we use FingerHigh, which represents the
  // the pressure needed for a tap to be registered. The range of FingerHigh
  // goes from 25 to 70. We store the sensitivity preference as an int from
  // 1 to 10. So we need to map the preference value of 1 to 10 to the
  // FingerHigh value of 25 to 70 inversely.
  if (value < 1)
    value = 1;
  if (value > 10)
    value = 10;
  // Convert from 1-10 to 70-25.
  double d = (15 - value) * 5;
  SetSynclientParam("FingerHigh", d);
}
