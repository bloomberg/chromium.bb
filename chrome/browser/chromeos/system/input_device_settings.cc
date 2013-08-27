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
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace system {

namespace {

const char kTpControl[] = "/opt/google/touchpad/tpcontrol";
const char kMouseControl[] = "/opt/google/mouse/mousecontrol";

const char kRemoraRequisition[] = "remora";

typedef base::RefCountedData<bool> RefCountedBool;

bool ScriptExists(const std::string& script) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  return base::PathExists(base::FilePath(script));
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

  base::ProcessHandle handle;
  base::LaunchProcess(CommandLine(argv), base::LaunchOptions(), &handle);
  base::EnsureProcessGetsReaped(handle);
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

  // Control scripts can take long enough to cause SIGART during shutdown
  // (http://crbug.com/261426). Run the blocking pool task with
  // CONTINUE_ON_SHUTDOWN so it won't be joined when Chrome shuts down.
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  scoped_refptr<base::TaskRunner> runner =
      pool->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  runner->PostTask(FROM_HERE, base::Bind(&ExecuteScriptOnFileThread, argv));
}

void SetPointerSensitivity(const char* script, int value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(value >= kMinPointerSensitivity && value <= kMaxPointerSensitivity);
  ExecuteScript(
      3, script, "sensitivity", base::StringPrintf("%d", value).c_str());
}

void SetTPControl(const char* control, bool enabled) {
  ExecuteScript(3, kTpControl, control, enabled ? "on" : "off");
}

void DeviceExistsBlockingPool(const char* script,
                              scoped_refptr<RefCountedBool> exists) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  exists->data = false;
  if (!ScriptExists(script))
    return;

  std::vector<std::string> argv;
  argv.push_back(script);
  argv.push_back("status");
  std::string output;
  // Output is empty if the device is not found.
  exists->data = base::GetAppOutput(CommandLine(argv), &output) &&
      !output.empty();
  DVLOG(1) << "DeviceExistsBlockingPool:" << script << "=" << exists->data;
}

void RunCallbackUIThread(scoped_refptr<RefCountedBool> exists,
                         const DeviceExistsCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DVLOG(1) << "RunCallbackUIThread " << exists->data;
  callback.Run(exists->data);
}

void DeviceExists(const char* script, const DeviceExistsCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // One or both of the control scripts can apparently hang during shutdown
  // (http://crbug.com/255546). Run the blocking pool task with
  // CONTINUE_ON_SHUTDOWN so it won't be joined when Chrome shuts down.
  scoped_refptr<RefCountedBool> exists(new RefCountedBool(false));
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  scoped_refptr<base::TaskRunner> runner =
      pool->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  runner->PostTaskAndReply(FROM_HERE,
      base::Bind(&DeviceExistsBlockingPool, script, exists),
      base::Bind(&RunCallbackUIThread, exists, callback));
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

  // For Alex/ZGB.
  SetTPControl("t5r2_three_finger_click", enabled);
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

namespace keyboard_settings {

bool ForceKeyboardDrivenUINavigation() {
  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  if (!connector)
    return false;

  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (!policy_manager)
    return false;

  if (policy_manager->GetDeviceRequisition() == kRemoraRequisition)
    return true;

  bool keyboard_driven = false;
  if (chromeos::system::StatisticsProvider::GetInstance()->GetMachineFlag(
      kOemKeyboardDrivenOobeKey, &keyboard_driven)) {
    return keyboard_driven;
  }

  return false;
}

}  // namespace keyboard_settings

}  // namespace system
}  // namespace chromeos
