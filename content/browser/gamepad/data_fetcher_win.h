// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_DATA_FETCHER_WIN_H_
#define CONTENT_BROWSER_GAMEPAD_DATA_FETCHER_WIN_H_

#include "build/build_config.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <XInput.h>

#include "base/compiler_specific.h"
#include "content/browser/gamepad/data_fetcher.h"

namespace content {

class GamepadDataFetcherWindows : public GamepadDataFetcher {
 public:
  GamepadDataFetcherWindows();
  virtual void GetGamepadData(WebKit::WebGamepads* pads,
                              bool devices_changed_hint) OVERRIDE;
 private:
  bool xinput_available_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_DATA_FETCHER_WIN_H_
