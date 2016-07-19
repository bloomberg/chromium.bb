// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/input_device_settings.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
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
#include "chrome/browser/chromeos/system/fake_input_device_settings.h"
#include "content/public/browser/browser_thread.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/gfx/x/x11_types.h"

namespace chromeos {
namespace system {

namespace {

InputDeviceSettings* g_instance = nullptr;

const char kDeviceTypeTouchpad[] = "touchpad";
const char kDeviceTypeMouse[] = "mouse";
const char kInputControl[] = "/opt/google/input/inputcontrol";

// The name of the xinput device corresponding to the internal touchpad.
const char kInternalTouchpadName[] = "Elan Touchpad";

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

  // Script must exist on device and is of correct format.
  DCHECK(script.compare(kInputControl) == 0);
  DCHECK(!base::SysInfo::IsRunningOnChromeOS() || ScriptExists(script));

  if (!ScriptExists(script))
    return;

  base::Process process =
      base::LaunchProcess(base::CommandLine(argv), base::LaunchOptions());
  if (process.IsValid())
    base::EnsureProcessGetsReaped(process.Pid());
}

void ExecuteScript(const std::vector<std::string>& argv) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (argv.size() == 1)
    return;

  VLOG(1) << "About to launch: \""
          << base::CommandLine(argv).GetCommandLineString() << "\"";

  // Control scripts can take long enough to cause SIGART during shutdown
  // (http://crbug.com/261426). Run the blocking pool task with
  // CONTINUE_ON_SHUTDOWN so it won't be joined when Chrome shuts down.
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  scoped_refptr<base::TaskRunner> runner =
      pool->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  runner->PostTask(FROM_HERE, base::Bind(&ExecuteScriptOnFileThread, argv));
}

void AddSensitivityArguments(const char* device_type,
                             int value,
                             std::vector<std::string>* argv) {
  DCHECK(value >= kMinPointerSensitivity && value <= kMaxPointerSensitivity);
  argv->push_back(
      base::StringPrintf("--%s_sensitivity=%d", device_type, value));
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
  exists->data =
      base::GetAppOutput(base::CommandLine(argv), &output) && !output.empty();
  DVLOG(1) << "DeviceExistsBlockingPool:" << device_type << "=" << exists->data;
}

void RunCallbackUIThread(
    scoped_refptr<RefCountedBool> exists,
    const InputDeviceSettings::DeviceExistsCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "RunCallbackUIThread " << exists->data;
  callback.Run(exists->data);
}

void DeviceExists(const char* script,
                  const InputDeviceSettings::DeviceExistsCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // One or both of the control scripts can apparently hang during shutdown
  // (http://crbug.com/255546). Run the blocking pool task with
  // CONTINUE_ON_SHUTDOWN so it won't be joined when Chrome shuts down.
  scoped_refptr<RefCountedBool> exists(new RefCountedBool(false));
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  scoped_refptr<base::TaskRunner> runner =
      pool->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  runner->PostTaskAndReply(
      FROM_HERE, base::Bind(&DeviceExistsBlockingPool, script, exists),
      base::Bind(&RunCallbackUIThread, exists, callback));
}

// InputDeviceSettings for Linux with X11.
class InputDeviceSettingsImplX11 : public InputDeviceSettings {
 public:
  InputDeviceSettingsImplX11();

 protected:
  ~InputDeviceSettingsImplX11() override {}

 private:
  // Overridden from InputDeviceSettings.
  void TouchpadExists(const DeviceExistsCallback& callback) override;
  void UpdateTouchpadSettings(const TouchpadSettings& settings) override;
  void SetTouchpadSensitivity(int value) override;
  void SetTapToClick(bool enabled) override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTapDragging(bool enabled) override;
  void SetNaturalScroll(bool enabled) override;
  void MouseExists(const DeviceExistsCallback& callback) override;
  void UpdateMouseSettings(const MouseSettings& settings) override;
  void SetMouseSensitivity(int value) override;
  void SetPrimaryButtonRight(bool right) override;
  void ReapplyTouchpadSettings() override;
  void ReapplyMouseSettings() override;
  InputDeviceSettings::FakeInterface* GetFakeInterface() override;
  void SetInternalTouchpadEnabled(bool enabled) override;
  void SetTouchscreensEnabled(bool enabled) override;

  // Generate arguments for the inputcontrol script.
  //
  // |argv| is filled with arguments of script, that should be launched in order
  // to apply update.
  void GenerateTouchpadArguments(std::vector<std::string>* argv);
  void GenerateMouseArguments(std::vector<std::string>* argv);

  TouchpadSettings current_touchpad_settings_;
  MouseSettings current_mouse_settings_;

