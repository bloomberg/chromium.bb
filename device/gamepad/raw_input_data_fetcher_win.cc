// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/raw_input_data_fetcher_win.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"

namespace device {

namespace {

float NormalizeAxis(long value, long min, long max) {
  return (2.f * (value - min) / static_cast<float>(max - min)) - 1.f;
}

unsigned long GetBitmask(unsigned short bits) {
  return (1 << bits) - 1;
}

// From the HID Usage Tables specification.
USHORT DeviceUsages[] = {
    0x04,  // Joysticks
    0x05,  // Gamepads
    0x08,  // Multi Axis
};

const uint32_t kAxisMinimumUsageNumber = 0x30;
const uint32_t kGameControlsUsagePage = 0x05;
const uint32_t kButtonUsagePage = 0x09;

const uint32_t kVendorOculus = 0x2833;
const uint32_t kVendorBlue = 0xb58e;

}  // namespace

RawGamepadInfo::RawGamepadInfo() {}

RawGamepadInfo::~RawGamepadInfo() {}

RawInputDataFetcher::RawInputDataFetcher()
    : rawinput_available_(false),
      filter_xinput_(true),
      events_monitored_(false),
      last_source_id_(0),
      last_enumeration_id_(0) {}

RawInputDataFetcher::~RawInputDataFetcher() {
  StopMonitor();
  ClearControllers();
  DCHECK(!window_);
  DCHECK(!events_monitored_);
}

GamepadSource RawInputDataFetcher::source() {
  return Factory::static_source();
}

void RawInputDataFetcher::OnAddedToProvider() {
  hid_dll_.Reset(base::LoadNativeLibrary(
      base::FilePath(FILE_PATH_LITERAL("hid.dll")), nullptr));
  rawinput_available_ = GetHidDllFunctions();
}

RAWINPUTDEVICE* RawInputDataFetcher::GetRawInputDevices(DWORD flags) {
  size_t usage_count = arraysize(DeviceUsages);
  std::unique_ptr<RAWINPUTDEVICE[]> devices(new RAWINPUTDEVICE[usage_count]);
  for (size_t i = 0; i < usage_count; ++i) {
    devices[i].dwFlags = flags;
    devices[i].usUsagePage = 1;
    devices[i].usUsage = DeviceUsages[i];
    devices[i].hwndTarget = (flags & RIDEV_REMOVE) ? 0 : window_->hwnd();
  }
  return devices.release();
}

void RawInputDataFetcher::PauseHint(bool pause) {
  if (pause)
    StopMonitor();
  else
    StartMonitor();
}

void RawInputDataFetcher::StartMonitor() {
  if (!rawinput_available_ || events_monitored_)
    return;

  if (!window_) {
    window_.reset(new base::win::MessageWindow());
    if (!window_->Create(base::Bind(&RawInputDataFetcher::HandleMessage,
                                    base::Unretained(this)))) {
      PLOG(ERROR) << "Failed to create the raw input window";
      window_.reset();
      return;
    }
  }

  // Register to receive raw HID input.
  std::unique_ptr<RAWINPUTDEVICE[]> devices(
      GetRawInputDevices(RIDEV_INPUTSINK));
  if (!RegisterRawInputDevices(devices.get(), arraysize(DeviceUsages),
                               sizeof(RAWINPUTDEVICE))) {
    PLOG(ERROR) << "RegisterRawInputDevices() failed for RIDEV_INPUTSINK";
    window_.reset();
    return;
  }

  events_monitored_ = true;
}

void RawInputDataFetcher::StopMonitor() {
  if (!rawinput_available_ || !events_monitored_)
    return;

  // Stop receiving raw input.
  DCHECK(window_);
  std::unique_ptr<RAWINPUTDEVICE[]> devices(GetRawInputDevices(RIDEV_REMOVE));

  if (!RegisterRawInputDevices(devices.get(), arraysize(DeviceUsages),
                               sizeof(RAWINPUTDEVICE))) {
    PLOG(INFO) << "RegisterRawInputDevices() failed for RIDEV_REMOVE";
  }

  events_monitored_ = false;
  window_.reset();
}

void RawInputDataFetcher::ClearControllers() {
  while (!controllers_.empty()) {
    RawGamepadInfo* gamepad_info = controllers_.begin()->second;
    controllers_.erase(gamepad_info->handle);
    delete gamepad_info;
  }
}

void RawInputDataFetcher::GetGamepadData(bool devices_changed_hint) {
  if (!rawinput_available_)
    return;

  if (devices_changed_hint)
    EnumerateDevices();

  for (const auto& controller : controllers_) {
    RawGamepadInfo* gamepad = controller.second;
    PadState* state = GetPadState(gamepad->source_id);
    if (!state)
      continue;

    Gamepad& pad = state->data;

    pad.timestamp = gamepad->report_id;
    pad.buttons_length = gamepad->buttons_length;
    pad.axes_length = gamepad->axes_length;

    for (unsigned int i = 0; i < pad.buttons_length; i++) {
      pad.buttons[i].pressed = gamepad->buttons[i];
      pad.buttons[i].value = gamepad->buttons[i] ? 1.0 : 0.0;
    }

    for (unsigned int i = 0; i < pad.axes_length; i++)
      pad.axes[i] = gamepad->axes[i].value;
  }
}

void RawInputDataFetcher::EnumerateDevices() {
  last_enumeration_id_++;

  UINT count = 0;
  UINT result = GetRawInputDeviceList(NULL, &count, sizeof(RAWINPUTDEVICELIST));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceList() failed";
    return;
  }
  DCHECK_EQ(0u, result);

