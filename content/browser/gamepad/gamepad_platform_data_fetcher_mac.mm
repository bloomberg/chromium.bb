// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_platform_data_fetcher_mac.h"

#include "base/mac/foundation_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

#include <IOKit/hid/IOHIDKeys.h>
#import <Foundation/Foundation.h>

namespace content {

namespace {

NSDictionary* DeviceMatching(uint32_t usage_page, uint32_t usage) {
  return [NSDictionary dictionaryWithObjectsAndKeys:
      [NSNumber numberWithUnsignedInt:usage_page],
          base::mac::CFToNSCast(CFSTR(kIOHIDDeviceUsagePageKey)),
      [NSNumber numberWithUnsignedInt:usage],
          base::mac::CFToNSCast(CFSTR(kIOHIDDeviceUsageKey)),
      nil];
}

float NormalizeAxis(CFIndex value, CFIndex min, CFIndex max) {
  return (2.f * (value - min) / static_cast<float>(max - min)) - 1.f;
}

// http://www.usb.org/developers/hidpage
const uint32_t kGenericDesktopUsagePage = 0x01;
const uint32_t kButtonUsagePage = 0x09;
const uint32_t kJoystickUsageNumber = 0x04;
const uint32_t kGameUsageNumber = 0x05;
const uint32_t kMultiAxisUsageNumber = 0x08;
const uint32_t kAxisMinimumUsageNumber = 0x30;
const uint32_t kAxisMaximumUsageNumber = 0x35;

}  // namespace

GamepadPlatformDataFetcherMac::GamepadPlatformDataFetcherMac()
    : enabled_(true) {
  memset(associated_, 0, sizeof(associated_));
  hid_manager_ref_.reset(IOHIDManagerCreate(kCFAllocatorDefault,
                                            kIOHIDOptionsTypeNone));
  if (CFGetTypeID(hid_manager_ref_) != IOHIDManagerGetTypeID()) {
    enabled_ = false;
    return;
  }

  scoped_nsobject<NSArray> criteria([[NSArray alloc] initWithObjects:
      DeviceMatching(kGenericDesktopUsagePage, kJoystickUsageNumber),
      DeviceMatching(kGenericDesktopUsagePage, kGameUsageNumber),
      DeviceMatching(kGenericDesktopUsagePage, kMultiAxisUsageNumber),
      nil]);
  IOHIDManagerSetDeviceMatchingMultiple(
      hid_manager_ref_,
      base::mac::NSToCFCast(criteria));

  RegisterForNotifications();
}

void GamepadPlatformDataFetcherMac::RegisterForNotifications() {
  // Register for plug/unplug notifications.
  IOHIDManagerRegisterDeviceMatchingCallback(
      hid_manager_ref_,
      &DeviceAddCallback,
      this);
  IOHIDManagerRegisterDeviceRemovalCallback(
      hid_manager_ref_,
      DeviceRemoveCallback,
      this);

  // Register for value change notifications.
  IOHIDManagerRegisterInputValueCallback(
      hid_manager_ref_,
      ValueChangedCallback,
      this);

  IOHIDManagerScheduleWithRunLoop(
      hid_manager_ref_,
      CFRunLoopGetMain(),
      kCFRunLoopDefaultMode);

  enabled_ = IOHIDManagerOpen(hid_manager_ref_,
                              kIOHIDOptionsTypeNone) == kIOReturnSuccess;
}

void GamepadPlatformDataFetcherMac::UnregisterFromNotifications() {
  IOHIDManagerUnscheduleFromRunLoop(
      hid_manager_ref_,
      CFRunLoopGetCurrent(),
      kCFRunLoopDefaultMode);
  IOHIDManagerClose(hid_manager_ref_, kIOHIDOptionsTypeNone);
}

void GamepadPlatformDataFetcherMac::PauseHint(bool pause) {
  if (pause)
    UnregisterFromNotifications();
  else
    RegisterForNotifications();
}

GamepadPlatformDataFetcherMac::~GamepadPlatformDataFetcherMac() {
  UnregisterFromNotifications();
}

GamepadPlatformDataFetcherMac*
GamepadPlatformDataFetcherMac::InstanceFromContext(
    void* context) {
  return reinterpret_cast<GamepadPlatformDataFetcherMac*>(context);
}

void GamepadPlatformDataFetcherMac::DeviceAddCallback(void* context,
                                              IOReturn result,
                                              void* sender,
                                              IOHIDDeviceRef ref) {
  InstanceFromContext(context)->DeviceAdd(ref);
}

void GamepadPlatformDataFetcherMac::DeviceRemoveCallback(void* context,
                                                 IOReturn result,
                                                 void* sender,
                                                 IOHIDDeviceRef ref) {
  InstanceFromContext(context)->DeviceRemove(ref);
}

void GamepadPlatformDataFetcherMac::ValueChangedCallback(void* context,
                                                 IOReturn result,
                                                 void* sender,
                                                 IOHIDValueRef ref) {
  InstanceFromContext(context)->ValueChanged(ref);
}

void GamepadPlatformDataFetcherMac::AddButtonsAndAxes(NSArray* elements,
                                              size_t slot) {
  WebKit::WebGamepad& pad = data_.items[slot];
  AssociatedData& associated = associated_[slot];

  pad.axesLength = 0;
  pad.buttonsLength = 0;
  pad.timestamp = 0;
  memset(pad.axes, 0, sizeof(pad.axes));
  memset(pad.buttons, 0, sizeof(pad.buttons));

  for (id elem in elements) {
    IOHIDElementRef element = reinterpret_cast<IOHIDElementRef>(elem);
    uint32_t usagePage = IOHIDElementGetUsagePage(element);
    uint32_t usage = IOHIDElementGetUsage(element);
    if (IOHIDElementGetType(element) == kIOHIDElementTypeInput_Button &&
        usagePage == kButtonUsagePage) {
      uint32_t button_index = usage - 1;
      if (button_index < WebKit::WebGamepad::buttonsLengthCap) {
        associated.button_elements[button_index] = element;
        pad.buttonsLength = std::max(pad.buttonsLength, button_index + 1);
      }
    }
    else if (IOHIDElementGetType(element) == kIOHIDElementTypeInput_Misc) {
      uint32_t axis_index = usage - kAxisMinimumUsageNumber;
      if (axis_index < WebKit::WebGamepad::axesLengthCap) {
        associated.axis_minimums[axis_index] =
            IOHIDElementGetLogicalMin(element);
        associated.axis_maximums[axis_index] =
            IOHIDElementGetLogicalMax(element);
        associated.axis_elements[axis_index] = element;
        pad.axesLength = std::max(pad.axesLength, axis_index + 1);
      }
    }
  }
}

void GamepadPlatformDataFetcherMac::DeviceAdd(IOHIDDeviceRef device) {
  using WebKit::WebGamepad;
  using WebKit::WebGamepads;
  using base::mac::CFToNSCast;
  using base::mac::CFCastStrict;
  size_t slot;

  if (!enabled_)
    return;

  // Find an index for this device.
  for (slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    // If we already have this device, and it's already connected, don't do
    // anything now.
    if (associated_[slot].device_ref == device && data_.items[slot].connected)
      return;
    if (!data_.items[slot].connected)
      break;
  }

  // We can't handle this many connected devices.
  if (slot == WebGamepads::itemsLengthCap)
    return;

  NSNumber* vendor_id = CFToNSCast(CFCastStrict<CFNumberRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey))));
  NSNumber* product_id = CFToNSCast(CFCastStrict<CFNumberRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey))));
  NSString* product = CFToNSCast(CFCastStrict<CFStringRef>(
      IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey))));
  int vendor_int = [vendor_id intValue];
  int product_int = [product_id intValue];

  char vendor_as_str[5], product_as_str[5];
  snprintf(vendor_as_str, sizeof(vendor_as_str), "%04x", vendor_int);
  snprintf(product_as_str, sizeof(product_as_str), "%04x", product_int);
  associated_[slot].mapper =
      GetGamepadStandardMappingFunction(vendor_as_str, product_as_str);

  NSString* ident = [NSString stringWithFormat:
      @"%@ (%sVendor: %04x Product: %04x)",
      product,
      associated_[slot].mapper ? "STANDARD GAMEPAD " : "",
      vendor_int,
      product_int];
  NSData* as16 = [ident dataUsingEncoding:NSUTF16StringEncoding];

  const size_t kOutputLengthBytes = sizeof(data_.items[slot].id);
  memset(&data_.items[slot].id, 0, kOutputLengthBytes);
  [as16 getBytes:data_.items[slot].id
          length:kOutputLengthBytes - sizeof(WebKit::WebUChar)];

  base::mac::ScopedCFTypeRef<CFArrayRef> elements(
      IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone));
  AddButtonsAndAxes(CFToNSCast(elements), slot);

  associated_[slot].device_ref = device;
  data_.items[slot].connected = true;
  if (slot >= data_.length)
    data_.length = slot + 1;
}

