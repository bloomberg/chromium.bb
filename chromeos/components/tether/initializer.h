// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_INITIALIZER_H_
#define CHROMEOS_COMPONENTS_TETHER_INITIALIZER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_clock.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"

class PrefService;

namespace cryptauth {
class CryptAuthService;
class LocalDeviceDataProvider;
class RemoteBeaconSeedFetcher;
}

namespace chromeos {

class ManagedNetworkConfigurationHandler;
class NetworkConnect;
class NetworkConnectionHandler;
class NetworkStateHandler;

namespace tether {

class ActiveHost;
class ActiveHostNetworkStateUpdater;
class BleConnectionManager;
class CrashRecoveryManager;
class NetworkConnectionHandlerTetherDelegate;
class DeviceIdTetherNetworkGuidMap;
class HostScanner;
class HostScanScheduler;
class HostScanDevicePrioritizerImpl;
class KeepAliveScheduler;
class HostConnectionMetricsLogger;
class MasterHostScanCache;
class NetworkConfigurationRemover;
class NetworkHostScanCache;
class NotificationPresenter;
class NotificationRemover;
class PersistentHostScanCache;
class TetherConnector;
class TetherDisconnector;
class TetherHostFetcher;
class TetherHostResponseRecorder;
class TetherNetworkDisconnectionHandler;
class WifiHotspotConnector;

// Initializes the Tether Chrome OS component.
class Initializer : public OAuth2TokenService::Observer {
 public:
  // Initializes the tether feature. If the feature has already been
  // initialized, this function is a no-op.
  static void Init(
      cryptauth::CryptAuthService* cryptauth_service,
      NotificationPresenter* notification_presenter,
      PrefService* pref_service,
      ProfileOAuth2TokenService* token_service,
      NetworkStateHandler* network_state_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      NetworkConnect* network_connect,
      NetworkConnectionHandler* network_connection_handler);

  // Shuts down the tether feature, destroying all internal classes. This should
  // be called before the dependencies passed to Init() are destroyed.
  static void Shutdown();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  friend class InitializerTest;

  static Initializer* instance_;

  Initializer(
      cryptauth::CryptAuthService* cryptauth_service,
      NotificationPresenter* notification_presenter,
      PrefService* pref_service,
      ProfileOAuth2TokenService* token_service,
      NetworkStateHandler* network_state_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      NetworkConnect* network_connect,
      NetworkConnectionHandler* network_connection_handler);
  ~Initializer() override;

  // OAuth2TokenService::Observer:
  void OnRefreshTokensLoaded() override;

  void FetchBluetoothAdapter();
  void OnBluetoothAdapterFetched(
      scoped_refptr<device::BluetoothAdapter> adapter);
  void OnBluetoothAdapterAdvertisingIntervalSet(
      scoped_refptr<device::BluetoothAdapter> adapter);
  void OnBluetoothAdapterAdvertisingIntervalError(
      device::BluetoothAdvertisement::ErrorCode status);
  void OnPreCrashStateRestored();

  cryptauth::CryptAuthService* cryptauth_service_;
  NotificationPresenter* notification_presenter_;
  PrefService* pref_service_;
  ProfileOAuth2TokenService* token_service_;
  NetworkStateHandler* network_state_handler_;
  ManagedNetworkConfigurationHandler* managed_network_configuration_handler_;
  NetworkConnect* network_connect_;
  NetworkConnectionHandler* network_connection_handler_;

  // Declare new objects in the order that they will be created during
  // initialization to ensure that they are destroyed in the correct order. This
  // order will be enforced by InitializerTest.TestCreateAndDestroy.
  std::unique_ptr<TetherHostFetcher> tether_host_fetcher_;
  std::unique_ptr<cryptauth::LocalDeviceDataProvider>
      local_device_data_provider_;
  std::unique_ptr<cryptauth::RemoteBeaconSeedFetcher>
      remote_beacon_seed_fetcher_;
  std::unique_ptr<BleConnectionManager> ble_connection_manager_;
  std::unique_ptr<TetherHostResponseRecorder> tether_host_response_recorder_;
  std::unique_ptr<HostScanDevicePrioritizerImpl> host_scan_device_prioritizer_;
  std::unique_ptr<WifiHotspotConnector> wifi_hotspot_connector_;
  std::unique_ptr<ActiveHost> active_host_;
  std::unique_ptr<ActiveHostNetworkStateUpdater>
      active_host_network_state_updater_;
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;
  std::unique_ptr<PersistentHostScanCache> persistent_host_scan_cache_;
  std::unique_ptr<NetworkHostScanCache> network_host_scan_cache_;
  std::unique_ptr<MasterHostScanCache> master_host_scan_cache_;
  std::unique_ptr<NotificationRemover> notification_remover_;
  std::unique_ptr<KeepAliveScheduler> keep_alive_scheduler_;
  std::unique_ptr<base::DefaultClock> clock_;
  std::unique_ptr<HostScanner> host_scanner_;
  std::unique_ptr<HostScanScheduler> host_scan_scheduler_;
  std::unique_ptr<HostConnectionMetricsLogger> host_connection_metrics_logger_;
  std::unique_ptr<TetherConnector> tether_connector_;
  std::unique_ptr<TetherDisconnector> tether_disconnector_;
  std::unique_ptr<NetworkConfigurationRemover> network_configuration_remover_;
  std::unique_ptr<NetworkConnectionHandlerTetherDelegate>
      network_connection_handler_tether_delegate_;
  std::unique_ptr<TetherNetworkDisconnectionHandler>
      tether_network_disconnection_handler_;
  std::unique_ptr<CrashRecoveryManager> crash_recovery_manager_;

  base::WeakPtrFactory<Initializer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Initializer);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_INITIALIZER_H_
