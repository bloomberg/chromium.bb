// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/input_device_settings.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace system {

namespace {

InputDeviceSettings* g_instance_;
InputDeviceSettings* g_test_instance_;

const char kDeviceTypeTouchpad[] = "touchpad";
const char kDeviceTypeMouse[] = "mouse";
const char kInputControl[] = "/opt/google/input/inputcontrol";

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
  DCHECK(!base::SysInfo::IsRunningOnChromeOS() || ScriptExists(script));

  if (!ScriptExists(script))
    return;

  base::ProcessHandle handle;
  base::LaunchProcess(CommandLine(argv), base::LaunchOptions(), &handle);
  base::EnsureProcessGetsReaped(handle);
}

void ExecuteScript(const std::vector<std::string>& argv) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (argv.size() == 1)
    return;

  VLOG(1) << "About to launch: \""
          << CommandLine(argv).GetCommandLineString() << "\"";

  // Control scripts can take long enough to cause SIGART during shutdown
  // (http://crbug.com/261426). Run the blocking pool task with
  // CONTINUE_ON_SHUTDOWN so it won't be joined when Chrome shuts down.
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  scoped_refptr<base::TaskRunner> runner =
      pool->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  runner->PostTask(FROM_HERE, base::Bind(&ExecuteScriptOnFileThread, argv));
}

void AddSensitivityArguments(const char* device_type, int value,
                             std::vector<std::string>* argv) {
  DCHECK(value >= kMinPointerSensitivity && value <= kMaxPointerSensitivity);
  argv->push_back(base::StringPrintf("--%s_sensitivity=%d",
                                     device_type, value));
}

void AddTPControlArguments(const char* control,
                           bool enabled,
                           std::vector<std::string>* argv) {
  argv->push_back(base::StringPrintf("--%s=%d", control, enabled ? 1 : 0));
}

void DeviceExistsBlockingPool(const char* device_type,
                              scoped_refptr<RefCountedBool> exists) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  exists->data = false;
  if (!ScriptExists(kInputControl))
    return;

  std::vector<std::string> argv;
  argv.push_back(kInputControl);
  argv.push_back(base::StringPrintf("--type=%s", device_type));
  argv.push_back("--list");
  std::string output;
  // Output is empty if the device is not found.
  exists->data = base::GetAppOutput(CommandLine(argv), &output) &&
      !output.empty();
  DVLOG(1) << "DeviceExistsBlockingPool:" << device_type << "=" << exists->data;
}

void RunCallbackUIThread(
    scoped_refptr<RefCountedBool> exists,
    const InputDeviceSettings::DeviceExistsCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DVLOG(1) << "RunCallbackUIThread " << exists->data;
  callback.Run(exists->data);
}

