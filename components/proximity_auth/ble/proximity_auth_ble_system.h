// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLE_PROXIMITY_AUTH_BLE_SYSTEM_H_
#define COMPONENTS_PROXIMITY_AUTH_BLE_PROXIMITY_AUTH_BLE_SYSTEM_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/proximity_auth/connection_observer.h"
#include "components/proximity_auth/cryptauth/cryptauth_client.h"
#include "components/proximity_auth/screenlock_bridge.h"

class PrefRegistrySimple;
class PrefService;

namespace device {
class BluetoothGattConnection;
}

namespace proximity_auth {

class BluetoothLowEnergyConnection;
class BluetoothLowEnergyConnectionFinder;
class BluetoothLowEnergyDeviceWhitelist;
class BluetoothThrottler;
class Connection;
class ConnectionFinder;
class ProximityAuthClient;

// This is the main entry point to start Proximity Auth over Bluetooth Low
// Energy. This is the underlying system for the Smart Lock features. It will
// discover Bluetooth Low Energy phones and unlock the lock screen if the phone
// passes an authorization and authentication protocol.
class ProximityAuthBleSystem : public ScreenlockBridge::Observer,
                               public ConnectionObserver {
 public:
  ProximityAuthBleSystem(
      ScreenlockBridge* screenlock_bridge,
      ProximityAuthClient* proximity_auth_client,
      scoped_ptr<CryptAuthClientFactory> cryptauth_client_factory,
      PrefService* pref_service);
  ~ProximityAuthBleSystem() override;

  // Registers the prefs used by this class
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // ScreenlockBridge::Observer:
  void OnScreenDidLock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnFocusedUserChanged(const std::string& user_id) override;

  // proximity_auth::ConnectionObserver:
  void OnConnectionStatusChanged(Connection* connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override;
  void OnMessageReceived(const Connection& connection,
                         const WireMessage& message) override;

  // Called by EasyUnlockService when the user clicks their user pod and
  // initiates an auth attempt.
  void OnAuthAttempted(const std::string& user_id);

 protected:
  // Virtual for testing.
  virtual ConnectionFinder* CreateConnectionFinder();

 private:
  // Fetches the the public keys of devices that can be used as unlock keys.
  void GetUnlockKeys();

  // Checks if the devices in |device_whitelist_| have their public keys
  // registered in CryptAuth (|unlock_keys_|), removes the ones that do not.
  void RemoveStaleWhitelistedDevices();

  // Callbacks for cryptauth::CryptAuthClient::GetMyDevices.
  void OnGetMyDevices(const cryptauth::GetMyDevicesResponse& response);
  void OnGetMyDevicesError(const std::string& error);

  // Handler for a new connection found event.
  void OnConnectionFound(scoped_ptr<Connection> connection);

  // Start (recurrently) polling every |polling_interval_| ms for the screen
  // state of the remote device.
  void StartPollingScreenState();

  // Stop polling for screen state of the remote device, if currently active.
  void StopPollingScreenState();

  // Checks if |message| contains a valid public key (registered in
  // |unlock_keys_|). If so, returns the public key in |out_public_key|.
  bool HasUnlockKey(const std::string& message, std::string* out_public_key);

  // Called when |spinner_timer_| is fired to stop showing the spinner on the
  // user pod.
  void OnSpinnerTimerFired();

  // Called to update the lock screen's UI for the current authentication state
  // with the remote device.
  void UpdateLockScreenUI();

  ScreenlockBridge* screenlock_bridge_;

  // Not owned. Must outlive this object.
  ProximityAuthClient* proximity_auth_client_;

  // Creates CryptAuth client instances to make API calls.
  scoped_ptr<CryptAuthClientFactory> cryptauth_client_factory_;

  // We only support one concurrent API call.
  scoped_ptr<CryptAuthClient> cryptauth_client_;

  // Maps devices public keys to the device friendly name.
  std::map<std::string, std::string> unlock_keys_;

  scoped_ptr<ConnectionFinder> connection_finder_;

  scoped_ptr<Connection> connection_;

  scoped_ptr<BluetoothLowEnergyDeviceWhitelist> device_whitelist_;

  scoped_ptr<BluetoothThrottler> bluetooth_throttler_;

  const base::TimeDelta polling_interval_;

  // True if the remote device sent public key contained in |unlock_keyes_| or
  // |device_whitelist_|.
  bool device_authenticated_;

  // True if |this| instance is currently polling the phone for the phone's
  // screenlock state.
  bool is_polling_screen_state_;

  // True if a call to |GetUnlockKeys()| was already made.
  bool unlock_keys_requested_;

  // The user id or email of the last focused user on the lock screen.
  std::string last_focused_user_;

  // True if the the last status update from the phone reports that the phone's
  // screen is locked. If no status update has been received, this value is true
  // by default.
  bool is_remote_screen_locked_;

  // The timer controlling the time the spinner for the user pod icon is shown
  // right after the screen is locked.
  base::OneShotTimer<ProximityAuthBleSystem> spinner_timer_;

  // The different UI states that the lock screen can be in.
  enum class ScreenlockUIState {
    NO_SCREENLOCK,
    SPINNER,
    UNAUTHENTICATED,
    AUTHENTICATED
  };
  ScreenlockUIState screenlock_ui_state_;

  base::WeakPtrFactory<ProximityAuthBleSystem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthBleSystem);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLE_PROXIMITY_AUTH_BLE_SYSTEM_H_
