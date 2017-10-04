// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_INPUT_SERVICE_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_INPUT_SERVICE_PROXY_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "device/hid/input_service_linux.h"
#include "device/hid/public/interfaces/input_service.mojom.h"

namespace chromeos {

// Proxy to device::InputServiceLinux. Should be created and used on the UI
// thread.
class InputServiceProxy {
 public:
  typedef device::mojom::InputDeviceInfoPtr InputDeviceInfoPtr;

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnInputDeviceAdded(InputDeviceInfoPtr info) = 0;
    virtual void OnInputDeviceRemoved(const std::string& id) = 0;
  };

  typedef base::OnceCallback<void(
      std::vector<device::mojom::InputDeviceInfoPtr> devices)>
      GetDevicesCallback;

  InputServiceProxy();
  ~InputServiceProxy();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void GetDevices(GetDevicesCallback callback);

  // Returns the SequencedTaskRunner for device::InputServiceLinux. Make it
  // static so that all InputServiceProxy instances and code that needs access
  // to device::InputServiceLinux uses the same sequence.
  static scoped_refptr<base::SequencedTaskRunner> GetInputServiceTaskRunner();

  // Should be called once before any InputServiceProxy instance is created.
  static void SetUseUIThreadForTesting(bool use_ui_thread);

 private:
  class ServiceObserver;

  void OnDeviceAdded(InputDeviceInfoPtr info);
  void OnDeviceRemoved(const std::string& id);

  base::ObserverList<Observer> observers_;
  std::unique_ptr<ServiceObserver> service_observer_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<InputServiceProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InputServiceProxy);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_INPUT_SERVICE_PROXY_H_