  std::unique_ptr<RAWINPUTDEVICELIST[]> device_list(
      new RAWINPUTDEVICELIST[count]);
  result = GetRawInputDeviceList(device_list.get(), &count,
                                 sizeof(RAWINPUTDEVICELIST));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceList() failed";
    return;
  }
  DCHECK_EQ(count, result);

  for (UINT i = 0; i < count; ++i) {
    if (device_list[i].dwType == RIM_TYPEHID) {
      HANDLE device_handle = device_list[i].hDevice;
      ControllerMap::iterator controller = controllers_.find(device_handle);

      RawGamepadInfo* gamepad;
      if (controller != controllers_.end()) {
        gamepad = controller->second;
      } else {
        gamepad = ParseGamepadInfo(device_handle);
        if (!gamepad)
          continue;

        PadState* state = GetPadState(gamepad->source_id);
        if (!state)
          continue;  // No slot available for this gamepad.

        controllers_[device_handle] = gamepad;

        Gamepad& pad = state->data;
        pad.connected = true;

        std::string vendor = base::StringPrintf("%04x", gamepad->vendor_id);
        std::string product = base::StringPrintf("%04x", gamepad->product_id);
        std::string version =
            base::StringPrintf("%04x", gamepad->version_number);
        state->mapper =
            GetGamepadStandardMappingFunction(vendor, product, version);
        state->axis_mask = 0;
        state->button_mask = 0;

        swprintf(pad.id, Gamepad::kIdLengthCap,
                 L"%ls (%lsVendor: %04x Product: %04x)", gamepad->id,
                 state->mapper ? L"STANDARD GAMEPAD " : L"", gamepad->vendor_id,
                 gamepad->product_id);

        if (state->mapper)
          swprintf(pad.mapping, Gamepad::kMappingLengthCap, L"standard");
        else
          pad.mapping[0] = 0;
      }

      gamepad->enumeration_id = last_enumeration_id_;
    }
  }

  // Clear out old controllers that weren't part of this enumeration pass.
  auto controller_it = controllers_.begin();
  while (controller_it != controllers_.end()) {
    RawGamepadInfo* gamepad = controller_it->second;
    if (gamepad->enumeration_id != last_enumeration_id_) {
      controller_it = controllers_.erase(controller_it);
      delete gamepad;
    } else {
      ++controller_it;
    }
  }
}