  DISALLOW_COPY_AND_ASSIGN(InputDeviceSettingsImplX11);
};

InputDeviceSettingsImplX11::InputDeviceSettingsImplX11() {
}

void InputDeviceSettingsImplX11::TouchpadExists(
    const DeviceExistsCallback& callback) {
  DeviceExists(kDeviceTypeTouchpad, callback);
}

void InputDeviceSettingsImplX11::UpdateTouchpadSettings(
    const TouchpadSettings& settings) {
  std::vector<std::string> argv;
  if (current_touchpad_settings_.Update(settings)) {
    GenerateTouchpadArguments(&argv);
    ExecuteScript(argv);
  }
}

void InputDeviceSettingsImplX11::SetTouchpadSensitivity(int value) {
  TouchpadSettings settings;
  settings.SetSensitivity(value);
  UpdateTouchpadSettings(settings);
}

void InputDeviceSettingsImplX11::SetNaturalScroll(bool enabled) {
  TouchpadSettings settings;
  settings.SetNaturalScroll(enabled);
  UpdateTouchpadSettings(settings);
}

void InputDeviceSettingsImplX11::SetTapToClick(bool enabled) {
  TouchpadSettings settings;
  settings.SetTapToClick(enabled);
  UpdateTouchpadSettings(settings);
}

void InputDeviceSettingsImplX11::SetThreeFingerClick(bool enabled) {
  // For Alex/ZGB.
  TouchpadSettings settings;
  settings.SetThreeFingerClick(enabled);
  UpdateTouchpadSettings(settings);
}

void InputDeviceSettingsImplX11::SetTapDragging(bool enabled) {
  TouchpadSettings settings;
  settings.SetTapDragging(enabled);
  UpdateTouchpadSettings(settings);
}

void InputDeviceSettingsImplX11::MouseExists(
    const DeviceExistsCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DeviceExists(kDeviceTypeMouse, callback);
}

void InputDeviceSettingsImplX11::UpdateMouseSettings(
    const MouseSettings& update) {
  std::vector<std::string> argv;
  if (current_mouse_settings_.Update(update)) {
    GenerateMouseArguments(&argv);
    ExecuteScript(argv);
  }
}

void InputDeviceSettingsImplX11::SetMouseSensitivity(int value) {
  MouseSettings settings;
  settings.SetSensitivity(value);
  UpdateMouseSettings(settings);
}

void InputDeviceSettingsImplX11::SetPrimaryButtonRight(bool right) {
  MouseSettings settings;
  settings.SetPrimaryButtonRight(right);
  UpdateMouseSettings(settings);
}

void InputDeviceSettingsImplX11::ReapplyTouchpadSettings() {
  TouchpadSettings settings = current_touchpad_settings_;
  current_touchpad_settings_ = TouchpadSettings();
  UpdateTouchpadSettings(settings);
}

void InputDeviceSettingsImplX11::ReapplyMouseSettings() {
  MouseSettings settings = current_mouse_settings_;
  current_mouse_settings_ = MouseSettings();
  UpdateMouseSettings(settings);
}

void InputDeviceSettingsImplX11::SetInternalTouchpadEnabled(bool enabled) {
  ui::DeviceDataManagerX11* device_data_manager =
      ui::DeviceDataManagerX11::GetInstance();
  if (!device_data_manager->IsXInput2Available())
    return;

  const XIDeviceList& xi_dev_list =
      ui::DeviceListCacheX11::GetInstance()->GetXI2DeviceList(
          gfx::GetXDisplay());
  for (int i = 0; i < xi_dev_list.count; ++i) {
    std::string device_name(xi_dev_list[i].name);
    base::TrimWhitespaceASCII(device_name, base::TRIM_TRAILING, &device_name);
    if (device_name == kInternalTouchpadName) {
      if (enabled)
        device_data_manager->EnableDevice(xi_dev_list[i].deviceid);
      else
        device_data_manager->DisableDevice(xi_dev_list[i].deviceid);

      return;
    }
  }
}

InputDeviceSettings::FakeInterface*
InputDeviceSettingsImplX11::GetFakeInterface() {
  return nullptr;
}

void InputDeviceSettingsImplX11::SetTouchscreensEnabled(bool enabled) {
  ui::TouchFactory::GetInstance()->SetTouchscreensEnabled(enabled);
}

void InputDeviceSettingsImplX11::GenerateTouchpadArguments(
    std::vector<std::string>* argv) {
  argv->push_back(kInputControl);
  if (current_touchpad_settings_.IsSensitivitySet()) {
    AddSensitivityArguments(kDeviceTypeTouchpad,
                            current_touchpad_settings_.GetSensitivity(), argv);
  }
  if (current_touchpad_settings_.IsTapToClickSet()) {
    AddTPControlArguments("tapclick",
                          current_touchpad_settings_.GetTapToClick(), argv);
  }
  if (current_touchpad_settings_.IsThreeFingerClickSet()) {
    AddTPControlArguments("t5r2_three_finger_click",
                          current_touchpad_settings_.GetThreeFingerClick(),
                          argv);
  }
  if (current_touchpad_settings_.IsTapDraggingSet()) {
    AddTPControlArguments("tapdrag",
                          current_touchpad_settings_.GetTapDragging(), argv);
  }
  if (current_touchpad_settings_.IsNaturalScrollSet()) {
    AddTPControlArguments("australian_scrolling",
                          current_touchpad_settings_.GetNaturalScroll(), argv);
  }
}

void InputDeviceSettingsImplX11::GenerateMouseArguments(
    std::vector<std::string>* argv) {
  argv->push_back(kInputControl);
  if (current_mouse_settings_.IsSensitivitySet()) {
    AddSensitivityArguments(kDeviceTypeMouse,
                            current_mouse_settings_.GetSensitivity(), argv);
  }
  if (current_mouse_settings_.IsPrimaryButtonRightSet()) {
    AddTPControlArguments(
        "mouse_swap_lr", current_mouse_settings_.GetPrimaryButtonRight(), argv);
  }
}

}  // namespace

// static
InputDeviceSettings* InputDeviceSettings::Get() {
  if (!g_instance) {
    if (base::SysInfo::IsRunningOnChromeOS())
      g_instance = new InputDeviceSettingsImplX11;
    else
      g_instance = new FakeInputDeviceSettings;
  }
  return g_instance;
}

}  // namespace system
}  // namespace chromeos
