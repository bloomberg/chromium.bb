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

namespace cryptauth {
class CryptAuthService;
}  // cryptauth

class PrefRegistrySimple;
class Profile;

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
  virtual void StartTetherIfEnabled();

  // Stop the Tether module.
  virtual void StopTether();

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
  void DeviceListChanged() override;

  // Callback when the controlling pref changes.
  void OnPrefsChanged();

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

  void OnBluetoothAdapterFetched(
      scoped_refptr<device::BluetoothAdapter> adapter);

  bool IsBluetoothAvailable() const;

  bool IsCellularAvailableButNotEnabled() const;

  // Whether Tether is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted if the flag is enabled.
  bool IsAllowedByPolicy() const;

  // Whether Tether is enabled.
  bool IsEnabledbyPreference() const;

  // Whether the service has been shut down.
  bool shut_down_ = false;

  // Whether the device and service have been suspended (e.g. the laptop lid
  // was closed).
  bool suspended_ = false;

  Profile* profile_;
  chromeos::PowerManagerClient* power_manager_client_;
  chromeos::SessionManagerClient* session_manager_client_;
  cryptauth::CryptAuthService* cryptauth_service_;
  chromeos::NetworkStateHandler* network_state_handler_;

  PrefChangeRegistrar registrar_;
  scoped_refptr<device::BluetoothAdapter> adapter_;

  base::WeakPtrFactory<TetherService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TetherService);
};

#endif  // CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_H_
