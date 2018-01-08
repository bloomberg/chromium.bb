// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_platform_data_fetcher_linux.h"

#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "device/udev_linux/scoped_udev.h"
#include "device/udev_linux/udev_linux.h"

namespace {

const char kInputSubsystem[] = "input";

int DeviceIndexFromDevicePath(const std::string& path,
                              const std::string& prefix) {
  if (!base::StartsWith(path, prefix, base::CompareCase::SENSITIVE))
    return -1;

  int index = -1;
  base::StringPiece index_str(&path.c_str()[prefix.length()],
                              path.length() - prefix.length());
  if (!base::StringToInt(index_str, &index))
    return -1;

  return index;
}

bool IsUdevGamepad(udev_device* dev, device::UdevGamepad* pad_info) {
  using DeviceRootPair = std::pair<device::UdevGamepadType, const char*>;
  static const std::vector<DeviceRootPair> device_roots = {
      {device::UdevGamepadType::EVDEV, "/dev/input/event"},
      {device::UdevGamepadType::JOYDEV, "/dev/input/js"},
  };

  if (!dev)
    return false;

  if (!device::udev_device_get_property_value(dev, "ID_INPUT_JOYSTICK"))
    return false;

  const char* node_path = device::udev_device_get_devnode(dev);
  if (!node_path)
    return false;

  // The device pointed to by parent_dev contains information about the logical
  // joystick device. We can compare syspaths to determine when a joydev device
  // and an evdev device refer to the same physical device.
  udev_device* parent_dev =
      device::udev_device_get_parent_with_subsystem_devtype(
          dev, kInputSubsystem, nullptr);
  const char* parent_syspath =
      parent_dev ? device::udev_device_get_syspath(parent_dev) : "";

  for (const auto& entry : device_roots) {
    device::UdevGamepadType node_type = entry.first;
    const char* prefix = entry.second;
    int index_value = DeviceIndexFromDevicePath(node_path, prefix);

    if (index_value < 0)
      continue;

    if (pad_info) {
      pad_info->type = node_type;
      pad_info->index = index_value;
      pad_info->path = node_path;
      pad_info->parent_syspath = parent_syspath;
    }

    return true;
  }

  return false;
}

}  // namespace

namespace device {

GamepadPlatformDataFetcherLinux::GamepadPlatformDataFetcherLinux() = default;

GamepadPlatformDataFetcherLinux::~GamepadPlatformDataFetcherLinux() {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    GamepadDeviceLinux* device = it->get();
    device->Shutdown();
  }
}

GamepadSource GamepadPlatformDataFetcherLinux::source() {
  return Factory::static_source();
}

void GamepadPlatformDataFetcherLinux::OnAddedToProvider() {
  std::vector<UdevLinux::UdevMonitorFilter> filters;
  filters.push_back(UdevLinux::UdevMonitorFilter(kInputSubsystem, nullptr));
  udev_.reset(new UdevLinux(
      filters, base::Bind(&GamepadPlatformDataFetcherLinux::RefreshDevice,
                          base::Unretained(this))));

  EnumerateDevices();
}

void GamepadPlatformDataFetcherLinux::GetGamepadData(bool) {
  TRACE_EVENT0("GAMEPAD", "GetGamepadData");

  // Update our internal state.
  for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i)
    ReadDeviceData(i);
}

// Used during enumeration, and monitor notifications.
void GamepadPlatformDataFetcherLinux::RefreshDevice(udev_device* dev) {
  UdevGamepad pad_info;
  if (IsUdevGamepad(dev, &pad_info)) {
    if (pad_info.type == UdevGamepadType::JOYDEV)
      RefreshJoydevDevice(dev, pad_info);
    else if (pad_info.type == UdevGamepadType::EVDEV)
      RefreshEvdevDevice(dev, pad_info);
  }
}

GamepadDeviceLinux* GamepadPlatformDataFetcherLinux::GetDeviceWithJoydevIndex(
    int joydev_index) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    GamepadDeviceLinux* device = it->get();
    if (device->GetJoydevIndex() == joydev_index)
      return device;
  }

  return nullptr;
}

void GamepadPlatformDataFetcherLinux::RemoveDevice(GamepadDeviceLinux* device) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    if (it->get() == device) {
      device->Shutdown();
      devices_.erase(it);
      break;
    }
  }
}

GamepadDeviceLinux* GamepadPlatformDataFetcherLinux::GetOrCreateMatchingDevice(
    const UdevGamepad& pad_info) {
  if (pad_info.parent_syspath.empty())
    return nullptr;

  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    GamepadDeviceLinux* device = it->get();
    if (device->GetParentSyspath() == pad_info.parent_syspath)
      return device;
  }

  auto emplace_result = devices_.emplace(
      std::make_unique<GamepadDeviceLinux>(pad_info.parent_syspath));
  return emplace_result.first->get();
}