void DeviceExists(const char* script,
                  const InputDeviceSettings::DeviceExistsCallback& callback) {
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

class InputDeviceSettingsImpl : public InputDeviceSettings {
 public:
  InputDeviceSettingsImpl();

 private:
  // Overridden from InputDeviceSettings.
  virtual void TouchpadExists(const DeviceExistsCallback& callback) OVERRIDE;
  virtual void UpdateTouchpadSettings(const TouchpadSettings& settings)
      OVERRIDE;
  virtual void SetTouchpadSensitivity(int value) OVERRIDE;
  virtual void SetTapToClick(bool enabled) OVERRIDE;
  virtual void SetThreeFingerClick(bool enabled) OVERRIDE;
  virtual void SetTapDragging(bool enabled) OVERRIDE;
  virtual void SetNaturalScroll(bool enabled) OVERRIDE;
  virtual void MouseExists(const DeviceExistsCallback& callback) OVERRIDE;
  virtual void UpdateMouseSettings(const MouseSettings& update) OVERRIDE;
  virtual void SetMouseSensitivity(int value) OVERRIDE;
  virtual void SetPrimaryButtonRight(bool right) OVERRIDE;
  virtual bool ForceKeyboardDrivenUINavigation() OVERRIDE;
  virtual void ReapplyTouchpadSettings() OVERRIDE;
  virtual void ReapplyMouseSettings() OVERRIDE;

 private:
  TouchpadSettings current_touchpad_settings_;
  MouseSettings current_mouse_settings_;

  DISALLOW_COPY_AND_ASSIGN(InputDeviceSettingsImpl);
};

InputDeviceSettingsImpl::InputDeviceSettingsImpl() {}

void InputDeviceSettingsImpl::TouchpadExists(
    const DeviceExistsCallback& callback) {
  DeviceExists(kDeviceTypeTouchpad, callback);
}

void InputDeviceSettingsImpl::UpdateTouchpadSettings(
    const TouchpadSettings& settings) {
  std::vector<std::string> argv;
  if (current_touchpad_settings_.Update(settings, &argv))
    ExecuteScript(argv);
}

void InputDeviceSettingsImpl::SetTouchpadSensitivity(int value) {
  TouchpadSettings settings;
  settings.SetSensitivity(value);
  UpdateTouchpadSettings(settings);
}

void InputDeviceSettingsImpl::SetNaturalScroll(bool enabled) {
  TouchpadSettings settings;
  settings.SetNaturalScroll(enabled);
  UpdateTouchpadSettings(settings);
}

void InputDeviceSettingsImpl::SetTapToClick(bool enabled) {
  TouchpadSettings settings;
  settings.SetTapToClick(enabled);
  UpdateTouchpadSettings(settings);
}

void InputDeviceSettingsImpl::SetThreeFingerClick(bool enabled) {
  // For Alex/ZGB.
  TouchpadSettings settings;
  settings.SetThreeFingerClick(enabled);
  UpdateTouchpadSettings(settings);
}

void InputDeviceSettingsImpl::SetTapDragging(bool enabled) {
  TouchpadSettings settings;
  settings.SetTapDragging(enabled);
  UpdateTouchpadSettings(settings);
}

void InputDeviceSettingsImpl::MouseExists(
    const DeviceExistsCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DeviceExists(kDeviceTypeMouse, callback);
}

void InputDeviceSettingsImpl::UpdateMouseSettings(const MouseSettings& update) {
  std::vector<std::string> argv;
  if (current_mouse_settings_.Update(update, &argv))
    ExecuteScript(argv);
}

void InputDeviceSettingsImpl::SetMouseSensitivity(int value) {
  MouseSettings settings;
  settings.SetSensitivity(value);
  UpdateMouseSettings(settings);
}

void InputDeviceSettingsImpl::SetPrimaryButtonRight(bool right) {
  MouseSettings settings;
  settings.SetPrimaryButtonRight(right);
  UpdateMouseSettings(settings);
}

bool InputDeviceSettingsImpl::ForceKeyboardDrivenUINavigation() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector)
    return false;

  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (!policy_manager)
    return false;

  if (policy_manager->IsRemoraRequisition() ||
      policy_manager->IsSharkRequisition()) {
    return true;
  }

  bool keyboard_driven = false;
  if (chromeos::system::StatisticsProvider::GetInstance()->GetMachineFlag(
          kOemKeyboardDrivenOobeKey, &keyboard_driven)) {
    return keyboard_driven;
  }

  return false;
}

void InputDeviceSettingsImpl::ReapplyTouchpadSettings() {
  TouchpadSettings settings = current_touchpad_settings_;
  current_touchpad_settings_ = TouchpadSettings();
  UpdateTouchpadSettings(settings);
}

void InputDeviceSettingsImpl::ReapplyMouseSettings() {
  MouseSettings settings = current_mouse_settings_;
  current_mouse_settings_ = MouseSettings();
  UpdateMouseSettings(settings);
}

}  // namespace

TouchpadSettings::TouchpadSettings() {}

TouchpadSettings& TouchpadSettings::operator=(const TouchpadSettings& other) {
  if (&other != this) {
    sensitivity_ = other.sensitivity_;
    tap_to_click_ = other.tap_to_click_;
    three_finger_click_ = other.three_finger_click_;
    tap_dragging_ = other.tap_dragging_;
    natural_scroll_ = other.natural_scroll_;
  }
  return *this;
}

