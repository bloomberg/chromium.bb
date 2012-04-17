// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FLIMFLAM_PROFILE_CLIENT_H_
#define CHROMEOS_DBUS_FLIMFLAM_PROFILE_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/flimflam_client_helper.h"

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
class CHROMEOS_EXPORT FlimflamProfileClient {
 public:
  typedef FlimflamClientHelper::PropertyChangedHandler PropertyChangedHandler;
  typedef FlimflamClientHelper::VoidCallback VoidCallback;
  typedef FlimflamClientHelper::DictionaryValueCallback DictionaryValueCallback;

  virtual ~FlimflamProfileClient();

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static FlimflamProfileClient* Create(DBusClientImplementationType type,
                                       dbus::Bus* bus);

  // Sets PropertyChanged signal handler.
  virtual void SetPropertyChangedHandler(
      const dbus::ObjectPath& profile_path,
      const PropertyChangedHandler& handler) = 0;

  // Resets PropertyChanged signal handler.
  virtual void ResetPropertyChangedHandler(
      const dbus::ObjectPath& profile_path) = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call succeeds.
  virtual void GetProperties(const dbus::ObjectPath& profile_path,
                             const DictionaryValueCallback& callback) = 0;

  // Calls GetEntry method.
  // |callback| is called after the method call succeeds.
  virtual void GetEntry(const dbus::ObjectPath& profile_path,
                        const std::string& entry_path,
                        const DictionaryValueCallback& callback) = 0;

  // Calls DeleteEntry method.
  // |callback| is called after the method call succeeds.
  virtual void DeleteEntry(const dbus::ObjectPath& profile_path,
                           const std::string& entry_path,
                           const VoidCallback& callback) = 0;

 protected:
  // Create() should be used instead.
  FlimflamProfileClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(FlimflamProfileClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FLIMFLAM_PROFILE_CLIENT_H_
