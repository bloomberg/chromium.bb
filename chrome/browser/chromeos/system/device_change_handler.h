// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_DEVICE_CHANGE_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_DEVICE_CHANGE_HANDLER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/system/pointer_device_observer.h"

namespace chromeos {
namespace system {

// Observes changes in device hierarchy. When a new touchpad/mouse is attached,
// applies the last used touchpad/mouse settings to the system.
class DeviceChangeHandler : public PointerDeviceObserver::Observer {
 public:
  DeviceChangeHandler();
  virtual ~DeviceChangeHandler();

 private:
  // PointerDeviceObserver::Observer implementation.
  virtual void TouchpadExists(bool exists) OVERRIDE;
  virtual void MouseExists(bool exists) OVERRIDE;

  scoped_ptr<PointerDeviceObserver> pointer_device_observer_;
};

}  // namespace system
}  // namespace chromeos


#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_DEVICE_CHANGE_HANDLER_H_
