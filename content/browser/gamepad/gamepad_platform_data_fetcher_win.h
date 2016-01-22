// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_WIN_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_WIN_H_

#include "build/build_config.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <stdlib.h>
#include <Unknwn.h>
#include <WinDef.h>
#include <windows.h>
#include <XInput.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/scoped_native_library.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_standard_mappings.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

class GamepadPlatformDataFetcherWin : public GamepadDataFetcher {
 public:
  GamepadPlatformDataFetcherWin();
  ~GamepadPlatformDataFetcherWin() override;
  void GetGamepadData(bool devices_changed_hint) override;

 private:
  void OnAddedToProvider() override;

  // The three function types we use from xinput1_3.dll.
  typedef void (WINAPI *XInputEnableFunc)(BOOL enable);
  typedef DWORD (WINAPI *XInputGetCapabilitiesFunc)(
    DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities);
  typedef DWORD (WINAPI *XInputGetStateFunc)(
      DWORD dwUserIndex, XINPUT_STATE* pState);

  // Get functions from dynamically loaded xinput1_3.dll. We don't use
  // DELAYLOAD because the import library for Win8 SDK pulls xinput1_4 which
  // isn't redistributable. Returns true if loading was successful. We include
  // xinput1_3.dll with Chrome.
  bool GetXInputDllFunctions();

  // Scan for connected XInput and DirectInput gamepads.
  void EnumerateDevices();
  void GetXInputPadData(int i);

  base::ScopedNativeLibrary xinput_dll_;
  bool xinput_available_;

  // Function pointers to XInput functionality, retrieved in
  // |GetXinputDllFunctions|.
  XInputEnableFunc xinput_enable_;
  XInputGetCapabilitiesFunc xinput_get_capabilities_;
  XInputGetStateFunc xinput_get_state_;

  bool xinput_connected_[XUSER_MAX_COUNT];

  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherWin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_WIN_H_