void TouchpadSettings::SetSensitivity(int value) {
  sensitivity_.Set(value);
}

int TouchpadSettings::GetSensitivity() const {
  return sensitivity_.value();
}

void TouchpadSettings::SetTapToClick(bool enabled) {
  tap_to_click_.Set(enabled);
}

bool TouchpadSettings::GetTapToClick() const {
  return tap_to_click_.value();
}

void TouchpadSettings::SetNaturalScroll(bool enabled) {
  natural_scroll_.Set(enabled);
}

bool TouchpadSettings::GetNaturalScroll() const {
  return natural_scroll_.value();
}

void TouchpadSettings::SetThreeFingerClick(bool enabled) {
  three_finger_click_.Set(enabled);
}

bool TouchpadSettings::GetThreeFingerClick() const {
  return three_finger_click_.value();
}

void TouchpadSettings::SetTapDragging(bool enabled) {
  tap_dragging_.Set(enabled);
}

bool TouchpadSettings::GetTapDragging() const {
  return tap_dragging_.value();
}

bool TouchpadSettings::Update(const TouchpadSettings& settings,
                              std::vector<std::string>* argv) {
  if (argv)
    argv->push_back(kInputControl);
  bool updated = false;
  if (sensitivity_.Update(settings.sensitivity_)) {
    updated = true;
    if (argv)
      AddSensitivityArguments(kDeviceTypeTouchpad, sensitivity_.value(), argv);
  }
  if (tap_to_click_.Update(settings.tap_to_click_)) {
    updated = true;
    if (argv)
      AddTPControlArguments("tapclick", tap_to_click_.value(), argv);
  }
  if (three_finger_click_.Update(settings.three_finger_click_)) {
    updated = true;
    if (argv)
      AddTPControlArguments("t5r2_three_finger_click",
                            three_finger_click_.value(),
                            argv);
  }
  if (tap_dragging_.Update(settings.tap_dragging_)) {
    updated = true;
    if (argv)
      AddTPControlArguments("tapdrag", tap_dragging_.value(), argv);
  }
  if (natural_scroll_.Update(settings.natural_scroll_)) {
    updated = true;
    if (argv)
      AddTPControlArguments("australian_scrolling", natural_scroll_.value(),
                            argv);
  }
  return updated;
}

MouseSettings::MouseSettings() {}

MouseSettings& MouseSettings::operator=(const MouseSettings& other) {
  if (&other != this) {
    sensitivity_ = other.sensitivity_;
    primary_button_right_ = other.primary_button_right_;
  }
  return *this;
}

void MouseSettings::SetSensitivity(int value) {
  sensitivity_.Set(value);
}

int MouseSettings::GetSensitivity() const {
  return sensitivity_.value();
}

void MouseSettings::SetPrimaryButtonRight(bool right) {
  primary_button_right_.Set(right);
}

bool MouseSettings::GetPrimaryButtonRight() const {
  return primary_button_right_.value();
}

bool MouseSettings::Update(const MouseSettings& settings,
                           std::vector<std::string>* argv) {
  if (argv)
    argv->push_back(kInputControl);
  bool updated = false;
  if (sensitivity_.Update(settings.sensitivity_)) {
    updated = true;
    if (argv)
      AddSensitivityArguments(kDeviceTypeMouse, sensitivity_.value(), argv);
  }
  if (primary_button_right_.Update(settings.primary_button_right_)) {
    updated = true;
    if (argv) {
      AddTPControlArguments("mouse_swap_lr", primary_button_right_.value(),
                            argv);
    }
  }
  return updated;
}

// static
InputDeviceSettings* InputDeviceSettings::Get() {
  if (g_test_instance_)
    return g_test_instance_;
  if (!g_instance_)
    g_instance_ = new InputDeviceSettingsImpl;
  return g_instance_;
}

// static
void InputDeviceSettings::SetSettingsForTesting(
    InputDeviceSettings* test_settings) {
  if (g_test_instance_ == test_settings)
    return;
  delete g_test_instance_;
  g_test_instance_ = test_settings;
}

}  // namespace system
}  // namespace chromeos
