// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_platform_data_fetcher_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

#import <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDKeys.h>

using blink::WebGamepad;
using blink::WebGamepads;

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

}  // namespace

GamepadPlatformDataFetcherMac::GamepadPlatformDataFetcherMac()
    : enabled_(true) {
  memset(associated_, 0, sizeof(associated_));

  xbox_fetcher_.reset(new XboxDataFetcher(this));
  if (!xbox_fetcher_->RegisterForNotifications())
    xbox_fetcher_.reset();

  hid_manager_ref_.reset(IOHIDManagerCreate(kCFAllocatorDefault,
                                            kIOHIDOptionsTypeNone));
  if (CFGetTypeID(hid_manager_ref_) != IOHIDManagerGetTypeID()) {
    enabled_ = false;
    return;
  }

  base::scoped_nsobject<NSArray> criteria([[NSArray alloc] initWithObjects:
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

  if (xbox_fetcher_)
    xbox_fetcher_->RegisterForNotifications();
}

void GamepadPlatformDataFetcherMac::UnregisterFromNotifications() {
  IOHIDManagerUnscheduleFromRunLoop(
      hid_manager_ref_,
      CFRunLoopGetCurrent(),
      kCFRunLoopDefaultMode);
  IOHIDManagerClose(hid_manager_ref_, kIOHIDOptionsTypeNone);
  if (xbox_fetcher_)
    xbox_fetcher_->UnregisterFromNotifications();
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
GamepadPlatformDataFetcherMac::InstanceFromContext(void* context) {
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
  WebGamepad& pad = data_.items[slot];
  AssociatedData& associated = associated_[slot];
  CHECK(!associated.is_xbox);

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
      if (button_index < WebGamepad::buttonsLengthCap) {
        associated.hid.button_elements[button_index] = element;
        pad.buttonsLength = std::max(pad.buttonsLength, button_index + 1);
      }
    }
    else if (IOHIDElementGetType(element) == kIOHIDElementTypeInput_Misc) {
      uint32_t axis_index = usage - kAxisMinimumUsageNumber;
      if (axis_index < WebGamepad::axesLengthCap) {
        associated.hid.axis_minimums[axis_index] =
            IOHIDElementGetLogicalMin(element);
        associated.hid.axis_maximums[axis_index] =
            IOHIDElementGetLogicalMax(element);
        associated.hid.axis_elements[axis_index] = element;
        pad.axesLength = std::max(pad.axesLength, axis_index + 1);
      }
    }
  }
}

size_t GamepadPlatformDataFetcherMac::GetEmptySlot() {
  // Find a free slot for this device.
  for (size_t slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    if (!data_.items[slot].connected)
      return slot;
  }
  return WebGamepads::itemsLengthCap;
}

size_t GamepadPlatformDataFetcherMac::GetSlotForDevice(IOHIDDeviceRef device) {
  for (size_t slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    // If we already have this device, and it's already connected, don't do
    // anything now.
    if (data_.items[slot].connected &&
        !associated_[slot].is_xbox &&
        associated_[slot].hid.device_ref == device)
      return WebGamepads::itemsLengthCap;
  }
  return GetEmptySlot();
}

size_t GamepadPlatformDataFetcherMac::GetSlotForXboxDevice(
    XboxController* device) {
  for (size_t slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    if (associated_[slot].is_xbox &&
        associated_[slot].xbox.location_id == device->location_id()) {
      if (data_.items[slot].connected) {
        // The device is already connected. No idea why we got a second "device
        // added" call, but let's not add it twice.
        DCHECK_EQ(associated_[slot].xbox.device, device);
        return WebGamepads::itemsLengthCap;
      } else {
        // A device with the same location ID was previously connected, so put
        // it in the same slot.
        return slot;
      }
    }
  }
  return GetEmptySlot();
}

void GamepadPlatformDataFetcherMac::DeviceAdd(IOHIDDeviceRef device) {
  using base::mac::CFToNSCast;
  using base::mac::CFCastStrict;

  if (!enabled_)
    return;

  // Find an index for this device.
  size_t slot = GetSlotForDevice(device);

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
  associated_[slot].hid.mapper =
      GetGamepadStandardMappingFunction(vendor_as_str, product_as_str);

  NSString* ident = [NSString stringWithFormat:
      @"%@ (%sVendor: %04x Product: %04x)",
      product,
      associated_[slot].hid.mapper ? "STANDARD GAMEPAD " : "",
      vendor_int,
      product_int];
  NSData* as16 = [ident dataUsingEncoding:NSUTF16LittleEndianStringEncoding];

  const size_t kOutputLengthBytes = sizeof(data_.items[slot].id);
  memset(&data_.items[slot].id, 0, kOutputLengthBytes);
  [as16 getBytes:data_.items[slot].id
          length:kOutputLengthBytes - sizeof(blink::WebUChar)];

  base::ScopedCFTypeRef<CFArrayRef> elements(
      IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone));
  AddButtonsAndAxes(CFToNSCast(elements), slot);

  associated_[slot].hid.device_ref = device;
  data_.items[slot].connected = true;
  if (slot >= data_.length)
    data_.length = slot + 1;
}

