// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_POINTER_DEVICE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_POINTER_DEVICE_OBSERVER_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/device_hierarchy_observer.h"

namespace chromeos {
namespace system {

class PointerDeviceObserver
    : public DeviceHierarchyObserver {
 public:
  PointerDeviceObserver();
  virtual ~PointerDeviceObserver();

  void Init();

  class Observer {
   public:
    virtual void TouchpadExists(bool exists) = 0;
    virtual void MouseExists(bool exists) = 0;

   protected:
    Observer() {}
    virtual ~Observer();

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // DeviceHierarchyObserver implementation.
  virtual void DeviceHierarchyChanged() OVERRIDE;

  // Check for input devices.
  void CheckTouchpadExists();
  void CheckMouseExists();

  // Callback for input device checks.
  void TouchpadExists(bool* exists);
  void MouseExists(bool* exists);

  ObserverList<Observer> observers_;

  base::WeakPtrFactory<PointerDeviceObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PointerDeviceObserver);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_POINTER_DEVICE_OBSERVER_H_
