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

NfcPropertySet::~NfcPropertySet() {
}

void NfcPropertySet::ConnectSignals() {
  object_proxy()->ConnectToSignal(
      interface(),
      nfc_common::kPropertyChangedSignal,
      base::Bind(&dbus::PropertySet::ChangedReceived, GetWeakPtr()),
      base::Bind(&dbus::PropertySet::ChangedConnected, GetWeakPtr()));
}

void NfcPropertySet::SetAllPropertiesReceivedCallback(
    const base::Closure& callback) {
  on_get_all_callback_ = callback;
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

void NfcPropertySet::OnGetAll(dbus::Response* response) {
  // First invoke the superclass implementation. If the call to GetAll was
  // successful, this will invoke the PropertyChangedCallback passed to the
  // constructor for each individual property received through the call and
  // make sure that the values of the properties have been cached. This way,
  // all received properties will be available when |on_get_all_callback_| is
  // run.
  dbus::PropertySet::OnGetAll(response);
  if (response) {
    VLOG(2) << "NfcPropertySet::GetAll returned successfully.";
    if (!on_get_all_callback_.is_null())
      on_get_all_callback_.Run();
  }
}

void NfcPropertySet::Set(dbus::PropertyBase* property,
                         SetCallback callback) {
  dbus::MethodCall method_call(
      interface(), nfc_common::kSetProperty);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(property->name());
  property->AppendSetValueToWriter(&writer);
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
