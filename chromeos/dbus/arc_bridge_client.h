// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ARC_BRIDGE_CLIENT_H_
#define CHROMEOS_ARC_BRIDGE_CLIENT_H_

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace dbus {
class ObjectPath;
}

namespace chromeos {

// Client for the Arc Bridge Service
class CHROMEOS_EXPORT ArcBridgeClient : public DBusClient {
 public:
  ~ArcBridgeClient() override;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static ArcBridgeClient* Create();

  // Calls Ping method.  |callback| is called after the method call succeeds.
  virtual void Ping(const VoidDBusMethodCallback& callback) = 0;

 protected:
  // Create() should be used instead.
  ArcBridgeClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcBridgeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_ARC_BRIDGE_CLIENT_H_
