// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_SYSTEM_H
#define COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_SYSTEM_H

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/remote_device_life_cycle.h"
#include "components/proximity_auth/screenlock_bridge.h"

namespace proximity_auth {

class ProximityAuthClient;
class RemoteDeviceLifeCycle;
class UnlockManager;

// This is the main entry point to start Proximity Auth, the underlying system
// for the Smart Lock feature. Given a registered remote device (i.e. a phone),
// this object will handle the connection, authentication, and protocol for the
// device.
class ProximityAuthSystem : public RemoteDeviceLifeCycle::Observer,
                            public ScreenlockBridge::Observer {
 public:
  enum ScreenlockType { SESSION_LOCK, SIGN_IN };

  ProximityAuthSystem(ScreenlockType screenlock_type,
                      RemoteDevice remote_device,
                      ProximityAuthClient* proximity_auth_client);
  ~ProximityAuthSystem() override;

  // Starts the system to begin connecting and authenticating the remote device.
  void Start();

  // Called when the user clicks the user pod and attempts to unlock/sign-in.
  void OnAuthAttempted(const std::string& user_id);

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
  void OnFocusedUserChanged(const std::string& user_id) override;

  // Resumes |remote_device_life_cycle_| after device wakes up and waits a
  // timeout.
  void ResumeAfterWakeUpTimeout();

  // The remote device to connect to.
  RemoteDevice remote_device_;

  // Delegate for Chrome dependent functionality.
  ProximityAuthClient* proximity_auth_client_;

  // Responsible for the life cycle of connecting and authenticating to
  // |remote_device_|.
  scoped_ptr<RemoteDeviceLifeCycle> remote_device_life_cycle_;

  // Handles the interaction with the lock screen UI.
  scoped_ptr<UnlockManager> unlock_manager_;

  // True if the system is suspended.
  bool suspended_;

  base::WeakPtrFactory<ProximityAuthSystem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthSystem);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_SYSTEM_H
