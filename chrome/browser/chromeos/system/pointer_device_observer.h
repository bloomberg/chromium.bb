// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_POINTER_DEVICE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_POINTER_DEVICE_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/device_hierarchy_observer.h"

namespace chromeos {
namespace system {

class PointerDeviceObserver : public DeviceHierarchyObserver {
 public:
  PointerDeviceObserver();
  virtual ~PointerDeviceObserver();

  // Start observing device hierarchy.
  void Init();

  // Check for presence of devices.
  void CheckDevices();

  class Observer {
   public:
    virtual void TouchpadExists(bool exists) = 0;
    virtual void MouseExists(bool exists) = 0;

   protected:
    Observer() {}
    virtual ~Observer();
  };
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // DeviceHierarchyObserver implementation.
  virtual void DeviceHierarchyChanged() OVERRIDE;
  virtual void DeviceAdded(int device_id) OVERRIDE {}
  virtual void DeviceRemoved(int device_id) OVERRIDE {}

  // Check for pointer devices.
  void CheckTouchpadExists();
  void CheckMouseExists();

  // Callback for pointer device checks.
  void OnTouchpadExists(bool exists);
  void OnMouseExists(bool exists);

  ObserverList<Observer> observers_;

  base::WeakPtrFactory<PointerDeviceObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PointerDeviceObserver);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_POINTER_DEVICE_OBSERVER_H_
