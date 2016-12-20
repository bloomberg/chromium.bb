// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISEMENT_DEVICE_QUEUE_H_
#define CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISEMENT_DEVICE_QUEUE_H_

#include <vector>

#include "base/macros.h"
#include "components/cryptauth/remote_device.h"

namespace chromeos {

namespace tether {

// Queue of devices to which to advertise. Because only
// |kMaxConcurrentAdvertisements| devices can be advertised to concurrently,
// this queue maintains the order of devices to ensure that each device gets its
// fair share of time spent contacting it.
class BleAdvertisementDeviceQueue {
 public:
  BleAdvertisementDeviceQueue();
  virtual ~BleAdvertisementDeviceQueue();

  // Updates the queue with the given |devices|. Devices which are already in
  // the queue and are not in |devices| are removed from the queue, and all
  // devices which are not in the queue but are in |devices| are added to the
  // end of the queue. Note devices that are already in the queue will not
  // change order as a result of this function being called to ensure that the
  // queue remains in order. Returns whether the device list has changed due to
  // the function call.
  bool SetDevices(std::vector<cryptauth::RemoteDevice> devices);

  // Moves the given device to the end of the queue. If the device was not in
  // the queue to begin with, do nothing.
  void MoveDeviceToEnd(std::string device_id);

  // Returns a list of devices to which to advertise. The devices returned are
  // the first |kMaxConcurrentAdvertisements| devices in the front of the queue,
  // or fewer if the number of devices in the queue is less than that value.
  std::vector<cryptauth::RemoteDevice> GetDevicesToWhichToAdvertise() const;

  size_t GetSize() const;

 private:
  std::vector<cryptauth::RemoteDevice> device_queue_;

  DISALLOW_COPY_AND_ASSIGN(BleAdvertisementDeviceQueue);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISEMENT_DEVICE_QUEUE_H_
