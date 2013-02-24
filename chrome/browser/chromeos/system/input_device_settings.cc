// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/input_device_settings.h"

#include <stdarg.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace system {

namespace {
const char kTpControl[] = "/opt/google/touchpad/tpcontrol";
const char kMouseControl[] = "/opt/google/mouse/mousecontrol";

bool ScriptExists(const std::string& script) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  return file_util::PathExists(base::FilePath(script));
}

// Executes the input control script asynchronously, if it exists.
void ExecuteScriptOnFileThread(const std::vector<std::string>& argv) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  DCHECK(!argv.empty());
  const std::string& script(argv[0]);

  // Script must exist on device.
  DCHECK(!base::chromeos::IsRunningOnChromeOS() || ScriptExists(script));

  if (!ScriptExists(script))
    return;

  base::LaunchOptions options;
  options.wait = true;
  base::LaunchProcess(CommandLine(argv), options, NULL);
}

void ExecuteScript(int argc, ...) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::vector<std::string> argv;
  va_list vl;
  va_start(vl, argc);
  for (int i = 0; i < argc; ++i) {
    argv.push_back(va_arg(vl, const char*));
  }
  va_end(vl);

  content::BrowserThread::GetBlockingPool()->PostTask(FROM_HERE,
      base::Bind(&ExecuteScriptOnFileThread, argv));
}

void SetPointerSensitivity(const char* script, int value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(value > 0 && value < 6);
  ExecuteScript(3, script, "sensitivity", StringPrintf("%d", value).c_str());
}

void SetTPControl(const char* control, bool enabled) {
  ExecuteScript(3, kTpControl, control, enabled ? "on" : "off");
}

void DeviceExistsBlockingPool(const char* script, bool* exists) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  *exists = false;
  if (!ScriptExists(script))
    return;

  std::vector<std::string> argv;
  argv.push_back(script);
  argv.push_back("status");
  std::string output;
  // Output is empty if the device is not found.
  *exists = base::GetAppOutput(CommandLine(argv), &output) && !output.empty();
  DVLOG(1) << "DeviceExistsBlockingPool:" << script << "=" << *exists;
}

void RunCallbackUIThread(bool* exists, const DeviceExistsCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DVLOG(1) << "RunCallbackUIThread " << *exists;
  callback.Run(*exists);
}

void DeviceExists(const char* script, const DeviceExistsCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  bool* exists = new bool(false);
  content::BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
      base::Bind(&DeviceExistsBlockingPool, script, exists),
      base::Bind(&RunCallbackUIThread, base::Owned(exists), callback));
}

}  // namespace

namespace touchpad_settings {

void TouchpadExists(const DeviceExistsCallback& callback) {
  DeviceExists(kTpControl, callback);
}

// Sets the touchpad sensitivity in the range [1, 5].
void SetSensitivity(int value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  SetPointerSensitivity(kTpControl, value);
}

void SetTapToClick(bool enabled) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  SetTPControl("taptoclick", enabled);
}

void SetThreeFingerClick(bool enabled) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  SetTPControl("three_finger_click", enabled);
  // For Alex/ZGB.
  SetTPControl("t5r2_three_finger_click", enabled);
}

void SetThreeFingerSwipe(bool enabled) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  SetTPControl("three_finger_swipe", enabled);
}

void SetTapDragging(bool enabled) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  SetTPControl("tap_dragging", enabled);
}

}  // namespace touchpad_settings

namespace mouse_settings {

void MouseExists(const DeviceExistsCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DeviceExists(kMouseControl, callback);
}

// Sets the touchpad sensitivity in the range [1, 5].
void SetSensitivity(int value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  SetPointerSensitivity(kMouseControl, value);
}

void SetPrimaryButtonRight(bool right) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ExecuteScript(3, kMouseControl, "swap_left_right", right ? "1" : "0");
}

}  // namespace mouse_settings

}  // namespace system
}  // namespace chromeos
