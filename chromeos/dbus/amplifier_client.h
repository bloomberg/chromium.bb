// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_AMPLIFIER_CLIENT_H_
#define CHROMEOS_DBUS_AMPLIFIER_CLIENT_H_

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

// A DBus client class for the org.chromium.Amplifier service.
class CHROMEOS_EXPORT AmplifierClient : public DBusClient {
 public:
  // Interface for observing amplifier events from an amplifier client.
  class Observer {
   public:
    virtual ~Observer() {}
    // Called when the Error signal is received.
    virtual void OnError(int32 error_code) = 0;
  };

  ~AmplifierClient() override;

  // Adds and removes observers for amplifier events.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Factory function; creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static AmplifierClient* Create();

  // Calls Initialize method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded, and a bool that is the return value from
  // the DBus method.
  virtual void Initialize(const BoolDBusMethodCallback& callback) = 0;

  // Calls SetStandbyMode method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded.
  virtual void SetStandbyMode(bool standby,
                              const VoidDBusMethodCallback& callback) = 0;

  // Calls SetVolume method.
  // |callback| will be called with a DBusMethodCallStatus indicating whether
  // the DBus method call succeeded.
  virtual void SetVolume(double db_spl,
                         const VoidDBusMethodCallback& callback) = 0;

 protected:
  // Create() should be used instead.
  AmplifierClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(AmplifierClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_AMPLIFIER_CLIENT_H_
