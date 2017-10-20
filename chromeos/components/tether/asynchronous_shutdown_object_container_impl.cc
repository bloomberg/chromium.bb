// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/asynchronous_shutdown_object_container_impl.h"

#include "chromeos/components/tether/ble_advertisement_device_queue.h"
#include "chromeos/components/tether/ble_advertiser_impl.h"
#include "chromeos/components/tether/ble_connection_manager.h"
#include "chromeos/components/tether/ble_scanner_impl.h"
#include "chromeos/components/tether/ble_synchronizer.h"
#include "chromeos/components/tether/disconnect_tethering_request_sender_impl.h"
#include "chromeos/components/tether/network_configuration_remover.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/components/tether/wifi_hotspot_disconnector_impl.h"
#include "components/cryptauth/cryptauth_service.h"
#include "components/cryptauth/local_device_data_provider.h"
#include "components/cryptauth/remote_beacon_seed_fetcher.h"

namespace chromeos {

namespace tether {

// static
AsynchronousShutdownObjectContainerImpl::Factory*
    AsynchronousShutdownObjectContainerImpl::Factory::factory_instance_ =
        nullptr;

// static
std::unique_ptr<AsynchronousShutdownObjectContainer>
AsynchronousShutdownObjectContainerImpl::Factory::NewInstance(
    scoped_refptr<device::BluetoothAdapter> adapter,
    cryptauth::CryptAuthService* cryptauth_service,
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    NetworkConnectionHandler* network_connection_handler,
    PrefService* pref_service) {
  if (!factory_instance_)
    factory_instance_ = new Factory();

  return factory_instance_->BuildInstance(
      adapter, cryptauth_service, network_state_handler,
      managed_network_configuration_handler, network_connection_handler,
      pref_service);
}

// static
void AsynchronousShutdownObjectContainerImpl::Factory::SetInstanceForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

AsynchronousShutdownObjectContainerImpl::Factory::~Factory() {}

std::unique_ptr<AsynchronousShutdownObjectContainer>
AsynchronousShutdownObjectContainerImpl::Factory::BuildInstance(
    scoped_refptr<device::BluetoothAdapter> adapter,
    cryptauth::CryptAuthService* cryptauth_service,
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    NetworkConnectionHandler* network_connection_handler,
    PrefService* pref_service) {
  return base::MakeUnique<AsynchronousShutdownObjectContainerImpl>(
      adapter, cryptauth_service, network_state_handler,
      managed_network_configuration_handler, network_connection_handler,
      pref_service);
}

AsynchronousShutdownObjectContainerImpl::
    AsynchronousShutdownObjectContainerImpl(
        scoped_refptr<device::BluetoothAdapter> adapter,
        cryptauth::CryptAuthService* cryptauth_service,
        NetworkStateHandler* network_state_handler,
        ManagedNetworkConfigurationHandler*
            managed_network_configuration_handler,
        NetworkConnectionHandler* network_connection_handler,
        PrefService* pref_service)
    : adapter_(adapter),
      tether_host_fetcher_(
          base::MakeUnique<TetherHostFetcher>(cryptauth_service)),
      local_device_data_provider_(
          base::MakeUnique<cryptauth::LocalDeviceDataProvider>(
              cryptauth_service)),
      remote_beacon_seed_fetcher_(
          base::MakeUnique<cryptauth::RemoteBeaconSeedFetcher>(
              cryptauth_service->GetCryptAuthDeviceManager())),
      ble_advertisement_device_queue_(
          base::MakeUnique<BleAdvertisementDeviceQueue>()),
      ble_synchronizer_(base::MakeUnique<BleSynchronizer>(adapter)),
      ble_advertiser_(
          base::MakeUnique<BleAdvertiserImpl>(local_device_data_provider_.get(),
                                              remote_beacon_seed_fetcher_.get(),
                                              ble_synchronizer_.get())),
      ble_scanner_(
          base::MakeUnique<BleScannerImpl>(adapter,
                                           local_device_data_provider_.get(),
                                           ble_synchronizer_.get())),
      ble_connection_manager_(base::MakeUnique<BleConnectionManager>(
          cryptauth_service,
          adapter,
          ble_advertisement_device_queue_.get(),
          ble_advertiser_.get(),
          ble_scanner_.get())),
      disconnect_tethering_request_sender_(
          base::MakeUnique<DisconnectTetheringRequestSenderImpl>(
              ble_connection_manager_.get(),
              tether_host_fetcher_.get())),
      network_configuration_remover_(
          base::MakeUnique<NetworkConfigurationRemover>(
              network_state_handler,
              managed_network_configuration_handler)),
      wifi_hotspot_disconnector_(base::MakeUnique<WifiHotspotDisconnectorImpl>(
          network_connection_handler,
          network_state_handler,
          pref_service,
          network_configuration_remover_.get())) {}

AsynchronousShutdownObjectContainerImpl::
    ~AsynchronousShutdownObjectContainerImpl() {
  ble_advertiser_->RemoveObserver(this);
  ble_scanner_->RemoveObserver(this);
  disconnect_tethering_request_sender_->RemoveObserver(this);
}

void AsynchronousShutdownObjectContainerImpl::Shutdown(
    const base::Closure& shutdown_complete_callback) {
  DCHECK(shutdown_complete_callback_.is_null());
  shutdown_complete_callback_ = shutdown_complete_callback;

  // The objects below require asynchronous shutdowns, so start observering
  // these objects. Once they notify observers that they are finished shutting
  // down, the asynchronous shutdown will complete.
  ble_advertiser_->AddObserver(this);
  ble_scanner_->AddObserver(this);
  disconnect_tethering_request_sender_->AddObserver(this);

  ShutdownIfPossible();
}

TetherHostFetcher*
AsynchronousShutdownObjectContainerImpl::tether_host_fetcher() {
  return tether_host_fetcher_.get();
}

BleConnectionManager*
AsynchronousShutdownObjectContainerImpl::ble_connection_manager() {
  return ble_connection_manager_.get();
}

DisconnectTetheringRequestSender*
AsynchronousShutdownObjectContainerImpl::disconnect_tethering_request_sender() {
  return disconnect_tethering_request_sender_.get();
}

NetworkConfigurationRemover*
AsynchronousShutdownObjectContainerImpl::network_configuration_remover() {
  return network_configuration_remover_.get();
}

WifiHotspotDisconnector*
AsynchronousShutdownObjectContainerImpl::wifi_hotspot_disconnector() {
  return wifi_hotspot_disconnector_.get();
}

void AsynchronousShutdownObjectContainerImpl::
    OnAllAdvertisementsUnregistered() {
  ShutdownIfPossible();
}

void AsynchronousShutdownObjectContainerImpl::
    OnPendingDisconnectRequestsComplete() {
  ShutdownIfPossible();
}

void AsynchronousShutdownObjectContainerImpl::OnDiscoverySessionStateChanged(
    bool discovery_session_active) {
  ShutdownIfPossible();
}

void AsynchronousShutdownObjectContainerImpl::ShutdownIfPossible() {
  DCHECK(!shutdown_complete_callback_.is_null());

  if (AreAsynchronousOperationsActive())
    return;

  ble_advertiser_->RemoveObserver(this);
  ble_scanner_->RemoveObserver(this);
  disconnect_tethering_request_sender_->RemoveObserver(this);

  shutdown_complete_callback_.Run();
}

bool AsynchronousShutdownObjectContainerImpl::
    AreAsynchronousOperationsActive() {
  // All of the asynchronous shutdown procedures depend on Bluetooth. If
  // Bluetooth is off, there is no way to complete these tasks.
  if (!adapter_->IsPowered())
    return false;

  // If there are pending disconnection requests, they must be sent before the
  // component shuts down.
  if (disconnect_tethering_request_sender_->HasPendingRequests()) {
    return true;
  }

  // The BLE scanner must shut down completely before the component shuts down.
  if (ble_scanner_->ShouldDiscoverySessionBeActive() !=
      ble_scanner_->IsDiscoverySessionActive()) {
    return true;
  }

  // The BLE advertiser must unregister all of its advertisements.
  if (ble_advertiser_->AreAdvertisementsRegistered())
    return true;

  return false;
}

void AsynchronousShutdownObjectContainerImpl::SetTestDoubles(
    std::unique_ptr<BleAdvertiser> ble_advertiser,
    std::unique_ptr<BleScanner> ble_scanner,
    std::unique_ptr<DisconnectTetheringRequestSender>
        disconnect_tethering_request_sender) {
  ble_advertiser_ = std::move(ble_advertiser);
  ble_scanner_ = std::move(ble_scanner);
  disconnect_tethering_request_sender_ =
      std::move(disconnect_tethering_request_sender);
}

}  // namespace tether

}  // namespace chromeos