void GamepadPlatformDataFetcherLinux::RefreshJoydevDevice(
    udev_device* dev,
    const UdevGamepad& pad_info) {
  const int joydev_index = pad_info.index;
  if (joydev_index < 0 || joydev_index >= (int)Gamepads::kItemsLengthCap)
    return;

  GamepadDeviceLinux* device = GetOrCreateMatchingDevice(pad_info);
  if (device == nullptr)
    return;

  PadState* state = GetPadState(joydev_index);
  if (!state) {
    // No slot available for device, don't use.
    RemoveDevice(device);
    return;
  }

  // The device pointed to by dev contains information about the logical
  // joystick device. In order to get the information about the physical
  // hardware, get the parent device that is also in the "input" subsystem.
  // This function should just walk up the tree one level.
  udev_device* parent_dev =
      device::udev_device_get_parent_with_subsystem_devtype(
          dev, kInputSubsystem, nullptr);
  if (!parent_dev) {
    device->CloseJoydevNode();
    if (device->IsEmpty())
      RemoveDevice(device);
    return;
  }

  // If the device cannot be opened, the joystick has been disconnected.
  if (!device->OpenJoydevNode(pad_info.path, dev, joydev_index)) {
    if (device->IsEmpty())
      RemoveDevice(device);
    return;
  }

  std::string vendor_id = device->GetVendorId();
  std::string product_id = device->GetProductId();
  std::string version_number = device->GetVersionNumber();
  std::string name = device->GetName();

  GamepadStandardMappingFunction& mapper = state->mapper;
  mapper = GetGamepadStandardMappingFunction(
      vendor_id.c_str(), product_id.c_str(), version_number.c_str());

  Gamepad& pad = state->data;

  // Append the vendor and product information then convert the utf-8
  // id string to WebUChar.
  std::string id =
      name + base::StringPrintf(" (%sVendor: %s Product: %s)",
                                mapper ? "STANDARD GAMEPAD " : "",
                                vendor_id.c_str(), product_id.c_str());
  base::TruncateUTF8ToByteSize(id, Gamepad::kIdLengthCap - 1, &id);
  base::string16 tmp16 = base::UTF8ToUTF16(id);
  memset(pad.id, 0, sizeof(pad.id));
  tmp16.copy(pad.id, arraysize(pad.id) - 1);

  if (mapper) {
    std::string mapping = "standard";
    base::TruncateUTF8ToByteSize(mapping, Gamepad::kMappingLengthCap - 1,
                                 &mapping);
    tmp16 = base::UTF8ToUTF16(mapping);
    memset(pad.mapping, 0, sizeof(pad.mapping));
    tmp16.copy(pad.mapping, arraysize(pad.mapping) - 1);
  } else {
    pad.mapping[0] = 0;
  }

  pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
  pad.vibration_actuator.not_null = device->SupportsVibration();

  pad.connected = true;
}

void GamepadPlatformDataFetcherLinux::RefreshEvdevDevice(
    udev_device* dev,
    const UdevGamepad& pad_info) {
  GamepadDeviceLinux* device = GetOrCreateMatchingDevice(pad_info);
  if (device == nullptr)
    return;

  if (!device->OpenEvdevNode(pad_info.path)) {
    if (device->IsEmpty())
      RemoveDevice(device);
    return;
  }

  int joydev_index = device->GetJoydevIndex();
  if (joydev_index >= 0) {
    PadState* state = GetPadState(joydev_index);
    DCHECK(state);
    if (state) {
      Gamepad& pad = state->data;
      pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
      pad.vibration_actuator.not_null = device->SupportsVibration();
    }
  }
}

void GamepadPlatformDataFetcherLinux::EnumerateDevices() {
  if (!udev_->udev_handle())
    return;
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev_->udev_handle()));
  if (!enumerate)
    return;
  int ret =
      udev_enumerate_add_match_subsystem(enumerate.get(), kInputSubsystem);
  if (ret != 0)
    return;
  ret = udev_enumerate_scan_devices(enumerate.get());
  if (ret != 0)
    return;

  devices_.clear();

  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate.get());
  for (udev_list_entry* dev_list_entry = devices; dev_list_entry != nullptr;
       dev_list_entry = udev_list_entry_get_next(dev_list_entry)) {
    // Get the filename of the /sys entry for the device and create a
    // udev_device object (dev) representing it
    const char* path = udev_list_entry_get_name(dev_list_entry);
    ScopedUdevDevicePtr dev(
        udev_device_new_from_syspath(udev_->udev_handle(), path));
    if (!dev)
      continue;
    RefreshDevice(dev.get());
  }
}

void GamepadPlatformDataFetcherLinux::ReadDeviceData(size_t index) {
  // Linker does not like CHECK_LT(index, Gamepads::kItemsLengthCap). =/
  if (index >= Gamepads::kItemsLengthCap) {
    CHECK(false);
    return;
  }

  GamepadDeviceLinux* device = GetDeviceWithJoydevIndex(index);
  if (!device)
    return;

  PadState* state = GetPadState(index);
  if (!state)
    return;

  device->ReadPadState(&state->data);
}

void GamepadPlatformDataFetcherLinux::PlayEffect(
    int pad_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback) {
  if (pad_id < 0 || pad_id >= static_cast<int>(Gamepads::kItemsLengthCap)) {
    std::move(callback).Run(
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  GamepadDeviceLinux* device = GetDeviceWithJoydevIndex(pad_id);
  if (!device) {
    std::move(callback).Run(
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  device->PlayEffect(type, std::move(params), std::move(callback));
}

void GamepadPlatformDataFetcherLinux::ResetVibration(
    int pad_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback) {
  if (pad_id < 0 || pad_id >= static_cast<int>(Gamepads::kItemsLengthCap)) {
    std::move(callback).Run(
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  GamepadDeviceLinux* device = GetDeviceWithJoydevIndex(pad_id);
  if (!device) {
    std::move(callback).Run(
        mojom::GamepadHapticsResult::GamepadHapticsResultError);
    return;
  }

  device->ResetVibration(std::move(callback));
}

}  // namespace device
