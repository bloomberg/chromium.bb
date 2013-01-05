// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_platform_data_fetcher.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebGamepads.h"

namespace content {

GamepadDataFetcherEmpty::GamepadDataFetcherEmpty() {
}

void GamepadDataFetcherEmpty::GetGamepadData(WebKit::WebGamepads* pads,
                                             bool devices_changed_hint) {
  pads->length = 0;
}

}  // namespace content
