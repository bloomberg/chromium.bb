// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_property.h"

#include "base/bind.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

void BluetoothPropertySet::ConnectSignals() {
  dbus::ObjectProxy* object_proxy = this->object_proxy();
  DCHECK(object_proxy);
  object_proxy->ConnectToSignal(
      interface(),
      bluetooth_common::kPropertyChangedSignal,
      base::Bind(&dbus::PropertySet::ChangedReceived, GetWeakPtr()),
      base::Bind(&dbus::PropertySet::ChangedConnected, GetWeakPtr()));
}

void BluetoothPropertySet::ChangedReceived(dbus::Signal* signal) {
  DCHECK(signal);

  dbus::MessageReader reader(signal);
  UpdatePropertyFromReader(&reader);
}

void BluetoothPropertySet::Get(dbus::PropertyBase* property,
                               GetCallback callback) {
  NOTREACHED() << "BlueZ does not implement Get for properties";
}

void BluetoothPropertySet::GetAll() {
  dbus::MethodCall method_call(interface(),
                               bluetooth_common::kGetProperties);

  dbus::ObjectProxy* object_proxy = this->object_proxy();
  DCHECK(object_proxy);
  object_proxy->CallMethod(&method_call,
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                           base::Bind(&dbus::PropertySet::OnGetAll,
                                      GetWeakPtr()));
}

void BluetoothPropertySet::Set(dbus::PropertyBase* property,
                               SetCallback callback) {
  dbus::MethodCall method_call(interface(),
                               bluetooth_common::kSetProperty);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(property->name());
  property->AppendSetValueToWriter(&writer);

  dbus::ObjectProxy *object_proxy = this->object_proxy();
  DCHECK(object_proxy);
  object_proxy->CallMethod(&method_call,
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                           base::Bind(&dbus::PropertySet::OnSet,
                                      this->GetWeakPtr(),
                                      property,
                                      callback));
}

}  // namespace chromeos
