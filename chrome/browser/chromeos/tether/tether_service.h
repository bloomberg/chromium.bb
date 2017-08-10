// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {
class NetworkStateHandler;
class ManagedNetworkConfigurationHandler;
class NetworkConnect;
class NetworkConnectionHandler;
namespace tether {
class NotificationPresenter;
}  // namespace tether
}  // namespace chromeos

namespace cryptauth {
class CryptAuthService;
}  // namespace cryptauth

class PrefRegistrySimple;
class Profile;
class ProfileOAuth2TokenService;

class TetherService : public KeyedService,
                      public chromeos::PowerManagerClient::Observer,
                      public chromeos::SessionManagerClient::Observer,
                      public cryptauth::CryptAuthDeviceManager::Observer,
                      public device::BluetoothAdapter::Observer,
                      public chromeos::NetworkStateHandlerObserver {
 public:
  TetherService(Profile* profile,
                chromeos::PowerManagerClient* power_manager_client,
                chromeos::SessionManagerClient* session_manager_client,
                cryptauth::CryptAuthService* cryptauth_service,
                chromeos::NetworkStateHandler* network_state_handler);
  ~TetherService() override;

  // Gets TetherService instance.
  static TetherService* Get(Profile* profile);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Whether the Tether feature has been enabled via a chrome://about or
  // command line flag.
  static bool IsFeatureFlagEnabled();

  // Attempt to start the Tether module. Only succeeds if all conditions to
  // reach chromeos::NetworkStateHandler::TechnologyState::ENABLED are reached.
  // Should only be called once a user is logged in.
  virtual void StartTetherIfPossible();

  // Delegate used to call the static functions of Initializer. Injected to
  // aid in testing.
  class InitializerDelegate {
   public:
    virtual void InitializeTether(
        cryptauth::CryptAuthService* cryptauth_service,
        chromeos::tether::NotificationPresenter* notification_presenter,
        PrefService* pref_service,
        ProfileOAuth2TokenService* token_service,
        chromeos::NetworkStateHandler* network_state_handler,
        chromeos::ManagedNetworkConfigurationHandler*
            managed_network_configuration_handler,
        chromeos::NetworkConnect* network_connect,
        chromeos::NetworkConnectionHandler* network_connection_handler,
        scoped_refptr<device::BluetoothAdapter> adapter);
    virtual void ShutdownTether();
  };

 protected:
  // KeyedService:
  void Shutdown() override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent() override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // chromeos::SessionManagerClient::Observer:
  void ScreenIsLocked() override;
  void ScreenIsUnlocked() override;

  // cryptauth::CryptAuthDeviceManager::Observer
  void OnSyncFinished(cryptauth::CryptAuthDeviceManager::SyncResult sync_result,
                      cryptauth::CryptAuthDeviceManager::DeviceChangeResult
                          device_change_result) override;

  // device::BluetoothAdapter::Observer:
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  // chromeos::NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;
  void DeviceListChanged() override;

  // Callback when the controlling pref changes.
  void OnPrefsChanged();

  // Stop the Tether module if it is currently enabled; if it was not enabled,
  // this function is a no-op.
  virtual void StopTetherIfNecessary();

  // Whether Tether hosts are available.
  virtual bool HasSyncedTetherHosts() const;

  virtual void UpdateTetherTechnologyState();
  chromeos::NetworkStateHandler::TechnologyState GetTetherTechnologyState();

  chromeos::NetworkStateHandler* network_state_handler() {
    return network_state_handler_;
  }

 private:
  friend class TetherServiceTest;
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestFeatureFlagEnabled);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestBluetoothNotification);

  void OnBluetoothAdapterFetched(
      scoped_refptr<device::BluetoothAdapter> adapter);
  void OnBluetoothAdapterAdvertisingIntervalSet();
  void OnBluetoothAdapterAdvertisingIntervalError(
      device::BluetoothAdvertisement::ErrorCode status);

  void SetBleAdvertisingInterval();

  // Whether BLE advertising is supported on this device. This should only
  // return true if a call to BluetoothAdapter::SetAdvertisingInterval() during
  // TetherService construction succeeds. That method will fail in cases like
  // those captured in crbug.com/738222.
  bool GetIsBleAdvertisingSupportedPref();
  void SetIsBleAdvertisingSupportedPref(bool is_ble_advertising_supported);

  bool IsBluetoothAvailable() const;

  bool IsCellularAvailableButNotEnabled() const;

  // Whether Tether is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted if the flag is enabled.
  bool IsAllowedByPolicy() const;

  // Whether Tether is enabled.
  bool IsEnabledbyPreference() const;

  // Returns whether the "enable Bluetooth" notification can be shown under the
  // current conditions.
  bool CanEnableBluetoothNotificationBeShown();

  void SetInitializerDelegateForTest(
      std::unique_ptr<InitializerDelegate> initializer_delegate);
  void SetNotificationPresenterForTest(
      std::unique_ptr<chromeos::tether::NotificationPresenter>
          notification_presenter);

  // Whether the service has been shut down.
  bool shut_down_ = false;

  // Whether the device and service have been suspended (e.g. the laptop lid
  // was closed).
  bool suspended_ = false;

  bool has_attempted_to_set_ble_advertising_interval_ = false;

  Profile* profile_;
  chromeos::PowerManagerClient* power_manager_client_;
  chromeos::SessionManagerClient* session_manager_client_;
  cryptauth::CryptAuthService* cryptauth_service_;
  chromeos::NetworkStateHandler* network_state_handler_;
  std::unique_ptr<InitializerDelegate> initializer_delegate_;
  std::unique_ptr<chromeos::tether::NotificationPresenter>
      notification_presenter_;

  PrefChangeRegistrar registrar_;
  scoped_refptr<device::BluetoothAdapter> adapter_;

  base::WeakPtrFactory<TetherService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TetherService);
};

#endif  // CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_H_
