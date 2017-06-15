// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_advertisement_device_queue.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "chromeos/components/tether/ble_constants.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

BleAdvertisementDeviceQueue::BleAdvertisementDeviceQueue() {}

BleAdvertisementDeviceQueue::~BleAdvertisementDeviceQueue() {}

bool BleAdvertisementDeviceQueue::SetDevices(
    std::vector<cryptauth::RemoteDevice> devices) {
  // Determine which devices exist in |devices| but not in |device_queue_|, then
  // add them to |device_queue_|.
  std::vector<cryptauth::RemoteDevice> missing_from_queue;
  for (auto& device : devices) {
    if (std::find(device_queue_.begin(), device_queue_.end(), device) ==
        device_queue_.end()) {
      missing_from_queue.push_back(device);
    }
  }
  if (!missing_from_queue.empty()) {
    for (auto& device : missing_from_queue) {
      device_queue_.push_back(device);
    }
  }

  // Determine which devices do not exist in |devices| but do exist in
  // |device_queue_|, then remove them from |device_queue_|.
  std::vector<cryptauth::RemoteDevice> to_remove_from_queue;
  for (auto& device : device_queue_) {
    if (std::find(devices.begin(), devices.end(), device) == devices.end()) {
      to_remove_from_queue.push_back(device);
    }
  }
  if (!to_remove_from_queue.empty()) {
    for (auto& device : to_remove_from_queue) {
      device_queue_.erase(
          std::find(device_queue_.begin(), device_queue_.end(), device));
    }
  }

  return !missing_from_queue.empty() || !to_remove_from_queue.empty();
}

void BleAdvertisementDeviceQueue::MoveDeviceToEnd(std::string device_id) {
  if (device_id.empty()) {
    return;
  }

  int index = -1;
  for (size_t i = 0; i < device_queue_.size(); i++) {
    if (device_id == device_queue_[i].GetDeviceId()) {
      index = i;
      break;
    }
  }

  if (index >= 0) {
    cryptauth::RemoteDevice to_move = device_queue_[index];
    device_queue_.erase(device_queue_.begin() + index);
    device_queue_.push_back(to_move);
  }
}

std::vector<cryptauth::RemoteDevice>
BleAdvertisementDeviceQueue::GetDevicesToWhichToAdvertise() const {
  std::vector<cryptauth::RemoteDevice> to_advertise;

  if (device_queue_.empty()) {
    return to_advertise;
  }

  for (auto& device : device_queue_) {
    to_advertise.push_back(device);

    if (to_advertise.size() ==
        static_cast<size_t>(kMaxConcurrentAdvertisements)) {
      break;
    }
  }

  return to_advertise;
}

size_t BleAdvertisementDeviceQueue::GetSize() const {
  return device_queue_.size();
}

}  // namespace tether

}  // namespace chromeos
