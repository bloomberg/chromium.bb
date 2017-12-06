// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_advertisement_device_queue.h"

#include <algorithm>

#include "base/logging.h"
#include "chromeos/components/tether/ble_constants.h"

namespace chromeos {

namespace tether {

namespace {

bool DoesVectorContainPrioritizedDeviceId(
    const ConnectionPriority& connection_priority,
    const std::string& device_id,
    const std::vector<BleAdvertisementDeviceQueue::PrioritizedDeviceId>&
        prioritized_devices) {
  for (const auto& prioritized_device : prioritized_devices) {
    if (prioritized_device.connection_priority == connection_priority &&
        prioritized_device.device_id == device_id) {
      return true;
    }
  }

  return false;
}

}  // namespace

BleAdvertisementDeviceQueue::PrioritizedDeviceId::PrioritizedDeviceId(
    const std::string& device_id,
    const ConnectionPriority& connection_priority)
    : device_id(device_id), connection_priority(connection_priority) {}

BleAdvertisementDeviceQueue::PrioritizedDeviceId::~PrioritizedDeviceId() =
    default;

BleAdvertisementDeviceQueue::BleAdvertisementDeviceQueue() = default;

BleAdvertisementDeviceQueue::~BleAdvertisementDeviceQueue() = default;

bool BleAdvertisementDeviceQueue::SetPrioritizedDeviceIds(
    const std::vector<PrioritizedDeviceId>& prioritized_ids) {
  bool updated = false;

  // For each device provided, check to see if the device is already part of the
  // queue. If it is not, add it to the end of the deque associated with the
  // device's priority.
  for (const auto& priotizied_id : prioritized_ids) {
    std::deque<std::string>& deque_for_priority =
        priority_to_deque_map_[priotizied_id.connection_priority];
    if (std::find(deque_for_priority.begin(), deque_for_priority.end(),
                  priotizied_id.device_id) == deque_for_priority.end()) {
      deque_for_priority.push_back(priotizied_id.device_id);
      updated = true;
    }
  }

  // Now, iterate through each priority's deque to see if any of the entries
  // were not provided as part of the |prioritized_ids| parameter. If any such
  // entries exist, remove them from the map.
  for (auto& map_entry : priority_to_deque_map_) {
    auto device_deque_it = map_entry.second.begin();
    while (device_deque_it != map_entry.second.end()) {
      if (DoesVectorContainPrioritizedDeviceId(
              map_entry.first, *device_deque_it, prioritized_ids)) {
        ++device_deque_it;
        continue;
      }

      device_deque_it = map_entry.second.erase(device_deque_it);
      updated = true;
    }
  }

  return updated;
}

void BleAdvertisementDeviceQueue::MoveDeviceToEnd(
    const std::string& device_id) {
  DCHECK(!device_id.empty());

  for (auto& map_entry : priority_to_deque_map_) {
    auto device_deque_it = map_entry.second.begin();
    while (device_deque_it != map_entry.second.end()) {
      if (*device_deque_it == device_id)
        break;
      ++device_deque_it;
    }

    if (device_deque_it == map_entry.second.end())
      continue;

    std::string to_move = *device_deque_it;
    map_entry.second.erase(device_deque_it);
    map_entry.second.push_back(to_move);
  }
}

std::vector<std::string>
BleAdvertisementDeviceQueue::GetDeviceIdsToWhichToAdvertise() const {
  std::vector<std::string> device_ids;
  AddDevicesToVectorForPriority(ConnectionPriority::CONNECTION_PRIORITY_HIGH,
                                &device_ids);
  AddDevicesToVectorForPriority(ConnectionPriority::CONNECTION_PRIORITY_MEDIUM,
                                &device_ids);
  AddDevicesToVectorForPriority(ConnectionPriority::CONNECTION_PRIORITY_LOW,
                                &device_ids);
  DCHECK(device_ids.size() <= kMaxConcurrentAdvertisements);
  return device_ids;
}

size_t BleAdvertisementDeviceQueue::GetSize() const {
  size_t count = 0;
  for (const auto& map_entry : priority_to_deque_map_)
    count += map_entry.second.size();
  return count;
}

void BleAdvertisementDeviceQueue::AddDevicesToVectorForPriority(
    ConnectionPriority connection_priority,
    std::vector<std::string>* device_ids_out) const {
  if (priority_to_deque_map_.find(connection_priority) ==
      priority_to_deque_map_.end()) {
    // Nothing to do if there is no entry for this priority.
    return;
  }

  const std::deque<std::string>& deque_for_priority =
      priority_to_deque_map_.at(connection_priority);
  size_t i = 0;
  while (i < deque_for_priority.size() &&
         device_ids_out->size() < kMaxConcurrentAdvertisements) {
    device_ids_out->push_back(deque_for_priority[i]);
    ++i;
  }
}

}  // namespace tether

}  // namespace chromeos
