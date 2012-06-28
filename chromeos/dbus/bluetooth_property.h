// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_PROPERTY_H_
#define CHROMEOS_DBUS_BLUETOOTH_PROPERTY_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// BlueZ predates the common D-Bus Properties API (though it inspired it),
// override dbus::PropertySet to generate the correct method call to get
// all properties, conenect to the correct signal and parse it correctly.
class BluetoothPropertySet : public dbus::PropertySet {
 public:
  BluetoothPropertySet(dbus::ObjectProxy* object_proxy,
                       const std::string& interface,
                       const PropertyChangedCallback& callback)
      : dbus::PropertySet(object_proxy, interface, callback) {}

  // dbus::PropertySet override.
  //
  // Call after construction to connect property change notification
  // signals. Sub-classes may override to use different D-Bus signals.
  virtual void ConnectSignals() OVERRIDE;

  // dbus::PropertySet override.
  //
  // Requests an updated value from the remote object incurring a round-trip.
  virtual void Get(dbus::PropertyBase* property,
                   GetCallback callback) OVERRIDE;

  // dbus::PropertySet override.
  //
  // Queries the remote object for values of all properties and updates
  // initial values.
  virtual void GetAll() OVERRIDE;

  // dbus::PropertySet override.
  //
  // Requests that the remote object change the property to its new value.
  virtual void Set(dbus::PropertyBase* property,
                   SetCallback callback) OVERRIDE;

  // dbus::PropertySet override.
  //
  // Method connected by ConnectSignals() and called by dbus:: when
  // a property is changed.
  virtual void ChangedReceived(dbus::Signal* signal) OVERRIDE;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_PROPERTY_H_
