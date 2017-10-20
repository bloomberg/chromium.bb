// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_component_impl.h"

#include "base/bind.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/asynchronous_shutdown_object_container_impl.h"
#include "chromeos/components/tether/crash_recovery_manager_impl.h"
#include "chromeos/components/tether/host_scan_scheduler.h"
#include "chromeos/components/tether/persistent_host_scan_cache_impl.h"
#include "chromeos/components/tether/synchronous_shutdown_object_container_impl.h"
#include "chromeos/components/tether/tether_disconnector.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"
#include "chromeos/components/tether/wifi_hotspot_disconnector_impl.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

namespace {

void OnDisconnectErrorDuringShutdown(const std::string& error_name) {
  PA_LOG(WARNING) << "Error disconnecting from Tether network during shutdown; "
                  << "Error name: " << error_name;
}

}  // namespace

// static
TetherComponentImpl::Factory* TetherComponentImpl::Factory::factory_instance_ =
    nullptr;

// static
std::unique_ptr<TetherComponent> TetherComponentImpl::Factory::NewInstance(
    cryptauth::CryptAuthService* cryptauth_service,
    NotificationPresenter* notification_presenter,
    PrefService* pref_service,
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    NetworkConnect* network_connect,
    NetworkConnectionHandler* network_connection_handler,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!factory_instance_)
    factory_instance_ = new Factory();

  return factory_instance_->BuildInstance(
      cryptauth_service, notification_presenter, pref_service,
      network_state_handler, managed_network_configuration_handler,
      network_connect, network_connection_handler, adapter);
}

// static
void TetherComponentImpl::Factory::SetInstanceForTesting(Factory* factory) {
  factory_instance_ = factory;
}

// static
void TetherComponentImpl::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  ActiveHost::RegisterPrefs(registry);
  PersistentHostScanCacheImpl::RegisterPrefs(registry);
  TetherHostResponseRecorder::RegisterPrefs(registry);
  WifiHotspotDisconnectorImpl::RegisterPrefs(registry);
}

std::unique_ptr<TetherComponent> TetherComponentImpl::Factory::BuildInstance(
    cryptauth::CryptAuthService* cryptauth_service,
    NotificationPresenter* notification_presenter,
    PrefService* pref_service,
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    NetworkConnect* network_connect,
    NetworkConnectionHandler* network_connection_handler,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  return base::MakeUnique<TetherComponentImpl>(
      cryptauth_service, notification_presenter, pref_service,
      network_state_handler, managed_network_configuration_handler,
      network_connect, network_connection_handler, adapter);
}

TetherComponentImpl::TetherComponentImpl(
    cryptauth::CryptAuthService* cryptauth_service,
    NotificationPresenter* notification_presenter,
    PrefService* pref_service,
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    NetworkConnect* network_connect,
    NetworkConnectionHandler* network_connection_handler,
    scoped_refptr<device::BluetoothAdapter> adapter)
    : asynchronous_shutdown_object_container_(
          AsynchronousShutdownObjectContainerImpl::Factory::NewInstance(
              adapter,
              cryptauth_service,
              network_state_handler,
              managed_network_configuration_handler,
              network_connection_handler,
              pref_service)),
      synchronous_shutdown_object_container_(
          SynchronousShutdownObjectContainerImpl::Factory::NewInstance(
              asynchronous_shutdown_object_container_.get(),
              notification_presenter,
              pref_service,
              network_state_handler,
              network_connect,
              network_connection_handler)),
      crash_recovery_manager_(CrashRecoveryManagerImpl::Factory::NewInstance(
          network_state_handler,
          synchronous_shutdown_object_container_->active_host(),
          synchronous_shutdown_object_container_->host_scan_cache())),
      weak_ptr_factory_(this) {
  crash_recovery_manager_->RestorePreCrashStateIfNecessary(
      base::Bind(&TetherComponentImpl::OnPreCrashStateRestored,
                 weak_ptr_factory_.GetWeakPtr()));
}

TetherComponentImpl::~TetherComponentImpl() {}

void TetherComponentImpl::RequestShutdown() {
  has_shutdown_been_requested_ = true;

  // If shutdown has already happened, there is nothing else to do.
  if (status() != TetherComponent::Status::ACTIVE)
    return;

  // If crash recovery has not yet completed, wait for it to complete before
  // continuing.
  if (crash_recovery_manager_)
    return;

  InitiateShutdown();
}

void TetherComponentImpl::OnPreCrashStateRestored() {
  // |crash_recovery_manager_| is no longer needed since it has completed.
  crash_recovery_manager_.reset();

  if (has_shutdown_been_requested_) {
    InitiateShutdown();
    return;
  }

  // Start a scan now that the Tether module has started up.
  synchronous_shutdown_object_container_->host_scan_scheduler()->ScheduleScan();
}

void TetherComponentImpl::InitiateShutdown() {
  DCHECK(has_shutdown_been_requested_);
  DCHECK(!crash_recovery_manager_);
  DCHECK(status() == TetherComponent::Status::ACTIVE);

  ActiveHost* active_host =
      synchronous_shutdown_object_container_->active_host();
  TetherDisconnector* tether_disconnector =
      synchronous_shutdown_object_container_->tether_disconnector();

  // If there is an active connection, it needs to be disconnected before the
  // Tether component is shut down.
  if (active_host->GetActiveHostStatus() !=
      ActiveHost::ActiveHostStatus::DISCONNECTED) {
    PA_LOG(INFO) << "There was an active connection during Tether shutdown. "
                 << "Initiating disconnection from device ID \""
                 << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(
                        active_host->GetActiveHostDeviceId())
                 << "\".";
    tether_disconnector->DisconnectFromNetwork(
        active_host->GetTetherNetworkGuid(), base::Bind(&base::DoNothing),
        base::Bind(&OnDisconnectErrorDuringShutdown));
  }

  TransitionToStatus(TetherComponent::Status::SHUTTING_DOWN);

  // Delete objects which can shutdown synchronously immediately.
  synchronous_shutdown_object_container_.reset();

  // Start the shutdown process for objects which shutdown asynchronously.
  asynchronous_shutdown_object_container_->Shutdown(
      base::Bind(&TetherComponentImpl::OnShutdownComplete,
                 weak_ptr_factory_.GetWeakPtr()));
}

void TetherComponentImpl::OnShutdownComplete() {
  DCHECK(status() == TetherComponent::Status::SHUTTING_DOWN);

  // Shutdown has completed. The asynchronous objects can now be deleted as
  // well.
  asynchronous_shutdown_object_container_.reset();

  TransitionToStatus(TetherComponent::Status::SHUT_DOWN);
}

}  // namespace tether

}  // namespace chromeos
