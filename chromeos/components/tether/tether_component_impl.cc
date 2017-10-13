// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_component_impl.h"

#include "base/bind.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/active_host_network_state_updater.h"
#include "chromeos/components/tether/ble_advertisement_device_queue.h"
#include "chromeos/components/tether/ble_connection_manager.h"
#include "chromeos/components/tether/ble_synchronizer.h"
#include "chromeos/components/tether/crash_recovery_manager.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/disconnect_tethering_request_sender.h"
#include "chromeos/components/tether/disconnect_tethering_request_sender_impl.h"
#include "chromeos/components/tether/host_connection_metrics_logger.h"
#include "chromeos/components/tether/host_scan_device_prioritizer_impl.h"
#include "chromeos/components/tether/host_scan_scheduler.h"
#include "chromeos/components/tether/host_scanner.h"
#include "chromeos/components/tether/keep_alive_scheduler.h"
#include "chromeos/components/tether/master_host_scan_cache.h"
#include "chromeos/components/tether/network_configuration_remover.h"
#include "chromeos/components/tether/network_connection_handler_tether_delegate.h"
#include "chromeos/components/tether/network_host_scan_cache.h"
#include "chromeos/components/tether/network_list_sorter.h"
#include "chromeos/components/tether/notification_presenter.h"
#include "chromeos/components/tether/notification_remover.h"
#include "chromeos/components/tether/persistent_host_scan_cache_impl.h"
#include "chromeos/components/tether/tether_connector_impl.h"
#include "chromeos/components/tether/tether_disconnector_impl.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"
#include "chromeos/components/tether/tether_network_disconnection_handler.h"
#include "chromeos/components/tether/timer_factory.h"
#include "chromeos/components/tether/wifi_hotspot_connector.h"
#include "chromeos/components/tether/wifi_hotspot_disconnector_impl.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/cryptauth/cryptauth_service.h"
#include "components/cryptauth/local_device_data_provider.h"
#include "components/cryptauth/remote_beacon_seed_fetcher.h"
#include "components/prefs/pref_service.h"
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
  if (!factory_instance_) {
    factory_instance_ = new Factory();
  }
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
  return base::WrapUnique(new TetherComponentImpl(
      cryptauth_service, notification_presenter, pref_service,
      network_state_handler, managed_network_configuration_handler,
      network_connect, network_connection_handler, adapter));
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
    : cryptauth_service_(cryptauth_service),
      notification_presenter_(notification_presenter),
      pref_service_(pref_service),
      network_state_handler_(network_state_handler),
      managed_network_configuration_handler_(
          managed_network_configuration_handler),
      network_connect_(network_connect),
      network_connection_handler_(network_connection_handler),
      adapter_(adapter),
      weak_ptr_factory_(this) {
  CreateComponent();
}

TetherComponentImpl::~TetherComponentImpl() {
  network_state_handler_->set_tether_sort_delegate(nullptr);

  if (disconnect_tethering_request_sender_)
    disconnect_tethering_request_sender_->RemoveObserver(this);
}

// Note: The asynchronous shutdown flow does not scale well (see
// crbug.com/761532).
// TODO(khorimoto): Refactor this flow.
void TetherComponentImpl::RequestShutdown() {
  // If shutdown has already happened, there is nothing else to do.
  if (status() != TetherComponent::Status::ACTIVE)
    return;

  // If there is an active connection, it needs to be disconnected before the
  // Tether component is shut down.
  if (active_host_->GetActiveHostStatus() !=
      ActiveHost::ActiveHostStatus::DISCONNECTED) {
    PA_LOG(INFO) << "There was an active connection during Tether shutdown. "
                 << "Initiating disconnection from device ID \""
                 << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(
                        active_host_->GetActiveHostDeviceId())
                 << "\".";
    tether_disconnector_->DisconnectFromNetwork(
        active_host_->GetTetherNetworkGuid(), base::Bind(&base::DoNothing),
        base::Bind(&OnDisconnectErrorDuringShutdown));
  }

  if (!IsAsyncShutdownRequired()) {
    TransitionToStatus(TetherComponent::Status::SHUT_DOWN);
    return;
  }

  TransitionToStatus(TetherComponent::Status::SHUTTING_DOWN);
  StartAsynchronousShutdown();
}

void TetherComponentImpl::OnAllAdvertisementsUnregistered() {
  FinishAsynchronousShutdownIfPossible();
}

void TetherComponentImpl::OnPendingDisconnectRequestsComplete() {
  FinishAsynchronousShutdownIfPossible();
}

void TetherComponentImpl::OnDiscoverySessionStateChanged(
    bool discovery_session_active) {
  FinishAsynchronousShutdownIfPossible();
}

