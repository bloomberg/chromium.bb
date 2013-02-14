// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_ipconfig_client_stub.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

ShillIPConfigClientStub::ShillIPConfigClientStub() : weak_ptr_factory_(this) {
}

ShillIPConfigClientStub::~ShillIPConfigClientStub() {
}

void ShillIPConfigClientStub::AddPropertyChangedObserver(
    const dbus::ObjectPath& ipconfig_path,
    ShillPropertyChangedObserver* observer) {
}

void ShillIPConfigClientStub::RemovePropertyChangedObserver(
    const dbus::ObjectPath& ipconfig_path,
    ShillPropertyChangedObserver* observer) {
}

void ShillIPConfigClientStub::Refresh(const dbus::ObjectPath& ipconfig_path,
                                      const VoidDBusMethodCallback& callback) {
}

void ShillIPConfigClientStub::GetProperties(
    const dbus::ObjectPath& ipconfig_path,
    const DictionaryValueCallback& callback) {
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ShillIPConfigClientStub::PassProperties,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback));
}

base::DictionaryValue* ShillIPConfigClientStub::CallGetPropertiesAndBlock(
    const dbus::ObjectPath& ipconfig_path) {
  return new base::DictionaryValue;
}

void ShillIPConfigClientStub::SetProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    const base::Value& value,
    const VoidDBusMethodCallback& callback) {
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
}

void ShillIPConfigClientStub::ClearProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    const VoidDBusMethodCallback& callback) {
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
}

void ShillIPConfigClientStub::Remove(const dbus::ObjectPath& ipconfig_path,
                                     const VoidDBusMethodCallback& callback) {
  if (callback.is_null())
    return;
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
}

void ShillIPConfigClientStub::PassProperties(
    const DictionaryValueCallback& callback) const {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, properties_);
}

}  // namespace chromeos
