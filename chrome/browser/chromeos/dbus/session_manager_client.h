// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_SESSION_MANAGER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_SESSION_MANAGER_CLIENT_H_

#include "base/callback.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/dbus/dbus_client_implementation_type.h"

#include <string>

namespace dbus {
class Bus;
}  // namespace

namespace chromeos {

// SessionManagerClient is used to communicate with the session manager.
class SessionManagerClient {
 public:
  // Interface for observing changes from the session manager.
  class Observer {
   public:
    // Called when the owner key is set.
    virtual void OwnerKeySet(bool success) {}
    // Called when the property change is complete.
    virtual void PropertyChangeComplete(bool success) {}
  };

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

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

  // Used for RetrievePolicy. Takes a serialized protocol buffer as string.
  typedef base::Callback<void(const std::string&)> RetrievePolicyCallback;

  // Fetches the policy blob stored by the session manager.  Upon
  // completion of the retrieve attempt, we will call the provided
  // callback.  Policies are serialized protocol buffers.  Upon success,
  // we will pass a protobuf to the callback.  On failure, we will pass
  // "".
  virtual void RetrievePolicy(RetrievePolicyCallback callback) = 0;

  // Used for StorePolicyCallback. Takes a boolean indicating whether the
  // operation was successful or not.
  typedef base::Callback<void(bool)> StorePolicyCallback;

  // Attempts to store |policy_blob| asynchronously.  Upon completion of
  // the store attempt, we will call callback.
  virtual void StorePolicy(const std::string& policy_bob,
                           StorePolicyCallback callback) = 0;

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

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_SESSION_MANAGER_CLIENT_H_
