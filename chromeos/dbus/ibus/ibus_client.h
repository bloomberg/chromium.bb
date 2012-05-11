// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_CLIENT_H_
#define CHROMEOS_DBUS_IBUS_IBUS_CLIENT_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

class IBusInputContextClient;

// A class to make the actual DBus calls for IBusBus service.
// This class only makes calls, result/error handling should be done by
// callbacks.
class CHROMEOS_EXPORT IBusClient {
 public:
  typedef base::Callback<void(const dbus::ObjectPath&)>
      CreateInputContextCallback;

  virtual ~IBusClient();

  // Requests the ibus-daemon to create new input context. If succeeded,
  // |callback| will be called with an ObjectPath which is used in input context
  // handling.
  virtual void CreateInputContext(
      const std::string& client_name,
      const CreateInputContextCallback& callback) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static CHROMEOS_EXPORT IBusClient* Create(DBusClientImplementationType type,
                                            dbus::Bus* bus);

 protected:
  // Create() should be used instead.
  IBusClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_CLIENT_H_
