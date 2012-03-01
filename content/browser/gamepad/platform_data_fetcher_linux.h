// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_
#define CONTENT_BROWSER_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_pump_libevent.h"
#include "build/build_config.h"
#include "content/browser/gamepad/data_fetcher.h"
#include "content/browser/gamepad/gamepad_standard_mappings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGamepads.h"

extern "C" {
struct udev;
struct udev_device;
struct udev_monitor;
}

namespace content {

class GamepadPlatformDataFetcherLinux :
    public GamepadDataFetcher,
    public base::MessagePumpLibevent::Watcher {
 public:
  GamepadPlatformDataFetcherLinux();
  virtual ~GamepadPlatformDataFetcherLinux();

  // GamepadDataFetcher:
  virtual void GetGamepadData(WebKit::WebGamepads* pads,
                              bool devices_changed_hint) OVERRIDE;

  // base::MessagePump:Libevent::Watcher:
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

 private:
  void RefreshDevice(udev_device* dev);
  void EnumerateDevices();
  void ReadDeviceData(size_t index);

  // libudev-related items, the main context, and the monitoring context to be
  // notified about changes to device states.
  udev* udev_;
  udev_monitor* monitor_;
  int monitor_fd_;
  base::MessagePumpLibevent::FileDescriptorWatcher monitor_watcher_;

  // File descriptors for the /dev/input/js* devices. -1 if not in use.
  int device_fds_[WebKit::WebGamepads::itemsLengthCap];

  // Functions to map from device data to standard layout, if available. May
  // be null if no mapping is available.
  GamepadStandardMappingFunction mappers_[WebKit::WebGamepads::itemsLengthCap];

  // Data that's returned to the consumer.
  WebKit::WebGamepads data_;

  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherLinux);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_
