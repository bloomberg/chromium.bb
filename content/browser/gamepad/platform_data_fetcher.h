// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define the default data fetcher that GamepadProvider will use if none is
// supplied. (GamepadPlatformDataFetcher).

#ifndef CONTENT_BROWSER_GAMEPAD_PLATFORM_DATA_FETCHER_H_
#define CONTENT_BROWSER_GAMEPAD_PLATFORM_DATA_FETCHER_H_

#if defined(OS_WIN)
#include "content/browser/gamepad/platform_data_fetcher_win.h"
#elif defined(OS_MACOSX)
#include "content/browser/gamepad/platform_data_fetcher_mac.h"
#elif defined(OS_LINUX)
#include "content/browser/gamepad/platform_data_fetcher_linux.h"
#endif

namespace content {

#if defined(OS_WIN)

typedef GamepadPlatformDataFetcherWin GamepadPlatformDataFetcher;

#elif defined(OS_MACOSX)

typedef GamepadPlatformDataFetcherMac GamepadPlatformDataFetcher;

#elif defined(OS_LINUX)

typedef GamepadPlatformDataFetcherLinux GamepadPlatformDataFetcher;

#else

class GamepadDataFetcherEmpty : public GamepadDataFetcher {
 public:
  void GetGamepadData(WebKit::WebGamepads* pads, bool) {
    pads->length = 0;
  }
};
typedef GamepadDataFetcherEmpty GamepadPlatformDataFetcher;

#endif

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_PLATFORM_DATA_FETCHER_H_
