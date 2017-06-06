// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/initializer.h"

#include "base/bind.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/active_host_network_state_updater.h"
#include "chromeos/components/tether/ble_connection_manager.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/host_scan_cache.h"
#include "chromeos/components/tether/host_scan_device_prioritizer.h"
#include "chromeos/components/tether/host_scan_scheduler.h"
#include "chromeos/components/tether/host_scanner.h"
#include "chromeos/components/tether/keep_alive_scheduler.h"
#include "chromeos/components/tether/local_device_data_provider.h"
#include "chromeos/components/tether/network_configuration_remover.h"
#include "chromeos/components/tether/network_connection_handler_tether_delegate.h"
#include "chromeos/components/tether/notification_presenter.h"
#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/components/tether/tether_disconnector.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"
#include "chromeos/components/tether/tether_network_disconnection_handler.h"
#include "chromeos/components/tether/wifi_hotspot_connector.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/cryptauth/bluetooth_throttler_impl.h"
#include "components/cryptauth/cryptauth_service.h"
#include "components/cryptauth/remote_beacon_seed_fetcher.h"
#include "components/prefs/pref_service.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace chromeos {

namespace tether {

namespace {

// TODO (hansberry): Experiment with intervals to determine ideal advertising
//                   interval parameters.
constexpr int64_t kMinAdvertisingIntervalMilliseconds = 100;
constexpr int64_t kMaxAdvertisingIntervalMilliseconds = 100;

}  // namespace

// static
Initializer* Initializer::instance_ = nullptr;

// static
void Initializer::Init(
    cryptauth::CryptAuthService* cryptauth_service,
    std::unique_ptr<NotificationPresenter> notification_presenter,
    PrefService* pref_service,
    ProfileOAuth2TokenService* token_service,
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    NetworkConnect* network_connect,
    NetworkConnectionHandler* network_connection_handler) {
  if (!device::BluetoothAdapterFactory::IsBluetoothSupported()) {
    PA_LOG(WARNING) << "Bluetooth is not supported on this device; cannot "
                    << "initialize tether feature.";
    return;
  }

  if (instance_) {
    PA_LOG(WARNING) << "Tether initialization was triggered when the feature "
                    << "had already been initialized; exiting initialization "
                    << "early.";
    return;
  }

  instance_ =
      new Initializer(cryptauth_service, std::move(notification_presenter),
                      pref_service, token_service, network_state_handler,
                      managed_network_configuration_handler, network_connect,
                      network_connection_handler);
}

// static
void Initializer::Shutdown() {
  if (instance_) {
    PA_LOG(INFO) << "Shutting down Tether feature.";
    delete instance_;
    instance_ = nullptr;
  }
}

// static
void Initializer::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  ActiveHost::RegisterPrefs(registry);
  TetherHostResponseRecorder::RegisterPrefs(registry);
}

Initializer::Initializer(
    cryptauth::CryptAuthService* cryptauth_service,
    std::unique_ptr<NotificationPresenter> notification_presenter,
    PrefService* pref_service,
    ProfileOAuth2TokenService* token_service,
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    NetworkConnect* network_connect,
    NetworkConnectionHandler* network_connection_handler)
    : cryptauth_service_(cryptauth_service),
      notification_presenter_(std::move(notification_presenter)),
      pref_service_(pref_service),
      token_service_(token_service),
      network_state_handler_(network_state_handler),
      managed_network_configuration_handler_(
          managed_network_configuration_handler),
      network_connect_(network_connect),
      network_connection_handler_(network_connection_handler),
      weak_ptr_factory_(this) {
  if (!token_service_->RefreshTokenIsAvailable(
          cryptauth_service_->GetAccountId())) {
    PA_LOG(INFO) << "Refresh token not yet available; "
                 << "waiting for valid token to initializing tether feature.";
    token_service_->AddObserver(this);
    return;
  }

  PA_LOG(INFO) << "Refresh token is available; initializing tether feature.";
  FetchBluetoothAdapter();
}

Initializer::~Initializer() {
  token_service_->RemoveObserver(this);
}

void Initializer::FetchBluetoothAdapter() {
  device::BluetoothAdapterFactory::GetAdapter(base::Bind(
      &Initializer::OnBluetoothAdapterFetched, weak_ptr_factory_.GetWeakPtr()));
}

void Initializer::OnRefreshTokensLoaded() {
  if (!token_service_->RefreshTokenIsAvailable(
          cryptauth_service_->GetAccountId())) {
    // If a token for the active account is still not available, continue
    // waiting for a new token.
    return;
  }

  PA_LOG(INFO) << "Refresh token has loaded; initializing tether feature.";

  token_service_->RemoveObserver(this);
  FetchBluetoothAdapter();
}

