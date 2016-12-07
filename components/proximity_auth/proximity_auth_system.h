// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_SYSTEM_H
#define COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_SYSTEM_H

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/cryptauth/remote_device.h"
#include "components/proximity_auth/remote_device_life_cycle.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/signin/core/account_id/account_id.h"

namespace proximity_auth {

class ProximityAuthClient;
class RemoteDeviceLifeCycle;
class UnlockManager;

// This is the main entry point to start Proximity Auth, the underlying system
// for the Smart Lock feature. Given a list of remote devices (i.e. a
// phone) for each registered user, the system will handle the connection,
// authentication, and messenging protocol when the screen is locked and the
// registered user is focused.
class ProximityAuthSystem : public RemoteDeviceLifeCycle::Observer,
                            public ScreenlockBridge::Observer {
 public:
  enum ScreenlockType { SESSION_LOCK, SIGN_IN };

  ProximityAuthSystem(ScreenlockType screenlock_type,
                      ProximityAuthClient* proximity_auth_client);
  ~ProximityAuthSystem() override;

  // Starts the system to connect and authenticate when a registered user is
  // focused on the lock/sign-in screen.
  void Start();

  // Stops the system.
  void Stop();

  // Registers a list of |remote_devices| for |account_id| that can be used for
  // sign-in/unlock. If devices were previously registered for the user, then
  // they will be replaced.
  void SetRemoteDevicesForUser(
      const AccountId& account_id,
      const cryptauth::RemoteDeviceList& remote_devices);

  // Returns the RemoteDevices registered for |account_id|. Returns an empty
  // list
  // if no devices are registered for |account_id|.
  cryptauth::RemoteDeviceList GetRemoteDevicesForUser(
      const AccountId& account_id) const;

  // Called when the user clicks the user pod and attempts to unlock/sign-in.
  void OnAuthAttempted(const AccountId& account_id);

  // Called when the system suspends.
  void OnSuspend();

  // Called when the system wakes up from a suspended state.
  void OnSuspendDone();

 private:
  // RemoteDeviceLifeCycle::Observer:
  void OnLifeCycleStateChanged(RemoteDeviceLifeCycle::State old_state,
                               RemoteDeviceLifeCycle::State new_state) override;

  // ScreenlockBridge::Observer:
  void OnScreenDidLock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnFocusedUserChanged(const AccountId& account_id) override;

  // Resumes |remote_device_life_cycle_| after device wakes up and waits a
  // timeout.
  void ResumeAfterWakeUpTimeout();

  // Lists of remote devices, keyed by user account id.
  std::map<AccountId, cryptauth::RemoteDeviceList> remote_devices_map_;

  // Delegate for Chrome dependent functionality.
  ProximityAuthClient* proximity_auth_client_;

  // Responsible for the life cycle of connecting and authenticating to
  // the RemoteDevice of the currently focused user.
  std::unique_ptr<RemoteDeviceLifeCycle> remote_device_life_cycle_;

  // Handles the interaction with the lock screen UI.
  std::unique_ptr<UnlockManager> unlock_manager_;

  // True if the system is suspended.
  bool suspended_;

  // True if the system is started_.
  bool started_;

  base::WeakPtrFactory<ProximityAuthSystem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthSystem);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_SYSTEM_H
