// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_INPUT_SERVICE_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_INPUT_SERVICE_PROXY_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "device/hid/input_service_linux.h"

namespace chromeos {

// Proxy to device::InputServiceLinux. Should be created and used on the UI
// thread.
class InputServiceProxy {
 public:
  typedef device::InputServiceLinux::InputDeviceInfo InputDeviceInfo;

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnInputDeviceAdded(const InputDeviceInfo& info) = 0;
    virtual void OnInputDeviceRemoved(const std::string& id) = 0;
  };

  typedef base::Callback<void(const std::vector<InputDeviceInfo>& devices)>
      GetDevicesCallback;
  typedef base::Callback<void(bool success, const InputDeviceInfo& info)>
      GetDeviceInfoCallback;

  InputServiceProxy();
  ~InputServiceProxy();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void GetDevices(const GetDevicesCallback& callback);
  void GetDeviceInfo(const std::string& id,
                     const GetDeviceInfoCallback& callback);

 private:
  class ServiceObserver;

  void OnDeviceAdded(const device::InputServiceLinux::InputDeviceInfo& info);
  void OnDeviceRemoved(const std::string& id);

  ObserverList<Observer> observers_;
  scoped_ptr<ServiceObserver> service_observer_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<InputServiceProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InputServiceProxy);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_INPUT_SERVICE_PROXY_H_
