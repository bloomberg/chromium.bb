// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_PROPERTY_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_PROPERTY_H_
#pragma once

#include "base/compiler_specific.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// BlueZ predates the common D-Bus Properties API (though it inspired it),
// override dbus::PropertySet to generate the correct method call to get
// all properties, conenect to the correct signal and parse it correctly.
//
// BluetoothPropertySet should be used with BluetoothProperty<>.
class BluetoothPropertySet : public dbus::PropertySet {
 public:
  BluetoothPropertySet(dbus::ObjectProxy* object_proxy,
                       const std::string& interface,
                       PropertyChangedCallback callback)
      : dbus::PropertySet(object_proxy, interface, callback) {}

  // Call after construction to connect property change notification
  // signals. Sub-classes may override to use different D-Bus signals.
  void ConnectSignals();

  // Queries the remote object for values of all properties and updates
  // initial values.
  void GetAll();

  // Method connected by ConnectSignals() and called by dbus:: when
  // a property is changed.
  virtual void ChangedReceived(dbus::Signal* signal) OVERRIDE;
};

// BlueZ predates the common D-Bus Properties API (though it inspired it),
// override dbus::Property<> to generate the correct method call to set a
// new property value.
template <class T>
class BluetoothProperty : public dbus::Property<T> {
 public:
  // Import the callbacks into our namespace (this is a template derived from
  // a template, the C++ standard gets a bit wibbly and doesn't do it for us).
  //
  // |success| indicates whether or not the value could be retrived, or new
  // value set. For Get, if true the new value can be obtained by calling
  // value() on the property; for Set() a Get() call may be necessary.
  typedef typename dbus::Property<T>::GetCallback GetCallback;
  typedef typename dbus::Property<T>::SetCallback SetCallback;

  // Requests an updated value from the remote object incurring a
  // round-trip. |callback| will be called when the new value is available.
  //
  // dbus::Property<> override.
  void Get(GetCallback callback) {
    NOTREACHED() << "BlueZ does not implement Get for properties";
  }

  // Requests that the remote object change the property value to |value|,
  // |callback| will be called to indicate the success or failure of the
  // request, however the new value may not be available depending on the
  // remote object.
  //
  // dbus::Property<> override.
  void Set(const T& value, SetCallback callback) {
    dbus::MethodCall method_call(this->property_set()->interface(),
                                 bluetooth_common::kSetProperty);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(this->name());
    this->AppendToWriter(&writer, value);

    dbus::ObjectProxy *object_proxy = this->property_set()->object_proxy();
    DCHECK(object_proxy);
    object_proxy->CallMethod(&method_call,
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                             base::Bind(&dbus::Property<T>::OnSet,
                                        this->GetWeakPtr(),
                                        callback));
  }
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_PROPERTY_H_
