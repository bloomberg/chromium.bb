// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_scheduler.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/tether/host_scanner.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

HostScanScheduler::ContextImpl::ContextImpl(
    const content::BrowserContext* browser_context) {
  // TODO(khorimoto): Use browser_context to get a CryptAuthDeviceManager.
}

void HostScanScheduler::ContextImpl::AddObserver(
    HostScanScheduler* host_scan_scheduler) {
  LoginState::Get()->AddObserver(host_scan_scheduler);
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      host_scan_scheduler);
  NetworkHandler::Get()->network_state_handler()->AddObserver(
      host_scan_scheduler, FROM_HERE);
  // TODO(khorimoto): Add listener for CryptAuthDeviceManager.
}

void HostScanScheduler::ContextImpl::RemoveObserver(
    HostScanScheduler* host_scan_scheduler) {
  LoginState::Get()->RemoveObserver(host_scan_scheduler);
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      host_scan_scheduler);
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(
      host_scan_scheduler, FROM_HERE);
  // TODO(khorimoto): Add observer of CryptAuthDeviceManager.
}

bool HostScanScheduler::ContextImpl::IsAuthenticatedUserLoggedIn() const {
  LoginState* login_state = LoginState::Get();
  return login_state && login_state->IsUserLoggedIn() &&
         login_state->IsUserAuthenticated();
}

bool HostScanScheduler::ContextImpl::IsNetworkConnectedOrConnecting() const {
  const NetworkState* network_state =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  return network_state && (network_state->IsConnectedState() ||
                           network_state->IsConnectingState());
}

bool HostScanScheduler::ContextImpl::AreTetherHostsSynced() const {
  // TODO(khorimoto): Return CryptAuthDeviceManager->GetTetherHosts().empty().
  return true;
}

HostScanScheduler::HostScanScheduler(
    const content::BrowserContext* browser_context,
    std::unique_ptr<HostScanner> host_scanner)
    : HostScanScheduler(base::MakeUnique<ContextImpl>(browser_context),
                        std::move(host_scanner)) {}

HostScanScheduler::~HostScanScheduler() {
  if (initialized_) {
    context_->RemoveObserver(this);
  }
}

HostScanScheduler::HostScanScheduler(std::unique_ptr<Context> context,
                                     std::unique_ptr<HostScanner> host_scanner)
    : context_(std::move(context)),
      host_scanner_(std::move(host_scanner)),
      initialized_(false) {}

void HostScanScheduler::InitializeAutomaticScans() {
  if (initialized_) {
    return;
  }

  initialized_ = true;
  context_->AddObserver(this);
}

bool HostScanScheduler::ScheduleScanNowIfPossible() {
  if (!context_->IsAuthenticatedUserLoggedIn()) {
    PA_LOG(INFO) << "Authenticated user not logged in; not starting scan.";
    return false;
  }

  if (context_->IsNetworkConnectedOrConnecting()) {
    PA_LOG(INFO)
        << "Network is already connected/connecting; not starting scan.";
    return false;
  }

  if (!context_->AreTetherHostsSynced()) {
    PA_LOG(INFO) << "No tether hosts available on account; not starting scan.";
    return false;
  }

  host_scanner_->StartScan();
  return true;
}

void HostScanScheduler::LoggedInStateChanged() {
  PA_LOG(INFO) << "Received login state change.";
  ScheduleScanNowIfPossible();
}

void HostScanScheduler::SuspendDone(const base::TimeDelta& sleep_duration) {
  PA_LOG(INFO) << "Device has resumed from sleeping.";
  ScheduleScanNowIfPossible();
}

void HostScanScheduler::NetworkConnectionStateChanged(
    const NetworkState* network) {
  PA_LOG(INFO) << "Received network connection state change.";
  ScheduleScanNowIfPossible();
}

void HostScanScheduler::OnSyncFinished(
    cryptauth::CryptAuthDeviceManager::SyncResult sync_result,
    cryptauth::CryptAuthDeviceManager::DeviceChangeResult
        device_change_result) {
  PA_LOG(INFO) << "CryptAuth device sync finished.";
  ScheduleScanNowIfPossible();
}

}  // namespace tether

}  // namespace chromeos
