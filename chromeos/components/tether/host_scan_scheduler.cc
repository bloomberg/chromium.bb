// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_scheduler.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

HostScanScheduler::HostScanScheduler(NetworkStateHandler* network_state_handler,
                                     HostScanner* host_scanner)
    : network_state_handler_(network_state_handler),
      host_scanner_(host_scanner) {
  network_state_handler_->AddObserver(this, FROM_HERE);
  host_scanner_->AddObserver(this);
}

HostScanScheduler::~HostScanScheduler() {
  // Note: We not call NetworkStateHandler::SetTetherScanState(false) here
  // because at the point in time when HostScanScheduler is being destroyed, the
  // Tether DeviceState will already have been destroyed. Calling
  // SetTetherScanState() is unnecessary and would cause a crash.

  network_state_handler_->RemoveObserver(this, FROM_HERE);
  host_scanner_->RemoveObserver(this);
}

void HostScanScheduler::UserLoggedIn() {
  if (!IsNetworkConnectingOrConnected(network_state_handler_->DefaultNetwork()))
    EnsureScan();
}

void HostScanScheduler::DefaultNetworkChanged(const NetworkState* network) {
  if (!IsNetworkConnectingOrConnected(network))
    EnsureScan();
}

void HostScanScheduler::ScanRequested() {
  if (!host_scanner_->HasRecentlyScanned())
    EnsureScan();
}

void HostScanScheduler::ScanFinished() {
  network_state_handler_->SetTetherScanState(false);
}

void HostScanScheduler::EnsureScan() {
  if (host_scanner_->IsScanActive())
    return;

  host_scanner_->StartScan();
  network_state_handler_->SetTetherScanState(true);
}

bool HostScanScheduler::IsNetworkConnectingOrConnected(
    const NetworkState* network) {
  return network &&
         (network->IsConnectingState() || network->IsConnectedState());
}

}  // namespace tether

}  // namespace chromeos
