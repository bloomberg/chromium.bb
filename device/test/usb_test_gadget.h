// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_TEST_USB_TEST_GADGET_H_
#define DEVICE_TEST_USB_TEST_GADGET_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace usb_service {
class UsbDevice;
}  // namespace usb_service

namespace device {

class UsbTestGadget {
 public:
  enum Type {
    DEFAULT = 0,
    KEYBOARD,
    MOUSE,
  };

  virtual ~UsbTestGadget() {}

  static bool IsTestEnabled();
  static scoped_ptr<UsbTestGadget> Claim();

  virtual bool Unclaim() = 0;
  virtual bool Disconnect() = 0;
  virtual bool Reconnect() = 0;
  virtual bool SetType(Type type) = 0;

  virtual usb_service::UsbDevice* GetDevice() const = 0;
  virtual std::string GetSerial() const = 0;

 protected:
  UsbTestGadget() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbTestGadget);
};

}  // namespace device

#endif  // DEVICE_TEST_USB_TEST_GADGET_H_
