// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_IMPL_H
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_IMPL_H

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/components/tether/host_scan_scheduler.h"
#include "chromeos/components/tether/host_scanner.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

// Concrete HostScanScheduler implementation. One of three events begin a scan
// attempt:
//   (1) NetworkStateHandler requests a Tether network scan.
//   (2) The device loses its Internet connection.
//   (3) The scan is explicitly requested via ScheduleScan().
class HostScanSchedulerImpl : public HostScanScheduler,
                              public NetworkStateHandlerObserver,
                              public HostScanner::Observer {
 public:
  HostScanSchedulerImpl(NetworkStateHandler* network_state_handler,
                        HostScanner* host_scanner);
  ~HostScanSchedulerImpl() override;

  // HostScanScheduler:
  void ScheduleScan() override;

 protected:
  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;
  void ScanRequested() override;

  // HostScanner::Observer:
  void ScanFinished() override;

 private:
  friend class HostScanSchedulerImplTest;

  void EnsureScan();
  bool IsTetherNetworkConnectingOrConnected();
  void LogHostScanBatchMetric();

  void SetTestDoubles(std::unique_ptr<base::Timer> test_timer,
                      std::unique_ptr<base::Clock> test_clock);

  NetworkStateHandler* network_state_handler_;
  HostScanner* host_scanner_;

  std::unique_ptr<base::Timer> timer_;
  std::unique_ptr<base::Clock> clock_;

  base::Time last_scan_batch_start_timestamp_;
  base::Time last_scan_end_timestamp_;

  base::WeakPtrFactory<HostScanSchedulerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostScanSchedulerImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_IMPL_H
