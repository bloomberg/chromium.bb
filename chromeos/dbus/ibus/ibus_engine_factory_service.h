// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_ENGINE_FACTORY_SERVICE_H_
#define CHROMEOS_DBUS_IBUS_IBUS_ENGINE_FACTORY_SERVICE_H_

#include <string>
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

// A class to make the actual DBus method call handling for IBusEngineFactory
// service. The exported method call is used by ibus-daemon to create engine
// service if the extension IME is enabled.
class CHROMEOS_EXPORT IBusEngineFactoryService {
 public:
  typedef base::Callback<void(const dbus::ObjectPath& path)>
      CreateEngineResponseSender;
  typedef base::Callback<void(const CreateEngineResponseSender& sender)>
      CreateEngineHandler;

  virtual ~IBusEngineFactoryService();

  // Sets CreateEngine method call handler for |engine_id|. If ibus-daemon calls
  // CreateEngine message with |engine_id|, the |create_engine_handler| will be
  // called.
  virtual void SetCreateEngineHandler(
      const std::string& engine_id,
      const CreateEngineHandler& create_engine_handler) = 0;

  // Unsets CreateEngine method call handler for |engine_id|.
  virtual void UnsetCreateEngineHandler(const std::string& engine_id) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, accesses the singleton via DBusThreadManager::Get().
  static CHROMEOS_EXPORT IBusEngineFactoryService* Create(
      dbus::Bus* bus,
      DBusClientImplementationType type);

 protected:
  // Create() should be used instead.
  IBusEngineFactoryService();

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusEngineFactoryService);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_ENGINE_FACTORY_SERVICE_H_
