// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_H_
#define DEVICE_HID_HID_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/thread_checker.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/public/interfaces/hid.mojom.h"

namespace device {

class HidConnection;

// The HidService keeps track of human interface devices connected to the
// system. Call HidService::GetInstance to get the singleton instance.
class HidService {
 public:
  // Clients of HidService should add themselves as observer in their
  // GetDevicesCallback. Earlier might cause OnDeviceAdded() and
  // OnDeviceRemoved() to be called before the GetDevicesCallback, while later
  // might cause missing OnDeviceAdded() and OnDeviceRemoved() notifications.
  // TODO(ke.he@intel.com): In the mojofication of HidService, clients should
  // pass an mojom::ObserverPtr in the GetDevices() interface, HidService adds
  // the observer immediately after calling the GetDevicesCallback.
  class Observer {
   public:
    virtual void OnDeviceAdded(device::mojom::HidDeviceInfoPtr info);
    // Notifies all observers that a device is being removed, called before
    // removing the device from HidService. Observers should not depend on the
    // order in which they are notified of the OnDeviceRemove event.
    virtual void OnDeviceRemoved(device::mojom::HidDeviceInfoPtr info);
  };

  using GetDevicesCallback =
      base::Callback<void(std::vector<device::mojom::HidDeviceInfoPtr>)>;
  using ConnectCallback =
      base::Callback<void(scoped_refptr<HidConnection> connection)>;

  static constexpr base::TaskTraits kBlockingTaskTraits = {
      base::MayBlock(), base::TaskPriority::USER_VISIBLE,
      base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

  // This function should be called on a thread with a MessageLoopForUI.
  static std::unique_ptr<HidService> Create();

  virtual ~HidService();

  // Enumerates available devices. The provided callback will always be posted
  // to the calling thread's task runner.
  void GetDevices(const GetDevicesCallback& callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Opens a connection to a device. The callback will be run with null on
  // failure.
  virtual void Connect(const std::string& device_guid,
                       const ConnectCallback& callback) = 0;

 protected:
  friend class HidConnectionTest;

  typedef std::map<std::string, scoped_refptr<HidDeviceInfo>> DeviceMap;

  HidService();

  virtual base::WeakPtr<HidService> GetWeakPtr() = 0;
  void AddDevice(scoped_refptr<HidDeviceInfo> info);
  void RemoveDevice(const HidPlatformDeviceId& platform_device_id);
  void FirstEnumerationComplete();

  const DeviceMap& devices() const { return devices_; }

  base::ThreadChecker thread_checker_;

 private:
  void RunPendingEnumerations();
  std::string FindDeviceIdByPlatformDeviceId(
      const HidPlatformDeviceId& platform_device_id);

  DeviceMap devices_;

  bool enumeration_ready_ = false;
  std::vector<GetDevicesCallback> pending_enumerations_;
  base::ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(HidService);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_H_
