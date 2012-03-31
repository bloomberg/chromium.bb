// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_FLIMFLAM_PROFILE_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_FLIMFLAM_PROFILE_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/dbus/dbus_client_implementation_type.h"

namespace base {

class Value;
class DictionaryValue;

}  // namespace base

namespace dbus {

class Bus;
class ObjectPath;

}  // namespace dbus

namespace chromeos {

// FlimflamProfileClient is used to communicate with the Flimflam Profile
// service.  All methods should be called from the origin thread which
// initializes the DBusThreadManager instance.
class FlimflamProfileClient {
 public:
  // An enum to describe whether or not a DBus method call succeeded.
  enum CallStatus {
    FAILURE,
    SUCCESS,
  };

  // A callback to handle PropertyChanged signals.
  typedef base::Callback<void(const std::string& name,
                              const base::Value& value)> PropertyChangedHandler;

  // A callback to handle responses for methods without results.
  typedef base::Callback<void(CallStatus call_status)> VoidCallback;

  // A callback to handle responses for methods with DictionaryValue results.
  typedef base::Callback<void(
      CallStatus call_status,
      const base::DictionaryValue& result)> DictionaryValueCallback;

  virtual ~FlimflamProfileClient();

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static FlimflamProfileClient* Create(DBusClientImplementationType type,
                                       dbus::Bus* bus);

  // Sets PropertyChanged signal handler.
  virtual void SetPropertyChangedHandler(
      const PropertyChangedHandler& handler) = 0;

  // Resets PropertyChanged signal handler.
  virtual void ResetPropertyChangedHandler() = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call succeeds.
  virtual void GetProperties(const DictionaryValueCallback& callback) = 0;

  // Calls GetEntry method.
  // |callback| is called after the method call succeeds.
  virtual void GetEntry(const dbus::ObjectPath& path,
                        const DictionaryValueCallback& callback) = 0;

  // Calls DeleteEntry method.
  // |callback| is called after the method call succeeds.
  virtual void DeleteEntry(const dbus::ObjectPath& path,
                           const VoidCallback& callback) = 0;

 protected:
  // Create() should be used instead.
  FlimflamProfileClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(FlimflamProfileClient);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_FLIMFLAM_PROFILE_CLIENT_H_