void GamepadPlatformDataFetcherMac::DeviceRemove(IOHIDDeviceRef device) {
  if (!enabled_)
    return;

  // Find the index for this device.
  size_t slot;
  for (slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    if (data_.items[slot].connected &&
        !associated_[slot].is_xbox &&
        associated_[slot].hid.device_ref == device)
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
    if (data_.items[slot].connected &&
        !associated_[slot].is_xbox &&
        associated_[slot].hid.device_ref == device)
      break;
  }
  if (slot == data_.length)
    return;

  WebGamepad& pad = data_.items[slot];
  AssociatedData& associated = associated_[slot];

  // Find and fill in the associated button event, if any.
  for (size_t i = 0; i < pad.buttonsLength; ++i) {
    if (associated.hid.button_elements[i] == element) {
      pad.buttons[i] = IOHIDValueGetIntegerValue(value) ? 1.f : 0.f;
      pad.timestamp = std::max(pad.timestamp, IOHIDValueGetTimeStamp(value));
      return;
    }
  }

  // Find and fill in the associated axis event, if any.
  for (size_t i = 0; i < pad.axesLength; ++i) {
    if (associated.hid.axis_elements[i] == element) {
      pad.axes[i] = NormalizeAxis(IOHIDValueGetIntegerValue(value),
                                  associated.hid.axis_minimums[i],
                                  associated.hid.axis_maximums[i]);
      pad.timestamp = std::max(pad.timestamp, IOHIDValueGetTimeStamp(value));
      return;
    }
  }
}

void GamepadPlatformDataFetcherMac::XboxDeviceAdd(XboxController* device) {
  if (!enabled_)
    return;

  size_t slot = GetSlotForXboxDevice(device);

  // We can't handle this many connected devices.
  if (slot == WebGamepads::itemsLengthCap)
    return;

  device->SetLEDPattern(
      (XboxController::LEDPattern)(XboxController::LED_FLASH_TOP_LEFT + slot));

  NSString* ident =
      [NSString stringWithFormat:
          @"%@ (STANDARD GAMEPAD Vendor: %04x Product: %04x)",
              device->GetControllerType() == XboxController::XBOX_360_CONTROLLER
                  ? @"Xbox 360 Controller"
                  : @"Xbox One Controller",
              device->GetProductId(), device->GetVendorId()];
  NSData* as16 = [ident dataUsingEncoding:NSUTF16StringEncoding];
  const size_t kOutputLengthBytes = sizeof(data_.items[slot].id);
  memset(&data_.items[slot].id, 0, kOutputLengthBytes);
  [as16 getBytes:data_.items[slot].id
          length:kOutputLengthBytes - sizeof(blink::WebUChar)];

  associated_[slot].is_xbox = true;
  associated_[slot].xbox.device = device;
  associated_[slot].xbox.location_id = device->location_id();
  data_.items[slot].connected = true;
  data_.items[slot].axesLength = 4;
  data_.items[slot].buttonsLength = 17;
  data_.items[slot].timestamp = 0;
  if (slot >= data_.length)
    data_.length = slot + 1;
}

void GamepadPlatformDataFetcherMac::XboxDeviceRemove(XboxController* device) {
  if (!enabled_)
    return;

  // Find the index for this device.
  size_t slot;
  for (slot = 0; slot < WebGamepads::itemsLengthCap; ++slot) {
    if (data_.items[slot].connected &&
        associated_[slot].is_xbox &&
        associated_[slot].xbox.device == device)
      break;
  }
  DCHECK(slot < WebGamepads::itemsLengthCap);
  // Leave associated location id so that the controller will be reconnected in
  // the same slot if it is plugged in again. Simply mark it as disconnected.
  data_.items[slot].connected = false;
}

void GamepadPlatformDataFetcherMac::XboxValueChanged(
    XboxController* device, const XboxController::Data& data) {
  // Find device slot.
  size_t slot;
  for (slot = 0; slot < data_.length; ++slot) {
    if (data_.items[slot].connected &&
        associated_[slot].is_xbox &&
        associated_[slot].xbox.device == device)
      break;
  }
  if (slot == data_.length)
    return;

  WebGamepad& pad = data_.items[slot];

  for (size_t i = 0; i < 6; i++) {
    pad.buttons[i] = data.buttons[i] ? 1.0f : 0.0f;
  }
  pad.buttons[6] = data.triggers[0];
  pad.buttons[7] = data.triggers[1];
  for (size_t i = 8; i < 17; i++) {
    pad.buttons[i] = data.buttons[i - 2] ? 1.0f : 0.0f;
  }
  for (size_t i = 0; i < arraysize(data.axes); i++) {
    pad.axes[i] = data.axes[i];
  }

  pad.timestamp = base::TimeTicks::Now().ToInternalValue();
}

void GamepadPlatformDataFetcherMac::GetGamepadData(WebGamepads* pads, bool) {
  if (!enabled_ && !xbox_fetcher_) {
    pads->length = 0;
    return;
  }

  // Copy to the current state to the output buffer, using the mapping
  // function, if there is one available.
  pads->length = WebGamepads::itemsLengthCap;
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    if (!associated_[i].is_xbox && associated_[i].hid.mapper)
      associated_[i].hid.mapper(data_.items[i], &pads->items[i]);
    else
      pads->items[i] = data_.items[i];
  }
}

}  // namespace content