void TetherComponentImpl::CreateComponent() {
  network_list_sorter_ = base::MakeUnique<NetworkListSorter>();
  network_state_handler_->set_tether_sort_delegate(network_list_sorter_.get());
  tether_host_fetcher_ =
      base::MakeUnique<TetherHostFetcher>(cryptauth_service_);
  local_device_data_provider_ =
      base::MakeUnique<cryptauth::LocalDeviceDataProvider>(cryptauth_service_);
  remote_beacon_seed_fetcher_ =
      base::MakeUnique<cryptauth::RemoteBeaconSeedFetcher>(
          cryptauth_service_->GetCryptAuthDeviceManager());
  ble_advertisement_device_queue_ =
      base::MakeUnique<BleAdvertisementDeviceQueue>();
  ble_synchronizer_ = base::MakeUnique<BleSynchronizer>(adapter_);
  ble_advertiser_ = base::MakeUnique<BleAdvertiser>(
      local_device_data_provider_.get(), remote_beacon_seed_fetcher_.get(),
      ble_synchronizer_.get());
  ble_scanner_ = base::MakeUnique<BleScanner>(
      adapter_, local_device_data_provider_.get(), ble_synchronizer_.get());
  ble_connection_manager_ = base::MakeUnique<BleConnectionManager>(
      cryptauth_service_, adapter_, ble_advertisement_device_queue_.get(),
      ble_advertiser_.get(), ble_scanner_.get());
  disconnect_tethering_request_sender_ =
      base::MakeUnique<DisconnectTetheringRequestSenderImpl>(
          ble_connection_manager_.get(), tether_host_fetcher_.get());
  tether_host_response_recorder_ =
      base::MakeUnique<TetherHostResponseRecorder>(pref_service_);
  device_id_tether_network_guid_map_ =
      base::MakeUnique<DeviceIdTetherNetworkGuidMap>();
  host_scan_device_prioritizer_ =
      base::MakeUnique<HostScanDevicePrioritizerImpl>(
          tether_host_response_recorder_.get());
  wifi_hotspot_connector_ = base::MakeUnique<WifiHotspotConnector>(
      network_state_handler_, network_connect_);
  active_host_ =
      base::MakeUnique<ActiveHost>(tether_host_fetcher_.get(), pref_service_);
  active_host_network_state_updater_ =
      base::MakeUnique<ActiveHostNetworkStateUpdater>(active_host_.get(),
                                                      network_state_handler_);
  persistent_host_scan_cache_ =
      base::MakeUnique<PersistentHostScanCacheImpl>(pref_service_);
  network_host_scan_cache_ = base::MakeUnique<NetworkHostScanCache>(
      network_state_handler_, tether_host_response_recorder_.get(),
      device_id_tether_network_guid_map_.get());
  master_host_scan_cache_ = base::MakeUnique<MasterHostScanCache>(
      base::MakeUnique<TimerFactory>(), active_host_.get(),
      network_host_scan_cache_.get(), persistent_host_scan_cache_.get());
  notification_remover_ = base::MakeUnique<NotificationRemover>(
      network_state_handler_, notification_presenter_,
      master_host_scan_cache_.get(), active_host_.get());
  keep_alive_scheduler_ = base::MakeUnique<KeepAliveScheduler>(
      active_host_.get(), ble_connection_manager_.get(),
      master_host_scan_cache_.get(), device_id_tether_network_guid_map_.get());
  clock_ = base::MakeUnique<base::DefaultClock>();
  host_scanner_ = base::MakeUnique<HostScanner>(
      network_state_handler_, tether_host_fetcher_.get(),
      ble_connection_manager_.get(), host_scan_device_prioritizer_.get(),
      tether_host_response_recorder_.get(), notification_presenter_,
      device_id_tether_network_guid_map_.get(), master_host_scan_cache_.get(),
      clock_.get());
  host_scan_scheduler_ = base::MakeUnique<HostScanScheduler>(
      network_state_handler_, host_scanner_.get());
  host_connection_metrics_logger_ =
      base::MakeUnique<HostConnectionMetricsLogger>();
  network_configuration_remover_ =
      base::MakeUnique<NetworkConfigurationRemover>(
          network_state_handler_, managed_network_configuration_handler_);
  wifi_hotspot_disconnector_ = base::MakeUnique<WifiHotspotDisconnectorImpl>(
      network_connection_handler_, network_state_handler_, pref_service_,
      network_configuration_remover_.get());
  tether_connector_ = base::MakeUnique<TetherConnectorImpl>(
      network_state_handler_, wifi_hotspot_connector_.get(), active_host_.get(),
      tether_host_fetcher_.get(), ble_connection_manager_.get(),
      tether_host_response_recorder_.get(),
      device_id_tether_network_guid_map_.get(), master_host_scan_cache_.get(),
      notification_presenter_, host_connection_metrics_logger_.get(),
      disconnect_tethering_request_sender_.get(),
      wifi_hotspot_disconnector_.get());
  tether_disconnector_ = base::MakeUnique<TetherDisconnectorImpl>(
      active_host_.get(), wifi_hotspot_disconnector_.get(),
      disconnect_tethering_request_sender_.get(), tether_connector_.get(),
      device_id_tether_network_guid_map_.get());
  tether_network_disconnection_handler_ =
      base::MakeUnique<TetherNetworkDisconnectionHandler>(
          active_host_.get(), network_state_handler_,
          network_configuration_remover_.get(),
          disconnect_tethering_request_sender_.get());
  network_connection_handler_tether_delegate_ =
      base::MakeUnique<NetworkConnectionHandlerTetherDelegate>(
          network_connection_handler_, active_host_.get(),
          tether_connector_.get(), tether_disconnector_.get());

  crash_recovery_manager_ = base::MakeUnique<CrashRecoveryManager>(
      network_state_handler_, active_host_.get(),
      master_host_scan_cache_.get());
  crash_recovery_manager_->RestorePreCrashStateIfNecessary(
      base::Bind(&TetherComponentImpl::OnPreCrashStateRestored,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool TetherComponentImpl::IsAsyncShutdownRequired() {
  // All of the asynchronous shutdown procedures depend on Bluetooth. If
  // Bluetooth is off, there is no way to complete these tasks.
  if (!adapter_->IsPowered())
    return false;

  // If there are pending disconnection requests, they must be sent before the
  // component shuts down.
  if (disconnect_tethering_request_sender_ &&
      disconnect_tethering_request_sender_->HasPendingRequests()) {
    return true;
  }

  // The BLE scanner must shut down completely before the component shuts down.
  if (ble_scanner_ && ble_scanner_->ShouldDiscoverySessionBeActive() !=
                          ble_scanner_->IsDiscoverySessionActive()) {
    return true;
  }

  // The BLE advertiser must unregister all of its advertisements.
  if (ble_advertiser_ && ble_advertiser_->AreAdvertisementsRegistered())
    return true;

  return false;
}

void TetherComponentImpl::OnPreCrashStateRestored() {
  // |crash_recovery_manager_| is no longer needed since it has completed.
  crash_recovery_manager_.reset();

  // Start a scan now that the Tether module has started up.
  host_scan_scheduler_->ScheduleScan();
}

void TetherComponentImpl::StartAsynchronousShutdown() {
  DCHECK(status() == TetherComponent::Status::SHUTTING_DOWN);
  DCHECK(disconnect_tethering_request_sender_->HasPendingRequests());

  // |ble_scanner_| and |disconnect_tethering_request_sender_| require
  // asynchronous shutdowns, so start observering these objects. Once they
  // notify observers that they are finished shutting down, asynchronous
  // shutdown will complete.
  ble_advertiser_->AddObserver(this);
  ble_scanner_->AddObserver(this);
  disconnect_tethering_request_sender_->AddObserver(this);

  // All objects which are not dependencies of |ble_scanner_| and
  // |disconnect_tethering_request_sender_| are no longer needed, so delete
  // them.
  crash_recovery_manager_.reset();
  network_connection_handler_tether_delegate_.reset();
  tether_network_disconnection_handler_.reset();
  tether_connector_.reset();
  host_connection_metrics_logger_.reset();
  host_scan_scheduler_.reset();
  host_scanner_.reset();
  clock_.reset();
  keep_alive_scheduler_.reset();
  notification_remover_.reset();
  master_host_scan_cache_.reset();
  network_host_scan_cache_.reset();
  persistent_host_scan_cache_.reset();
  active_host_network_state_updater_.reset();
  active_host_.reset();
  wifi_hotspot_connector_.reset();
  host_scan_device_prioritizer_.reset();
  device_id_tether_network_guid_map_.reset();
  tether_host_response_recorder_.reset();
  network_state_handler_->set_tether_sort_delegate(nullptr);
  network_list_sorter_.reset();
}

void TetherComponentImpl::FinishAsynchronousShutdownIfPossible() {
  DCHECK(status() == TetherComponent::Status::SHUTTING_DOWN);

  // If the asynchronous shutdown is not yet complete (i.e., only some of the
  // shutdown requirements are complete), do not shut down yet.
  if (IsAsyncShutdownRequired())
    return;

  ble_advertiser_->RemoveObserver(this);
  ble_scanner_->RemoveObserver(this);
  disconnect_tethering_request_sender_->RemoveObserver(this);

  // Shutdown has completed. It is now safe to delete the objects that were
  // shutting down asynchronously.
  wifi_hotspot_disconnector_.reset();
  network_configuration_remover_.reset();
  disconnect_tethering_request_sender_.reset();
  ble_connection_manager_.reset();
  ble_scanner_.reset();
  ble_advertiser_.reset();
  ble_synchronizer_.reset();
  ble_advertisement_device_queue_.reset();
  remote_beacon_seed_fetcher_.reset();
  local_device_data_provider_.reset();
  tether_host_fetcher_.reset();

  TransitionToStatus(TetherComponent::Status::SHUT_DOWN);
}

}  // namespace tether

}  // namespace chromeos
