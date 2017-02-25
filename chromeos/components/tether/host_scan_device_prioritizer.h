// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_DEVICE_PRIORITIZER_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_DEVICE_PRIORITIZER_H_

#include "base/macros.h"
#include "components/cryptauth/remote_device.h"

class PrefService;
class PrefRegistrySimple;

namespace chromeos {

namespace tether {

// Prioritizes the order of devices when performing a host scan. To optimize for
// the most common tethering operations, this class uses the following rules:
//   * The device which has most recently sent a successful
//     ConnectTetheringResponse is always at the front of the queue.
//   * Devices which have most recently sent a successful
//     TetherAvailabilityResponse are next in the order, as long as they do not
//     violate the first rule.
//   * All other devices are left in the order they are passed.
class HostScanDevicePrioritizer {
 public:
  // Note: The PrefService* passed here must be created using the same registry
  // passed to RegisterPrefs().
  HostScanDevicePrioritizer(PrefService* pref_service);
  ~HostScanDevicePrioritizer();

  // Registers the prefs used by this class to |registry|. Must be called before
  // this class is utilized.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Records a TetherAvailabilityResponse. This function should be called each
  // time that a response is received from a potential host, even if a
  // connection is not started.
  void RecordSuccessfulTetherAvailabilityResponse(
      const cryptauth::RemoteDevice& remote_device);

  // Records a ConnectTetheringResponse. This function should be called each
  // time that a response is received from a host.
  void RecordSuccessfulConnectTetheringResponse(
      const cryptauth::RemoteDevice& remote_device);

  // Prioritizes |remote_devices| using the rules described above.
  void SortByHostScanOrder(
      std::vector<cryptauth::RemoteDevice>* remote_devices) const;

 private:
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(HostScanDevicePrioritizer);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_DEVICE_PRIORITIZER_H_
