// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_DATA_FETCHER_H_
#define CONTENT_BROWSER_GAMEPAD_DATA_FETCHER_H_

namespace WebKit {
class WebGamepads;
}

namespace gamepad {

class DataFetcher {
 public:
  virtual ~DataFetcher() {}
  virtual void GetGamepadData(WebKit::WebGamepads* pads,
                              bool devices_changed_hint) = 0;
};

} // namespace gamepad

#endif  // CONTENT_BROWSER_GAMEPAD_DATA_FETCHER_H_
