// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SESSION_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_SESSION_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

namespace chromeos {

// SessionManagerClient is used to communicate with the session manager.
class CHROMEOS_EXPORT SessionManagerClient : public DBusClient {
 public:
  // Interface for observing changes from the session manager.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the owner key is set.
    virtual void OwnerKeySet(bool success) {}

    // Called when the property change is complete.
    virtual void PropertyChangeComplete(bool success) {}

    // Called when the session manager announces that the screen has been locked
    // successfully (i.e. after NotifyLockScreenShown() has been called).
    virtual void ScreenIsLocked() {}

    // Called when the session manager announces that the screen has been
    // unlocked successfully (i.e. after NotifyLockScreenDismissed() has
    // been called).
    virtual void ScreenIsUnlocked() {}

    // Called after EmitLoginPromptVisible is called.
    virtual void EmitLoginPromptVisibleCalled() {}
  };

  // Interface for performing actions on behalf of the stub implementation.
  class StubDelegate {
   public:
    virtual ~StubDelegate() {}

    // Locks the screen. Invoked by the stub when RequestLockScreen() is called.
    // In the real implementation of SessionManagerClient::RequestLockScreen(),
    // a lock request is forwarded to the session manager; in the stub, this is
    // short-circuited and the screen is locked immediately.
    virtual void LockScreenForStub() = 0;
  };

  // Sets the delegate used by the stub implementation. Ownership of |delegate|
  // remains with the caller.
  virtual void SetStubDelegate(StubDelegate* delegate) = 0;

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(Observer* observer) = 0;

  // Kicks off an attempt to emit the "login-prompt-visible" upstart signal.
  virtual void EmitLoginPromptVisible() = 0;

  // Restarts a job referenced by |pid| with the provided command line.
  virtual void RestartJob(int pid, const std::string& command_line) = 0;

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

  // Notifies that the lock screen is dismissed.
  virtual void NotifyLockScreenDismissed() = 0;

  // Map that is used to describe the set of active user sessions where |key|
  // is user_id and |value| is user_id_hash.
  typedef std::map<std::string, std::string> ActiveSessionsMap;

  // The ActiveSessionsCallback is used for the RetrieveActiveSessions()
  // method. It receives |sessions| argument where the keys are user_ids for
  // all users that are currently active and |success| argument which indicates
  // whether or not the request succeded.
  typedef base::Callback<void(const ActiveSessionsMap& sessions,
                              bool success)> ActiveSessionsCallback;

  // Enumerates active user sessions. Usually Chrome naturally keeps track of
  // active users when they are added into current session. When Chrome is
  // restarted after crash by session_manager it only receives user_id and
  // user_id_hash for one user. This method is used to retrieve list of all
  // active users.
  virtual void RetrieveActiveSessions(
      const ActiveSessionsCallback& callback) = 0;

  // Used for RetrieveDevicePolicy, RetrievePolicyForUser and
  // RetrieveDeviceLocalAccountPolicy. Takes a serialized protocol buffer as
  // string.  Upon success, we will pass a protobuf to the callback.  On
  // failure, we will pass "".
  typedef base::Callback<void(const std::string&)> RetrievePolicyCallback;

  // Fetches the device policy blob stored by the session manager.  Upon
  // completion of the retrieve attempt, we will call the provided callback.
  virtual void RetrieveDevicePolicy(const RetrievePolicyCallback& callback) = 0;

  // Fetches the user policy blob stored by the session manager for the given
  // |username|. Upon completion of the retrieve attempt, we will call the
  // provided callback.
  virtual void RetrievePolicyForUser(
      const std::string& username,
      const RetrievePolicyCallback& callback) = 0;

  // Same as RetrievePolicyForUser() but blocks until a reply is received, and
  // returns the policy synchronously. Returns an empty string if the method
  // call fails.
  // This may only be called in situations where blocking the UI thread is
  // considered acceptable (e.g. restarting the browser after a crash or after
  // a flag change).
  virtual std::string BlockingRetrievePolicyForUser(
      const std::string& username) = 0;

  // Fetches the policy blob associated with the specified device-local account
  // from session manager.  |callback| is invoked up on completion.
  virtual void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_id,
      const RetrievePolicyCallback& callback) = 0;

  // Used for StoreDevicePolicy, StorePolicyForUser and
  // StoreDeviceLocalAccountPolicy. Takes a boolean indicating whether the
  // operation was successful or not.
  typedef base::Callback<void(bool)> StorePolicyCallback;

  // Attempts to asynchronously store |policy_blob| as device policy.  Upon
  // completion of the store attempt, we will call callback.
  virtual void StoreDevicePolicy(const std::string& policy_blob,
                                 const StorePolicyCallback& callback) = 0;

  // Attempts to asynchronously store |policy_blob| as user policy for the given
  // |username|. Upon completion of the store attempt, we will call callback.
  virtual void StorePolicyForUser(const std::string& username,
                                  const std::string& policy_blob,
                                  const StorePolicyCallback& callback) = 0;

  // Sends a request to store a policy blob for the specified device-local
  // account. The result of the operation is reported through |callback|.
  virtual void StoreDeviceLocalAccountPolicy(
      const std::string& account_id,
      const std::string& policy_blob,
      const StorePolicyCallback& callback) = 0;

  // Sets the flags to be applied next time by the session manager when Chrome
  // is restarted inside an already started session for a particular user.
  virtual void SetFlagsForUser(const std::string& username,
                               const std::vector<std::string>& flags) = 0;

  typedef base::Callback<void(const std::vector<std::string>& state_keys)>
      StateKeysCallback;

  // Get the currently valid server-backed state keys for the device.
  // Server-backed state keys are opaque, device-unique, time-dependent,
  // client-determined identifiers that are used for keying state in the cloud
  // for the device to retrieve after a device factory reset.
  //
  // The state keys are returned asynchronously via |callback|. The callback
  // will be invoked with an empty state key vector in case of errors.
  virtual void GetServerBackedStateKeys(const StateKeysCallback& callback) = 0;

  // Creates the instance.
  static SessionManagerClient* Create(DBusClientImplementationType type);

  virtual ~SessionManagerClient();

 protected:
  // Create() should be used instead.
  SessionManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SESSION_MANAGER_CLIENT_H_
