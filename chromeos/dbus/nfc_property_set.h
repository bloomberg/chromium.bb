// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_NFC_PROPERTY_SET_H_
#define CHROMEOS_DBUS_NFC_PROPERTY_SET_H_

#include <string>

#include "base/callback.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace chromeos {

// neard doesn't use the standard D-Bus interfaces for property access and
// instead defines property accessor methods in each D-Bus interface. This
// class customizes dbus::PropertySet to generate the correct method call to
// get all properties, connect to the correct signal and parse it correctly.
class NfcPropertySet : public dbus::PropertySet {
 public:
  NfcPropertySet(dbus::ObjectProxy* object_proxy,
                 const std::string& interface,
                 const PropertyChangedCallback& callback);

  // Destructor; we don't hold on to any references or memory that needs
  // explicit clean-up, but clang thinks we might.
  virtual ~NfcPropertySet();

  // Caches |callback| so that it will be invoked after a call to GetAll()
  // has successfully received all existing properties from the remote object.
  void SetAllPropertiesReceivedCallback(const base::Closure& callback);

  // dbus::PropertySet overrides
  virtual void ConnectSignals() OVERRIDE;
  virtual void Get(dbus::PropertyBase* property,
                   GetCallback callback) OVERRIDE;
  virtual void GetAll() OVERRIDE;
  virtual void OnGetAll(dbus::Response* response) OVERRIDE;
  virtual void Set(dbus::PropertyBase* property,
                   SetCallback callback) OVERRIDE;
  virtual void ChangedReceived(dbus::Signal* signal) OVERRIDE;

 protected:
  const base::Closure& on_get_all_callback() { return on_get_all_callback_; }

 private:
  // Optional callback used to notify clients when all properties were received
  // after a call to GetAll.
  base::Closure on_get_all_callback_;

  DISALLOW_COPY_AND_ASSIGN(NfcPropertySet);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_NFC_PROPERTY_SET_H_
