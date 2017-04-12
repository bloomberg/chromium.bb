// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/initializer.h"

#include "base/bind.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/active_host_network_state_updater.h"
#include "chromeos/components/tether/ble_connection_manager.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/host_scan_device_prioritizer.h"
#include "chromeos/components/tether/host_scan_scheduler.h"
#include "chromeos/components/tether/host_scanner.h"
#include "chromeos/components/tether/local_device_data_provider.h"
#include "chromeos/components/tether/notification_presenter.h"
#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/components/tether/wifi_hotspot_connector.h"
#include "chromeos/network/network_connect.h"
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
    NetworkConnect* network_connect) {
  if (!device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    PA_LOG(WARNING) << "Bluetooth is unavailable on this device; cannot "
                    << "initialize tether feature.";
    return;
  }

  if (instance_) {
    PA_LOG(WARNING) << "Tether initialization was triggered when the feature "
                    << "had already been initialized; exiting initialization "
                    << "early.";
    return;
  }

  instance_ = new Initializer(
      cryptauth_service, std::move(notification_presenter), pref_service,
      token_service, network_state_handler, network_connect);
}

// static
void Initializer::Shutdown() {
  if (instance_) {
    PA_LOG(INFO) << "Shutting down tether feature.";
    delete instance_;
    instance_ = nullptr;
  }
}

// static
void Initializer::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  ActiveHost::RegisterPrefs(registry);
  HostScanDevicePrioritizer::RegisterPrefs(registry);
}

Initializer::Initializer(
    cryptauth::CryptAuthService* cryptauth_service,
    std::unique_ptr<NotificationPresenter> notification_presenter,
    PrefService* pref_service,
    ProfileOAuth2TokenService* token_service,
    NetworkStateHandler* network_state_handler,
    NetworkConnect* network_connect)
    : cryptauth_service_(cryptauth_service),
      notification_presenter_(std::move(notification_presenter)),
      pref_service_(pref_service),
      token_service_(token_service),
      network_state_handler_(network_state_handler),
      network_connect_(network_connect),
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
  host_scan_device_prioritizer_ =
      base::MakeUnique<HostScanDevicePrioritizer>(pref_service_);
  wifi_hotspot_connector_ = base::MakeUnique<WifiHotspotConnector>(
      network_state_handler_, network_connect_);
  active_host_ =
      base::MakeUnique<ActiveHost>(tether_host_fetcher_.get(), pref_service_);
  active_host_network_state_updater_ =
      base::MakeUnique<ActiveHostNetworkStateUpdater>(active_host_.get(),
                                                      network_state_handler_);
  device_id_tether_network_guid_map_ =
      base::MakeUnique<DeviceIdTetherNetworkGuidMap>();
  tether_connector_ = base::MakeUnique<TetherConnector>(
      network_connect_, network_state_handler_, wifi_hotspot_connector_.get(),
      active_host_.get(), tether_host_fetcher_.get(),
      ble_connection_manager_.get(), host_scan_device_prioritizer_.get(),
      device_id_tether_network_guid_map_.get());
  host_scanner_ = base::MakeUnique<HostScanner>(
      tether_host_fetcher_.get(), ble_connection_manager_.get(),
      host_scan_device_prioritizer_.get(), network_state_handler_,
      notification_presenter_.get());

  // TODO(khorimoto): Hook up HostScanScheduler. Currently, we simply start a
  // new scan once the user logs in.
  host_scanner_->StartScan();
}

void Initializer::OnBluetoothAdapterAdvertisingIntervalError(
    device::BluetoothAdvertisement::ErrorCode status) {
  PA_LOG(ERROR) << "Failed to set Bluetooth advertisement interval; "
                << "cannot use tether feature. Error code: " << status;
}

}  // namespace tether

}  // namespace chromeos
