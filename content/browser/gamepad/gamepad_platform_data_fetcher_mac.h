// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_MAC_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_MAC_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/mac/scoped_cftyperef.h"
#include "build/build_config.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_standard_mappings.h"
#include "content/common/gamepad_hardware_buffer.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

#if defined(__OBJC__)
@class NSArray;
#else
class NSArray;
#endif

namespace content {

class GamepadPlatformDataFetcherMac : public GamepadDataFetcher {
 public:
  GamepadPlatformDataFetcherMac();
  virtual ~GamepadPlatformDataFetcherMac();
  virtual void GetGamepadData(WebKit::WebGamepads* pads,
                              bool devices_changed_hint) OVERRIDE;
  virtual void PauseHint(bool paused) OVERRIDE;

 private:
  bool enabled_;
  base::mac::ScopedCFTypeRef<IOHIDManagerRef> hid_manager_ref_;

  static GamepadPlatformDataFetcherMac* InstanceFromContext(void* context);
  static void DeviceAddCallback(void* context,
                                IOReturn result,
                                void* sender,
                                IOHIDDeviceRef ref);
  static void DeviceRemoveCallback(void* context,
                                   IOReturn result,
                                   void* sender,
                                   IOHIDDeviceRef ref);
  static void ValueChangedCallback(void* context,
                                   IOReturn result,
                                   void* sender,
                                   IOHIDValueRef ref);

  void DeviceAdd(IOHIDDeviceRef device);
  void AddButtonsAndAxes(NSArray* elements, size_t slot);
  void DeviceRemove(IOHIDDeviceRef device);
  void ValueChanged(IOHIDValueRef value);

  void RegisterForNotifications();
  void UnregisterFromNotifications();

  WebKit::WebGamepads data_;

  // Side-band data that's not passed to the consumer, but we need to maintain
  // to update data_.
  struct AssociatedData {
    IOHIDDeviceRef device_ref;
    IOHIDElementRef button_elements[WebKit::WebGamepad::buttonsLengthCap];
    IOHIDElementRef axis_elements[WebKit::WebGamepad::buttonsLengthCap];
    CFIndex axis_minimums[WebKit::WebGamepad::axesLengthCap];
    CFIndex axis_maximums[WebKit::WebGamepad::axesLengthCap];
    // Function to map from device data to standard layout, if available. May
    // be null if no mapping is available.
    GamepadStandardMappingFunction mapper;
  };
  AssociatedData associated_[WebKit::WebGamepads::itemsLengthCap];

  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_MAC_H_
