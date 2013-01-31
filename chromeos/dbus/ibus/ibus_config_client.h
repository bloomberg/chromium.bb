// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_CONFIG_CLIENT_H_
#define CHROMEOS_DBUS_IBUS_IBUS_CONFIG_CLIENT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

class IBusComponent;
class IBusInputContextClient;

// A class to make the actual DBus calls for IBusConfig service.
class CHROMEOS_EXPORT IBusConfigClient {
 public:
  typedef base::Callback<void()> ErrorCallback;
  typedef base::Callback<void()> OnIBusConfigReady;
  virtual ~IBusConfigClient();

  // Initializes IBusConfig asynchronously. |on_ready| will be called if it is
  // ready.
  virtual void InitializeAsync(const OnIBusConfigReady& on_ready) = 0;

  // Requests the IBusConfig to set a string value.
  virtual void SetStringValue(const std::string& section,
                              const std::string& key,
                              const std::string& value,
                              const ErrorCallback& error_callback) = 0;

  // Requests the IBusConfig to set a integer value.
  virtual void SetIntValue(const std::string& section,
                           const std::string& key,
                           int value,
                           const ErrorCallback& error_callback) = 0;

  // Requests the IBusConfig to set a boolean value.
  virtual void SetBoolValue(const std::string& section,
                            const std::string& key,
                            bool value,
                            const ErrorCallback& error_callback) = 0;

  // Requests the IBusConfig to set multiple string values.
  virtual void SetStringListValue(const std::string& section,
                                  const std::string& key,
                                  const std::vector<std::string>& value,
                                  const ErrorCallback& error_callback) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static CHROMEOS_EXPORT IBusConfigClient* Create(
      DBusClientImplementationType type,
      dbus::Bus* bus);

 protected:
  // Create() should be used instead.
  IBusConfigClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusConfigClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_CONFIG_CLIENT_H_
