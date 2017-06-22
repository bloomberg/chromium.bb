// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_DEVICE_PRIORITIZER_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_DEVICE_PRIORITIZER_IMPL_H_

#include "base/macros.h"
#include "chromeos/components/tether/host_scan_device_prioritizer.h"
#include "chromeos/network/network_state_handler.h"
#include "components/cryptauth/remote_device.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

class TetherHostResponseRecorder;
class DeviceIdTetherNetworkGuidMap;

// Implementation of HostScanDevicePrioritizer.
class HostScanDevicePrioritizerImpl
    : public HostScanDevicePrioritizer,
      public NetworkStateHandler::TetherSortDelegate {
 public:
  HostScanDevicePrioritizerImpl(
      TetherHostResponseRecorder* tether_host_response_recorder,
      DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map);
  ~HostScanDevicePrioritizerImpl() override;

  // HostScanDevicePrioritizer:
  void SortByHostScanOrder(
      std::vector<cryptauth::RemoteDevice>* remote_devices) const override;

  // NetworkStateHandler::TetherNetworkListSorter:
  void SortTetherNetworkList(
      NetworkStateHandler::ManagedStateList* tether_networks) const override;

 private:
  // Performs sorting for both SortByHostScanOrder() and
  // SortTetherNetworkList().
  template <typename T>
  void SortNetworks(std::vector<T>* list_to_sort) const;

  std::string GetDeviceId(const cryptauth::RemoteDevice& remote_device) const;
  std::string GetDeviceId(
      const std::unique_ptr<ManagedState>& tether_network_state) const;

  TetherHostResponseRecorder* tether_host_response_recorder_;
  DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map_;

  DISALLOW_COPY_AND_ASSIGN(HostScanDevicePrioritizerImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_DEVICE_PRIORITIZER_IMPL_H_