RawGamepadInfo* RawInputDataFetcher::ParseGamepadInfo(HANDLE hDevice) {
  UINT size = 0;

  // Query basic device info.
  UINT result = GetRawInputDeviceInfo(hDevice, RIDI_DEVICEINFO, NULL, &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return NULL;
  }
  DCHECK_EQ(0u, result);

  std::unique_ptr<uint8_t[]> di_buffer(new uint8_t[size]);
  RID_DEVICE_INFO* device_info =
      reinterpret_cast<RID_DEVICE_INFO*>(di_buffer.get());
  result =
      GetRawInputDeviceInfo(hDevice, RIDI_DEVICEINFO, di_buffer.get(), &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return NULL;
  }
  DCHECK_EQ(size, result);

  // Make sure this device is of a type that we want to observe.
  bool valid_type = false;
  for (USHORT device_usage : DeviceUsages) {
    if (device_info->hid.usUsage == device_usage) {
      valid_type = true;
      break;
    }
  }

  // This is terrible, but the Oculus Rift and some Blue Yeti microphones seem
  // to think they are gamepads. Filter out any such devices. Oculus Touch is
  // handled elsewhere.
  if (device_info->hid.dwVendorId == kVendorOculus ||
      device_info->hid.dwVendorId == kVendorBlue) {
    valid_type = false;
  }

  if (!valid_type)
    return NULL;

  std::unique_ptr<RawGamepadInfo> gamepad_info(new RawGamepadInfo);
  gamepad_info->source_id = ++last_source_id_;
  gamepad_info->handle = hDevice;
  gamepad_info->report_id = 0;
  gamepad_info->vendor_id = device_info->hid.dwVendorId;
  gamepad_info->product_id = device_info->hid.dwProductId;
  gamepad_info->version_number = device_info->hid.dwVersionNumber;
  gamepad_info->buttons_length = 0;
  ZeroMemory(gamepad_info->buttons, sizeof(gamepad_info->buttons));
  gamepad_info->axes_length = 0;
  ZeroMemory(gamepad_info->axes, sizeof(gamepad_info->axes));

  // Query device identifier
  result = GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME, NULL, &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return NULL;
  }
  DCHECK_EQ(0u, result);

  std::unique_ptr<wchar_t[]> name_buffer(new wchar_t[size]);
  result =
      GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME, name_buffer.get(), &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return NULL;
  }
  DCHECK_EQ(size, result);

  // The presence of "IG_" in the device name indicates that this is an XInput
  // Gamepad. Skip enumerating these devices and let the XInput path handle it.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ee417014.aspx
  if (filter_xinput_ && wcsstr(name_buffer.get(), L"IG_"))
    return NULL;

  // Get a friendly device name
  BOOLEAN got_product_string = FALSE;
  HANDLE hid_handle = CreateFile(
      name_buffer.get(), GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
  if (hid_handle) {
    got_product_string = hidd_get_product_string_(hid_handle, gamepad_info->id,
                                                  sizeof(gamepad_info->id));
    CloseHandle(hid_handle);
  }

  if (!got_product_string)
    swprintf(gamepad_info->id, Gamepad::kIdLengthCap, L"Unknown Gamepad");

  // Query device capabilities.
  result = GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA, NULL, &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return NULL;
  }
  DCHECK_EQ(0u, result);

  gamepad_info->ppd_buffer.reset(new uint8_t[size]);
  gamepad_info->preparsed_data =
      reinterpret_cast<PHIDP_PREPARSED_DATA>(gamepad_info->ppd_buffer.get());
  result = GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA,
                                 gamepad_info->ppd_buffer.get(), &size);
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputDeviceInfo() failed";
    return NULL;
  }
  DCHECK_EQ(size, result);

  HIDP_CAPS caps;
  NTSTATUS status = hidp_get_caps_(gamepad_info->preparsed_data, &caps);
  DCHECK_EQ(HIDP_STATUS_SUCCESS, status);

  // Query button information.
  USHORT count = caps.NumberInputButtonCaps;
  if (count > 0) {
    std::unique_ptr<HIDP_BUTTON_CAPS[]> button_caps(
        new HIDP_BUTTON_CAPS[count]);
    status = hidp_get_button_caps_(HidP_Input, button_caps.get(), &count,
                                   gamepad_info->preparsed_data);
    DCHECK_EQ(HIDP_STATUS_SUCCESS, status);

    for (uint32_t i = 0; i < count; ++i) {
      if (button_caps[i].Range.UsageMin <= Gamepad::kButtonsLengthCap &&
          button_caps[i].UsagePage == kButtonUsagePage) {
        uint32_t max_index =
            std::min(Gamepad::kButtonsLengthCap,
                     static_cast<size_t>(button_caps[i].Range.UsageMax));
        gamepad_info->buttons_length =
            std::max(gamepad_info->buttons_length, max_index);
      }
    }
  }

  // Query axis information.
  count = caps.NumberInputValueCaps;
  std::unique_ptr<HIDP_VALUE_CAPS[]> axes_caps(new HIDP_VALUE_CAPS[count]);
  status = hidp_get_value_caps_(HidP_Input, axes_caps.get(), &count,
                                gamepad_info->preparsed_data);

  bool mapped_all_axes = true;

  for (UINT i = 0; i < count; i++) {
    uint32_t axis_index = axes_caps[i].Range.UsageMin - kAxisMinimumUsageNumber;
    if (axis_index < Gamepad::kAxesLengthCap) {
      gamepad_info->axes[axis_index].caps = axes_caps[i];
      gamepad_info->axes[axis_index].value = 0;
      gamepad_info->axes[axis_index].active = true;
      gamepad_info->axes[axis_index].bitmask = GetBitmask(axes_caps[i].BitSize);
      gamepad_info->axes_length =
          std::max(gamepad_info->axes_length, axis_index + 1);
    } else {
      mapped_all_axes = false;
    }
  }

  if (!mapped_all_axes) {
    // For axes who's usage puts them outside the standard axesLengthCap range.
    uint32_t next_index = 0;
    for (UINT i = 0; i < count; i++) {
      uint32_t usage = axes_caps[i].Range.UsageMin - kAxisMinimumUsageNumber;
      if (usage >= Gamepad::kAxesLengthCap &&
          axes_caps[i].UsagePage <= kGameControlsUsagePage) {
        for (; next_index < Gamepad::kAxesLengthCap; ++next_index) {
          if (!gamepad_info->axes[next_index].active)
            break;
        }
        if (next_index < Gamepad::kAxesLengthCap) {
          gamepad_info->axes[next_index].caps = axes_caps[i];
          gamepad_info->axes[next_index].value = 0;
          gamepad_info->axes[next_index].active = true;
          gamepad_info->axes[next_index].bitmask =
              GetBitmask(axes_caps[i].BitSize);
          gamepad_info->axes_length =
              std::max(gamepad_info->axes_length, next_index + 1);
        }
      }

      if (next_index >= Gamepad::kAxesLengthCap)
        break;
    }
  }

  // Sometimes devices show up with no buttons or axes. Don't return these.
  if (gamepad_info->buttons_length == 0 && gamepad_info->axes_length == 0)
    return nullptr;

  return gamepad_info.release();
}

