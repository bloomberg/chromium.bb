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
#include "components/proximity_auth/connection_observer.h"
#include "components/proximity_auth/cryptauth/cryptauth_client.h"
#include "components/proximity_auth/screenlock_bridge.h"

namespace content {
class BrowserContext;
}

namespace device {
class BluetoothGattConnection;
}

namespace proximity_auth {

class BluetoothLowEnergyConnection;
class BluetoothLowEnergyConnectionFinder;
class Connection;
class ConnectionFinder;

// This is the main entry point to start Proximity Auth over Bluetooth Low
// Energy. This is the underlying system for the Smart Lock features. It will
// discover Bluetooth Low Energy phones and unlock the lock screen if the phone
// passes an authorization and authentication protocol.
class ProximityAuthBleSystem : public ScreenlockBridge::Observer,
                               public ConnectionObserver {
 public:
  ProximityAuthBleSystem(
      ScreenlockBridge* screenlock_bridge,
      content::BrowserContext* browser_context,
      scoped_ptr<CryptAuthClientFactory> cryptauth_client_factory);
  ~ProximityAuthBleSystem() override;

  // ScreenlockBridge::Observer:
  void OnScreenDidLock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnFocusedUserChanged(const std::string& user_id) override;

  // proximity_auth::ConnectionObserver:
  void OnMessageReceived(const Connection& connection,
                         const WireMessage& message) override;

 protected:
  class ScreenlockBridgeAdapter {
   public:
    ScreenlockBridgeAdapter(ScreenlockBridge* screenlock_bridge);
    virtual ~ScreenlockBridgeAdapter();

    virtual void AddObserver(ScreenlockBridge::Observer* observer);
    virtual void RemoveObserver(ScreenlockBridge::Observer* observer);
    virtual void Unlock(content::BrowserContext* browser_context);

   protected:
    ScreenlockBridgeAdapter();

   private:
    // Not owned. Must outlive this object.
    ScreenlockBridge* screenlock_bridge_;
  };

  // Used for testing.
  ProximityAuthBleSystem(ScreenlockBridgeAdapter* screenlock_bridge,
                         content::BrowserContext* browser_context);

  // Virtual for testing.
  virtual ConnectionFinder* CreateConnectionFinder();

 private:
  // Fetches the the public keys of devices that can be used as unlock keys.
  void GetUnlockKeys();

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

  scoped_ptr<ScreenlockBridgeAdapter> screenlock_bridge_;

  content::BrowserContext*
      browser_context_;  // Not owned. Must outlive this object.

  // Creates CryptAuth client instances to make API calls.
  scoped_ptr<CryptAuthClientFactory> cryptauth_client_factory_;

  // We only support one concurrent API call.
  scoped_ptr<CryptAuthClient> cryptauth_client_;

  // Maps devices public keys to the device friendly name.
  std::map<std::string, std::string> unlock_keys_;

  scoped_ptr<ConnectionFinder> connection_finder_;

  scoped_ptr<Connection> connection_;

  const base::TimeDelta polling_interval_;

  bool is_polling_screen_state_;

  base::WeakPtrFactory<ProximityAuthBleSystem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthBleSystem);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLE_PROXIMITY_AUTH_BLE_SYSTEM_H_