void GamepadPlatformDataFetcherMac::DeviceRemove(IOHIDDeviceRef device) {
  using WebKit::WebGamepads;
  size_t slot;
  if (!enabled_)
    return;

  // Find the index for this device.
  for (slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    if (associated_[slot].device_ref == device && data_.items[slot].connected)
      break;
  }
  DCHECK(slot < WebGamepads::itemsLengthCap);
  // Leave associated device_ref so that it will be reconnected in the same
  // location. Simply mark it as disconnected.
  data_.items[slot].connected = false;
}

void GamepadPlatformDataFetcherMac::ValueChanged(IOHIDValueRef value) {
  if (!enabled_)
    return;

  IOHIDElementRef element = IOHIDValueGetElement(value);
  IOHIDDeviceRef device = IOHIDElementGetDevice(element);

  // Find device slot.
  size_t slot;
  for (slot = 0; slot < data_.length; ++slot) {
    if (data_.items[slot].connected && associated_[slot].device_ref == device)
      break;
  }
  if (slot == data_.length)
    return;

  WebKit::WebGamepad& pad = data_.items[slot];
  AssociatedData& associated = associated_[slot];

  // Find and fill in the associated button event, if any.
  for (size_t i = 0; i < pad.buttonsLength; ++i) {
    if (associated.button_elements[i] == element) {
      pad.buttons[i] = IOHIDValueGetIntegerValue(value) ? 1.f : 0.f;
      pad.timestamp = std::max(pad.timestamp, IOHIDValueGetTimeStamp(value));
      return;
    }
  }

  // Find and fill in the associated axis event, if any.
  for (size_t i = 0; i < pad.axesLength; ++i) {
    if (associated.axis_elements[i] == element) {
      pad.axes[i] = NormalizeAxis(IOHIDValueGetIntegerValue(value),
                                  associated.axis_minimums[i],
                                  associated.axis_maximums[i]);
      pad.timestamp = std::max(pad.timestamp, IOHIDValueGetTimeStamp(value));
      return;
    }
  }
}

void GamepadPlatformDataFetcherMac::GetGamepadData(
    WebKit::WebGamepads* pads,
    bool) {
  if (!enabled_) {
    pads->length = 0;
    return;
  }

  // Copy to the current state to the output buffer, using the mapping
  // function, if there is one available.
  pads->length = data_.length;
  for (size_t i = 0; i < WebKit::WebGamepads::itemsLengthCap; ++i) {
    if (associated_[i].mapper)
      associated_[i].mapper(data_.items[i], &pads->items[i]);
    else
      pads->items[i] = data_.items[i];
  }
}

}  // namespace content
