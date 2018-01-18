// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_
#define DEVICE_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_

#include <Unknwn.h>
#include <WinDef.h>
#include <hidsdi.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_native_library.h"
#include "base/win/message_window.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/hid_dll_functions_win.h"
#include "device/gamepad/public/cpp/gamepad.h"

namespace device {

struct RawGamepadAxis {
  HIDP_VALUE_CAPS caps;
  float value;
  bool active;
  unsigned long bitmask;
};

struct RawGamepadInfo {
  RawGamepadInfo();
  ~RawGamepadInfo();

  int source_id;
  int enumeration_id;
  HANDLE handle;
  std::unique_ptr<uint8_t[]> ppd_buffer;
  PHIDP_PREPARSED_DATA preparsed_data;

  uint32_t report_id;
  uint32_t vendor_id;
  uint32_t product_id;
  uint32_t version_number;

  wchar_t id[Gamepad::kIdLengthCap];

  uint32_t buttons_length;
  bool buttons[Gamepad::kButtonsLengthCap];

  uint32_t axes_length;
  RawGamepadAxis axes[Gamepad::kAxesLengthCap];
};

class RawInputDataFetcher : public GamepadDataFetcher,
                            public base::SupportsWeakPtr<RawInputDataFetcher> {
 public:
  typedef GamepadDataFetcherFactoryImpl<RawInputDataFetcher,
                                        GAMEPAD_SOURCE_WIN_RAW>
      Factory;

  explicit RawInputDataFetcher();
  ~RawInputDataFetcher() override;

  GamepadSource source() override;

  void GetGamepadData(bool devices_changed_hint) override;
  void PauseHint(bool paused) override;

 private:
  void OnAddedToProvider() override;

  void StartMonitor();
  void StopMonitor();
  void EnumerateDevices();
  RawGamepadInfo* ParseGamepadInfo(HANDLE hDevice);
  void UpdateGamepad(RAWINPUT* input, RawGamepadInfo* gamepad_info);
  // Handles WM_INPUT messages.
  LRESULT OnInput(HRAWINPUT input_handle);
  // Handles messages received by |window_|.
  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result);
  RAWINPUTDEVICE* GetRawInputDevices(DWORD flags);
  void ClearControllers();

  std::unique_ptr<base::win::MessageWindow> window_;
  bool rawinput_available_;
  bool filter_xinput_;
  bool events_monitored_;
  int last_source_id_;
  int last_enumeration_id_;

  typedef std::map<HANDLE, RawGamepadInfo*> ControllerMap;
  ControllerMap controllers_;

  // HID functions loaded from hid.dll.
  std::unique_ptr<HidDllFunctionsWin> hid_functions_;

  DISALLOW_COPY_AND_ASSIGN(RawInputDataFetcher);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_RAW_INPUT_DATA_FETCHER_WIN_H_
