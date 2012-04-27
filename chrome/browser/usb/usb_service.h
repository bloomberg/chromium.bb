// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_SERVICE_H_
#define CHROME_BROWSER_USB_USB_SERVICE_H_
#pragma once

#include <map>
#include <utility>

#include "base/basictypes.h"
#include "base/threading/thread.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/usb/usb_device.h"
#include "third_party/libusb/libusb/libusb.h"

typedef libusb_context* PlatformUsbContext;

// The USB service handles creating and managing an event handler thread that is
// used to manage and dispatch USB events. It is also responsbile for device
// discovery on the system, which allows it to re-use device handles to prevent
// competition for the same USB device.
class UsbService : public ProfileKeyedService {
 public:
  UsbService();
  virtual ~UsbService();

  // Cleanup must be invoked before the service is destroyed. It interrupts the
  // event handling thread and disposes of open devices.
  void Cleanup();

  // Find the (topologically) first USB device identified by vendor_id and
  // product_id. The created device is associated with this service, so that
  // it can be used to close the device later.
  UsbDevice* FindDevice(const uint16 vendor_id, const uint16 product_id);

  // This function should not be called by normal code. It is invoked by a
  // UsbDevice's Close function and disposes of the associated platform handle.
  void CloseDevice(scoped_refptr<UsbDevice> device);

 private:
  // Posts a HandleEvent task to the event handling thread.
  void PostHandleEventTask();

  // Handles a single USB event. If the service is still running after the event
  // is handled, posts another HandleEvent callback to the thread.
  void HandleEvent();

  // PlatformShutdown is invoked after event handling has been suspended and is
  // used to free the platform resources associated with the service.
  void PlatformShutdown();

  bool running_;
  PlatformUsbContext context_;
  base::Thread thread_;

  // The devices_ map contains scoped_refptrs to all open devices, indicated by
  // their vendor and product id. This allows for reusing an open device without
  // creating another platform handle for it.
  typedef std::map<std::pair<uint16, uint16>, scoped_refptr<UsbDevice> >
      DeviceMap;
  DeviceMap devices_;

  DISALLOW_EVIL_CONSTRUCTORS(UsbService);
};

#endif  // CHROME_BROWSER_USB_USB_SERVICE_H_
