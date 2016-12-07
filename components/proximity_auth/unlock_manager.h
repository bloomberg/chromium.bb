// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_UNLOCK_MANAGER_H
#define COMPONENTS_PROXIMITY_AUTH_UNLOCK_MANAGER_H

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/proximity_auth/messenger_observer.h"
#include "components/proximity_auth/proximity_auth_system.h"
#include "components/proximity_auth/remote_device_life_cycle.h"
#include "components/proximity_auth/remote_status_update.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/proximity_auth/screenlock_state.h"
#include "device/bluetooth/bluetooth_adapter.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/power_manager_client.h"
#endif

namespace proximity_auth {

class Messenger;
class ProximityAuthClient;
class ProximityMonitor;

// The unlock manager is responsible for controlling the lock screen UI based on
// the authentication status of the registered remote devices.
class UnlockManager : public MessengerObserver,
                      public ScreenlockBridge::Observer,
#if defined(OS_CHROMEOS)
                      chromeos::PowerManagerClient::Observer,
#endif  // defined(OS_CHROMEOS)
                      public device::BluetoothAdapter::Observer {
 public:
  // The |proximity_auth_client| is not owned and should outlive the constructed
  // unlock manager.
  UnlockManager(ProximityAuthSystem::ScreenlockType screenlock_type,
                ProximityAuthClient* proximity_auth_client);
  ~UnlockManager() override;

  // Whether proximity-based unlocking is currently allowed. True if any one of
  // the remote devices is authenticated and in range.
  bool IsUnlockAllowed();

  // Sets the |life_cycle| of the rmeote device to which local events are
  // dispatched. A null |life_cycle| indicates that proximity-based
  // authentication is inactive.
  void SetRemoteDeviceLifeCycle(RemoteDeviceLifeCycle* life_cycle);

  // Called when the life cycle's state changes.
  void OnLifeCycleStateChanged();

  // Called when the user pod is clicked for an authentication attempt of type
  // |auth_type|.
  // Exposed for testing.
  void OnAuthAttempted(ScreenlockBridge::LockHandler::AuthType auth_type);

 protected:
  // Creates a ProximityMonitor instance for the given |remote_device|.
  // Exposed for testing.
  virtual std::unique_ptr<ProximityMonitor> CreateProximityMonitor(
      const cryptauth::RemoteDevice& remote_device);

 private:
  // The possible lock screen states for the remote device.
  enum class RemoteScreenlockState {
    UNKNOWN,
    UNLOCKED,
    DISABLED,
    LOCKED,
  };

  // MessengerObserver:
  void OnUnlockEventSent(bool success) override;
  void OnRemoteStatusUpdate(const RemoteStatusUpdate& status_update) override;
  void OnDecryptResponse(const std::string& decrypted_bytes) override;
  void OnUnlockResponse(bool success) override;
  void OnDisconnected() override;

  // ScreenlockBridge::Observer
  void OnScreenDidLock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnFocusedUserChanged(const AccountId& account_id) override;

  // Called when the screenlock state changes.
  void OnScreenLockedOrUnlocked(bool is_locked);

  // Called when the Bluetooth adapter is initialized.
  void OnBluetoothAdapterInitialized(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

#if defined(OS_CHROMEOS)
  // chromeos::PowerManagerClient::Observer:
  void SuspendDone(const base::TimeDelta& sleep_duration) override;
#endif  // defined(OS_CHROMEOS)

  // Called when auth is attempted to send the sign-in challenge to the remote
  // device for decryption.
  void SendSignInChallenge();

  // Called with the sign-in |challenge| so we can send it to the remote device
  // for decryption.
  void OnGotSignInChallenge(const std::string& challenge);

  // Returns the current state for the screen lock UI.
  ScreenlockState GetScreenlockState();

  // Updates the lock screen based on the manager's current state.
  void UpdateLockScreen();

  // Activates or deactivates the proximity monitor, as appropriate given the
  // current state of |this| unlock manager.
  void UpdateProximityMonitorState();

  // Sets waking up state.
  void SetWakingUpState(bool is_waking_up);

  // Accepts or rejects the current auth attempt according to |should_accept|.
  // If the auth attempt is accepted, unlocks the screen.
  void AcceptAuthAttempt(bool should_accept);

  // Returns the screen lock state corresponding to the given remote |status|
  // update.
  RemoteScreenlockState GetScreenlockStateFromRemoteUpdate(
      RemoteStatusUpdate update);

  // Returns the Messenger instance associated with |life_cycle_|. This function
  // will return nullptr if |life_cycle_| is not set or the remote device is not
  // yet authenticated.
  Messenger* GetMessenger();

  // Whether |this| manager is being used for sign-in or session unlock.
  const ProximityAuthSystem::ScreenlockType screenlock_type_;

  // Whether the user is present at the remote device. Unset if no remote status
  // update has yet been received.
  std::unique_ptr<RemoteScreenlockState> remote_screenlock_state_;

  // Controls the proximity auth flow logic for a remote device. Not owned, and
  // expcted to outlive |this| instance.
  RemoteDeviceLifeCycle* life_cycle_;

  // Tracks whether the remote device is currently in close enough proximity to
  // the local device to allow unlocking.
  std::unique_ptr<ProximityMonitor> proximity_monitor_;

  // Used to call into the embedder. Expected to outlive |this| instance.
  ProximityAuthClient* proximity_auth_client_;

  // Whether the screen is currently locked.
  bool is_locked_;

  // True if the manager is currently processing a user-initiated authentication
  // attempt, which is initiated when the user pod is clicked.
  bool is_attempting_auth_;

  // Whether the system is waking up from sleep.
  bool is_waking_up_;

  // The Bluetooth adapter. Null if there is no adapter present on the local
  // device.
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  // The sign-in secret received from the remote device by decrypting the
  // sign-in challenge.
  std::unique_ptr<std::string> sign_in_secret_;

  // The state of the current screen lock UI.
  ScreenlockState screenlock_state_;

  // Used to clear the waking up state after a timeout.
  base::WeakPtrFactory<UnlockManager> clear_waking_up_state_weak_ptr_factory_;

  // Used to reject auth attempts after a timeout. An in-progress auth attempt
  // blocks the sign-in screen UI, so it's important to prevent the auth attempt
  // from blocking the UI in case a step in the code path hangs.
  base::WeakPtrFactory<UnlockManager> reject_auth_attempt_weak_ptr_factory_;

  // Used to vend all other weak pointers.
  base::WeakPtrFactory<UnlockManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UnlockManager);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_UNLOCK_MANAGER_H
