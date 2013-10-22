// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/nfc_property_set.h"

#include "base/bind.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

NfcPropertySet::NfcPropertySet(dbus::ObjectProxy* object_proxy,
                               const std::string& interface,
                               const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface, callback) {
}

void NfcPropertySet::ConnectSignals() {
  object_proxy()->ConnectToSignal(
      interface(),
      nfc_common::kPropertyChangedSignal,
      base::Bind(&dbus::PropertySet::ChangedReceived, GetWeakPtr()),
      base::Bind(&dbus::PropertySet::ChangedConnected, GetWeakPtr()));
}

void NfcPropertySet::Get(dbus::PropertyBase* property,
                         GetCallback callback) {
  NOTREACHED() << "neard does not implement Get for properties.";
}

void NfcPropertySet::GetAll() {
  dbus::MethodCall method_call(
      interface(), nfc_common::kGetProperties);
  object_proxy()->CallMethod(&method_call,
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                           base::Bind(&dbus::PropertySet::OnGetAll,
                                      GetWeakPtr()));
}

void NfcPropertySet::Set(dbus::PropertyBase* property,
                         SetCallback callback) {
  dbus::MethodCall method_call(
      interface(), nfc_common::kSetProperty);
  object_proxy()->CallMethod(&method_call,
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                           base::Bind(&dbus::PropertySet::OnSet,
                                      GetWeakPtr(),
                                      property,
                                      callback));
}

void NfcPropertySet::ChangedReceived(dbus::Signal* signal) {
  DCHECK(signal);
  dbus::MessageReader reader(signal);
  UpdatePropertyFromReader(&reader);
}

}  // namespace chromeos
