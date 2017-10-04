// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_INPUT_SERVICE_LINUX_H_
#define DEVICE_HID_INPUT_SERVICE_LINUX_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "device/hid/public/interfaces/input_service.mojom.h"

namespace device {

// This class provides information and notifications about
// connected/disconnected input/HID devices. This class is *NOT*
// thread-safe and all methods must be called from the FILE thread.
// TODO(ke.he@intel.com): We'll move the InputServiceLinux into
// device service, and run it in the FILE thread.
class InputServiceLinux {
 public:
  using DeviceMap = std::map<std::string, device::mojom::InputDeviceInfoPtr>;

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnInputDeviceAdded(device::mojom::InputDeviceInfoPtr info) = 0;
    virtual void OnInputDeviceRemoved(const std::string& id) = 0;
  };

  InputServiceLinux();
  virtual ~InputServiceLinux();

  // Returns the InputServiceLinux instance for the current process. Creates one
  // if none has been set.
  static InputServiceLinux* GetInstance();

  // Returns true if an InputServiceLinux instance has been set for the current
  // process. An instance is set on the first call to GetInstance() or
  // SetForTesting().
  static bool HasInstance();

  // Sets the InputServiceLinux instance for the current process. Cannot be
  // called if GetInstance() or SetForTesting() has already been called in the
  // current process. |service| will never be deleted.
  static void SetForTesting(std::unique_ptr<InputServiceLinux> service);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns list of all currently connected input/hid devices.
  virtual void GetDevices(
      std::vector<device::mojom::InputDeviceInfoPtr>* devices);

 protected:
  void AddDevice(device::mojom::InputDeviceInfoPtr info);
  void RemoveDevice(const std::string& id);

  bool CalledOnValidThread() const;

  DeviceMap devices_;
  base::ObserverList<Observer> observers_;

 private:
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(InputServiceLinux);
};

}  // namespace device

#endif  // DEVICE_HID_INPUT_SERVICE_LINUX_H_