void RawInputDataFetcher::UpdateGamepad(RAWINPUT* input,
                                        RawGamepadInfo* gamepad_info) {
  NTSTATUS status;

  gamepad_info->report_id++;

  // Query button state.
  if (gamepad_info->buttons_length) {
    // Clear the button state
    ZeroMemory(gamepad_info->buttons, sizeof(gamepad_info->buttons));
    ULONG buttons_length = 0;

    hidp_get_usages_ex_(HidP_Input, 0, NULL, &buttons_length,
                        gamepad_info->preparsed_data,
                        reinterpret_cast<PCHAR>(input->data.hid.bRawData),
                        input->data.hid.dwSizeHid);

    std::unique_ptr<USAGE_AND_PAGE[]> usages(
        new USAGE_AND_PAGE[buttons_length]);
    status =
        hidp_get_usages_ex_(HidP_Input, 0, usages.get(), &buttons_length,
                            gamepad_info->preparsed_data,
                            reinterpret_cast<PCHAR>(input->data.hid.bRawData),
                            input->data.hid.dwSizeHid);

    if (status == HIDP_STATUS_SUCCESS) {
      // Set each reported button to true.
      for (uint32_t j = 0; j < buttons_length; j++) {
        int32_t button_index = usages[j].Usage - 1;
        if (usages[j].UsagePage == kButtonUsagePage && button_index >= 0 &&
            button_index < static_cast<int>(Gamepad::kButtonsLengthCap)) {
          gamepad_info->buttons[button_index] = true;
        }
      }
    }
  }

  // Query axis state.
  ULONG axis_value = 0;
  LONG scaled_axis_value = 0;
  for (uint32_t i = 0; i < gamepad_info->axes_length; i++) {
    RawGamepadAxis* axis = &gamepad_info->axes[i];

    // If the min is < 0 we have to query the scaled value, otherwise we need
    // the normal unscaled value.
    if (axis->caps.LogicalMin < 0) {
      status = hidp_get_scaled_usage_value_(
          HidP_Input, axis->caps.UsagePage, 0, axis->caps.Range.UsageMin,
          &scaled_axis_value, gamepad_info->preparsed_data,
          reinterpret_cast<PCHAR>(input->data.hid.bRawData),
          input->data.hid.dwSizeHid);
      if (status == HIDP_STATUS_SUCCESS) {
        axis->value = NormalizeAxis(scaled_axis_value, axis->caps.PhysicalMin,
                                    axis->caps.PhysicalMax);
      }
    } else {
      status = hidp_get_usage_value_(
          HidP_Input, axis->caps.UsagePage, 0, axis->caps.Range.UsageMin,
          &axis_value, gamepad_info->preparsed_data,
          reinterpret_cast<PCHAR>(input->data.hid.bRawData),
          input->data.hid.dwSizeHid);
      if (status == HIDP_STATUS_SUCCESS) {
        axis->value = NormalizeAxis(axis_value & axis->bitmask,
                                    axis->caps.LogicalMin & axis->bitmask,
                                    axis->caps.LogicalMax & axis->bitmask);
      }
    }
  }
}

