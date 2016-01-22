// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define the default data fetcher that GamepadProvider will use if none is
// supplied. (GamepadPlatformDataFetcher).

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_provider.h"

#if defined(OS_ANDROID)
#include "content/browser/gamepad/gamepad_platform_data_fetcher_android.h"
#elif defined(OS_WIN)
#include "content/browser/gamepad/gamepad_platform_data_fetcher_win.h"
#include "content/browser/gamepad/raw_input_data_fetcher_win.h"
#elif defined(OS_MACOSX)
#include "content/browser/gamepad/gamepad_platform_data_fetcher_mac.h"
#include "content/browser/gamepad/xbox_data_fetcher_mac.h"
#elif defined(OS_LINUX)
#include "content/browser/gamepad/gamepad_platform_data_fetcher_linux.h"
#endif

namespace content {

void AddGamepadPlatformDataFetchers(GamepadProvider* provider) {
#if defined(OS_ANDROID)

  provider->AddGamepadDataFetcher(scoped_ptr<GamepadDataFetcher>(
      new GamepadPlatformDataFetcherAndroid()));

#elif defined(OS_WIN)

  provider->AddGamepadDataFetcher(scoped_ptr<GamepadDataFetcher>(
      new GamepadPlatformDataFetcherWin()));
  provider->AddGamepadDataFetcher(scoped_ptr<GamepadDataFetcher>(
      new RawInputDataFetcher()));

#elif defined(OS_MACOSX)

  provider->AddGamepadDataFetcher(scoped_ptr<GamepadDataFetcher>(
      new GamepadPlatformDataFetcherMac()));
  provider->AddGamepadDataFetcher(scoped_ptr<GamepadDataFetcher>(
      new XboxDataFetcher()));

#elif defined(OS_LINUX) && defined(USE_UDEV)

  provider->AddGamepadDataFetcher(scoped_ptr<GamepadDataFetcher>(
      new GamepadPlatformDataFetcherLinux()));

#endif
}

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_H_
