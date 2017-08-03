// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_H
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_H

#include "chromeos/components/tether/host_scanner.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

// Schedules scans for Tether hosts. One of three events begin a scan attempt:
//
//   (1) NetworkStateHandler requests a Tether network scan.
//   (2) The device loses its Internet connection.
//   (3) The user has just logged in or has just resumed using the device after
//       it had been sleeping/suspended, and the device does not have an
//       Internet connection. Note: It is the responsibility of the owner of
//       HostScanScheduler to inform it of user login via UserLoggedIn().
class HostScanScheduler : public NetworkStateHandlerObserver,
                          public HostScanner::Observer {
 public:
  HostScanScheduler(NetworkStateHandler* network_state_handler,
                    HostScanner* host_scanner);
  ~HostScanScheduler() override;

  void ScheduleScan();

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;
  void ScanRequested() override;

  // HostScanner::Observer:
  void ScanFinished() override;

 private:
  friend class HostScanSchedulerTest;

  void EnsureScan();
  bool IsNetworkConnectingOrConnected(const NetworkState* network);
  bool IsTetherNetworkConnectingOrConnected();

  NetworkStateHandler* network_state_handler_;
  HostScanner* host_scanner_;

  DISALLOW_COPY_AND_ASSIGN(HostScanScheduler);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_H