LRESULT RawInputDataFetcher::OnInput(HRAWINPUT input_handle) {
  // Get the size of the input record.
  UINT size = 0;
  UINT result = GetRawInputData(input_handle, RID_INPUT, NULL, &size,
                                sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }
  DCHECK_EQ(0u, result);

  // Retrieve the input record.
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
  RAWINPUT* input = reinterpret_cast<RAWINPUT*>(buffer.get());
  result = GetRawInputData(input_handle, RID_INPUT, buffer.get(), &size,
                           sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }
  DCHECK_EQ(size, result);

  // Notify the observer about events generated locally.
  if (input->header.dwType == RIM_TYPEHID && input->header.hDevice != NULL) {
    ControllerMap::iterator it = controllers_.find(input->header.hDevice);
    if (it != controllers_.end())
      UpdateGamepad(input, it->second);
  }

  return DefRawInputProc(&input, 1, sizeof(RAWINPUTHEADER));
}

bool RawInputDataFetcher::HandleMessage(UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam,
                                        LRESULT* result) {
  switch (message) {
    case WM_INPUT:
      *result = OnInput(reinterpret_cast<HRAWINPUT>(lparam));
      return true;

    default:
      return false;
  }
}

bool RawInputDataFetcher::GetHidDllFunctions() {
  hidp_get_caps_ = NULL;
  hidp_get_button_caps_ = NULL;
  hidp_get_value_caps_ = NULL;
  hidp_get_usages_ex_ = NULL;
  hidp_get_usage_value_ = NULL;
  hidp_get_scaled_usage_value_ = NULL;
  hidd_get_product_string_ = NULL;

  if (!hid_dll_.is_valid())
    return false;

  hidp_get_caps_ = reinterpret_cast<HidPGetCapsFunc>(
      hid_dll_.GetFunctionPointer("HidP_GetCaps"));
  if (!hidp_get_caps_)
    return false;
  hidp_get_button_caps_ = reinterpret_cast<HidPGetButtonCapsFunc>(
      hid_dll_.GetFunctionPointer("HidP_GetButtonCaps"));
  if (!hidp_get_button_caps_)
    return false;
  hidp_get_value_caps_ = reinterpret_cast<HidPGetValueCapsFunc>(
      hid_dll_.GetFunctionPointer("HidP_GetValueCaps"));
  if (!hidp_get_value_caps_)
    return false;
  hidp_get_usages_ex_ = reinterpret_cast<HidPGetUsagesExFunc>(
      hid_dll_.GetFunctionPointer("HidP_GetUsagesEx"));
  if (!hidp_get_usages_ex_)
    return false;
  hidp_get_usage_value_ = reinterpret_cast<HidPGetUsageValueFunc>(
      hid_dll_.GetFunctionPointer("HidP_GetUsageValue"));
  if (!hidp_get_usage_value_)
    return false;
  hidp_get_scaled_usage_value_ = reinterpret_cast<HidPGetScaledUsageValueFunc>(
      hid_dll_.GetFunctionPointer("HidP_GetScaledUsageValue"));
  if (!hidp_get_scaled_usage_value_)
    return false;
  hidd_get_product_string_ = reinterpret_cast<HidDGetStringFunc>(
      hid_dll_.GetFunctionPointer("HidD_GetProductString"));
  if (!hidd_get_product_string_)
    return false;

  return true;
}

}  // namespace device
