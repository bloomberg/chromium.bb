// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"

extern "C" {
struct udev_device;
}

namespace device {
class UdevLinux;
}

namespace content {

class GamepadPlatformDataFetcherLinux : public GamepadDataFetcher {
 public:
  GamepadPlatformDataFetcherLinux();
  ~GamepadPlatformDataFetcherLinux() override;

  // GamepadDataFetcher implementation.
  void GetGamepadData(blink::WebGamepads* pads,
                      bool devices_changed_hint) override;

 private:
  void RefreshDevice(udev_device* dev);
  void EnumerateDevices();
  void ReadDeviceData(size_t index);

  // File descriptor for the /dev/input/js* devices. -1 if not in use.
  int device_fd_[blink::WebGamepads::itemsLengthCap];

  scoped_ptr<device::UdevLinux> udev_;

  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherLinux);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_
