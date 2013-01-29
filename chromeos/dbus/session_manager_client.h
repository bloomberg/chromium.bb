// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SESSION_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_SESSION_MANAGER_CLIENT_H_

#include "base/callback.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

#include <string>

namespace dbus {
class Bus;
}  // namespace

namespace chromeos {

// SessionManagerClient is used to communicate with the session manager.
class CHROMEOS_EXPORT SessionManagerClient {
 public:
  // Interface for observing changes from the session manager.
  class Observer {
   public:
    // Called when the owner key is set.
    virtual void OwnerKeySet(bool success) {}

    // Called when the property change is complete.
    virtual void PropertyChangeComplete(bool success) {}

    // Called when the session manager requests that the lock screen be
    // displayed.  NotifyLockScreenShown() is called after the lock screen
    // is shown (the canonical "is the screen locked?" state lives in the
    // session manager).
    virtual void LockScreen() {}

    // Called when the session manager requests that the lock screen be
    // dismissed.  NotifyLockScreenDismissed() is called afterward.
    virtual void UnlockScreen() {}

    // Called when the session manager announces that the screen has been locked
    // successfully (i.e. after NotifyLockScreenShown() has been called).
    virtual void ScreenIsLocked() {}

    // Called when the session manager announces that the screen has been
    // unlocked successfully (i.e. after NotifyLockScreenDismissed() has
    // been called).
    virtual void ScreenIsUnlocked() {}
  };

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(Observer* observer) = 0;

  // Kicks off an attempt to emit the "login-prompt-ready" upstart signal.
  virtual void EmitLoginPromptReady() = 0;

  // Kicks off an attempt to emit the "login-prompt-visible" upstart signal.
  virtual void EmitLoginPromptVisible() = 0;

  // Restarts a job referenced by |pid| with the provided command line.
  virtual void RestartJob(int pid, const std::string& command_line) = 0;

  // Restarts entd (the enterprise daemon).
  // DEPRECATED: will be deleted soon.
  virtual void RestartEntd() = 0;

  // Starts the session for the user.
  virtual void StartSession(const std::string& user_email) = 0;

  // Stops the current session.
  virtual void StopSession() = 0;

  // Starts the factory reset.
  virtual void StartDeviceWipe() = 0;

  // Locks the screen.
  virtual void RequestLockScreen() = 0;

  // Notifies that the lock screen is shown.
  virtual void NotifyLockScreenShown() = 0;

  // Unlocks the screen.
  virtual void RequestUnlockScreen() = 0;

  // Notifies that the lock screen is dismissed.
  virtual void NotifyLockScreenDismissed() = 0;

  // Used for RetrieveDevicePolicy, RetrieveUserPolicy and
  // RetrieveDeviceLocalAccountPolicy. Takes a serialized protocol buffer as
  // string.  Upon success, we will pass a protobuf to the callback.  On
  // failure, we will pass "".
  typedef base::Callback<void(const std::string&)> RetrievePolicyCallback;

  // Fetches the device policy blob stored by the session manager.  Upon
  // completion of the retrieve attempt, we will call the provided callback.
  virtual void RetrieveDevicePolicy(const RetrievePolicyCallback& callback) = 0;

  // Fetches the user policy blob stored by the session manager for the
  // currently signed-in user.  Upon completion of the retrieve attempt, we will
  // call the provided callback.
  virtual void RetrieveUserPolicy(const RetrievePolicyCallback& callback) = 0;

  // Fetches the policy blob associated with the specified device-local account
  // from session manager.  |callback| is invoked up on completion.
  virtual void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_id,
      const RetrievePolicyCallback& callback) = 0;

  // Used for StoreDevicePolicy, StoreUserPolicy and
  // StoreDeviceLocalAccountPolicy. Takes a boolean indicating whether the
  // operation was successful or not.
  typedef base::Callback<void(bool)> StorePolicyCallback;

  // Attempts to asynchronously store |policy_blob| as device policy.  Upon
  // completion of the store attempt, we will call callback.
  virtual void StoreDevicePolicy(const std::string& policy_blob,
                                 const StorePolicyCallback& callback) = 0;

  // Attempts to asynchronously store |policy_blob| as user policy for the
  // currently signed-in user.  Upon completion of the store attempt, we will
  // call callback.
  virtual void StoreUserPolicy(const std::string& policy_blob,
                               const StorePolicyCallback& callback) = 0;

  // Sends a request to store a policy blob for the specified device-local
  // account. The result of the operation is reported through |callback|.
  virtual void StoreDeviceLocalAccountPolicy(
      const std::string& account_id,
      const std::string& policy_blob,
      const StorePolicyCallback& callback) = 0;

  // Creates the instance.
  static SessionManagerClient* Create(DBusClientImplementationType type,
                                      dbus::Bus* bus);

  virtual ~SessionManagerClient();

 protected:
  // Create() should be used instead.
  SessionManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SESSION_MANAGER_CLIENT_H_