void Initializer::OnBluetoothAdapterFetched(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  PA_LOG(INFO) << "Successfully fetched Bluetooth adapter. Setting advertising "
               << "interval.";

  adapter->SetAdvertisingInterval(
      base::TimeDelta::FromMilliseconds(kMinAdvertisingIntervalMilliseconds),
      base::TimeDelta::FromMilliseconds(kMaxAdvertisingIntervalMilliseconds),
      base::Bind(&Initializer::OnBluetoothAdapterAdvertisingIntervalSet,
                 weak_ptr_factory_.GetWeakPtr(), adapter),
      base::Bind(&Initializer::OnBluetoothAdapterAdvertisingIntervalError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void Initializer::OnBluetoothAdapterAdvertisingIntervalSet(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  PA_LOG(INFO) << "Successfully set Bluetooth advertisement interval. "
               << "Initializing tether feature.";

  tether_host_fetcher_ =
      base::MakeUnique<TetherHostFetcher>(cryptauth_service_);
  local_device_data_provider_ =
      base::MakeUnique<LocalDeviceDataProvider>(cryptauth_service_);
  remote_beacon_seed_fetcher_ =
      base::MakeUnique<cryptauth::RemoteBeaconSeedFetcher>(
          cryptauth_service_->GetCryptAuthDeviceManager());
  ble_connection_manager_ = base::MakeUnique<BleConnectionManager>(
      cryptauth_service_, adapter, local_device_data_provider_.get(),
      remote_beacon_seed_fetcher_.get(),
      cryptauth::BluetoothThrottlerImpl::GetInstance());
  tether_host_response_recorder_ =
      base::MakeUnique<TetherHostResponseRecorder>(pref_service_);
  host_scan_device_prioritizer_ = base::MakeUnique<HostScanDevicePrioritizer>(
      tether_host_response_recorder_.get());
  wifi_hotspot_connector_ = base::MakeUnique<WifiHotspotConnector>(
      network_state_handler_, network_connect_);
  active_host_ =
      base::MakeUnique<ActiveHost>(tether_host_fetcher_.get(), pref_service_);
  active_host_network_state_updater_ =
      base::MakeUnique<ActiveHostNetworkStateUpdater>(active_host_.get(),
                                                      network_state_handler_);
  device_id_tether_network_guid_map_ =
      base::MakeUnique<DeviceIdTetherNetworkGuidMap>();
  host_scan_cache_ = base::MakeUnique<HostScanCache>(
      network_state_handler_, active_host_.get(),
      tether_host_response_recorder_.get(),
      device_id_tether_network_guid_map_.get());
  keep_alive_scheduler_ = base::MakeUnique<KeepAliveScheduler>(
      active_host_.get(), ble_connection_manager_.get(), host_scan_cache_.get(),
      device_id_tether_network_guid_map_.get());
  clock_ = base::MakeUnique<base::DefaultClock>();
  host_scanner_ = base::MakeUnique<HostScanner>(
      tether_host_fetcher_.get(), ble_connection_manager_.get(),
      host_scan_device_prioritizer_.get(), tether_host_response_recorder_.get(),
      notification_presenter_.get(), device_id_tether_network_guid_map_.get(),
      host_scan_cache_.get(), clock_.get());
  host_scan_scheduler_ = base::MakeUnique<HostScanScheduler>(
      network_state_handler_, host_scanner_.get());
  tether_connector_ = base::MakeUnique<TetherConnector>(
      network_state_handler_, wifi_hotspot_connector_.get(), active_host_.get(),
      tether_host_fetcher_.get(), ble_connection_manager_.get(),
      tether_host_response_recorder_.get(),
      device_id_tether_network_guid_map_.get(), host_scan_cache_.get(),
      notification_presenter_.get());
  network_configuration_remover_ =
      base::MakeUnique<NetworkConfigurationRemover>(
          network_state_handler_, managed_network_configuration_handler_);
  tether_disconnector_ = base::MakeUnique<TetherDisconnector>(
      network_connection_handler_, network_state_handler_, active_host_.get(),
      ble_connection_manager_.get(), network_configuration_remover_.get(),
      tether_connector_.get(), device_id_tether_network_guid_map_.get(),
      tether_host_fetcher_.get());
  tether_network_disconnection_handler_ =
      base::MakeUnique<TetherNetworkDisconnectionHandler>(
          active_host_.get(), network_state_handler_,
          network_configuration_remover_.get());
  network_connection_handler_tether_delegate_ =
      base::MakeUnique<NetworkConnectionHandlerTetherDelegate>(
          network_connection_handler_, tether_connector_.get(),
          tether_disconnector_.get());

  // Because Initializer is created on each user log in, it's appropriate to
  // call this method now.
  host_scan_scheduler_->UserLoggedIn();
}

void Initializer::OnBluetoothAdapterAdvertisingIntervalError(
    device::BluetoothAdvertisement::ErrorCode status) {
  PA_LOG(ERROR) << "Failed to set Bluetooth advertisement interval; "
                << "cannot use tether feature. Error code: " << status;
}

}  // namespace tether

}  // namespace chromeos
