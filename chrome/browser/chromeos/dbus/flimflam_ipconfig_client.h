// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_FLIMFLAM_IPCONFIG_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_FLIMFLAM_IPCONFIG_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/dbus/dbus_client_implementation_type.h"
#include "chrome/browser/chromeos/dbus/flimflam_client_helper.h"

namespace base {

class Value;
class DictionaryValue;

}  // namespace base

namespace dbus {

class Bus;
class ObjectPath;

}  // namespace dbus

namespace chromeos {

// FlimflamIPConfigClient is used to communicate with the Flimflam IPConfig
// service.  All methods should be called from the origin thread which
// initializes the DBusThreadManager instance.
class FlimflamIPConfigClient {
 public:
  typedef FlimflamClientHelper::PropertyChangedHandler PropertyChangedHandler;
  typedef FlimflamClientHelper::VoidCallback VoidCallback;
  typedef FlimflamClientHelper::DictionaryValueCallback DictionaryValueCallback;
  virtual ~FlimflamIPConfigClient();

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static FlimflamIPConfigClient* Create(DBusClientImplementationType type,
                                        dbus::Bus* bus);

  // Sets PropertyChanged signal handler.
  virtual void SetPropertyChangedHandler(
      const PropertyChangedHandler& handler) = 0;

  // Resets PropertyChanged signal handler.
  virtual void ResetPropertyChangedHandler() = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call succeeds.
  virtual void GetProperties(const DictionaryValueCallback& callback) = 0;

  // Calls SetProperty method.
  // |callback| is called after the method call succeeds.
  virtual void SetProperty(const std::string& name,
                           const base::Value& value,
                           const VoidCallback& callback) = 0;

  // Calls ClearProperty method.
  // |callback| is called after the method call succeeds.
  virtual void ClearProperty(const std::string& name,
                             const VoidCallback& callback) = 0;

  // Calls Remove method.
  // |callback| is called after the method call succeeds.
  virtual void Remove(const VoidCallback& callback) = 0;

 protected:
  // Create() should be used instead.
  FlimflamIPConfigClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(FlimflamIPConfigClient);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_FLIMFLAM_IPCONFIG_CLIENT_H_
