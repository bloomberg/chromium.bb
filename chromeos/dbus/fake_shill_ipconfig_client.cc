// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_shill_ipconfig_client.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
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

FakeShillIPConfigClient::FakeShillIPConfigClient() : weak_ptr_factory_(this) {
}

FakeShillIPConfigClient::~FakeShillIPConfigClient() {
}

void FakeShillIPConfigClient::Init(dbus::Bus* bus) {
}

void FakeShillIPConfigClient::AddPropertyChangedObserver(
    const dbus::ObjectPath& ipconfig_path,
    ShillPropertyChangedObserver* observer) {
}

void FakeShillIPConfigClient::RemovePropertyChangedObserver(
    const dbus::ObjectPath& ipconfig_path,
    ShillPropertyChangedObserver* observer) {
}

void FakeShillIPConfigClient::Refresh(const dbus::ObjectPath& ipconfig_path,
                                      const VoidDBusMethodCallback& callback) {
}

void FakeShillIPConfigClient::GetProperties(
    const dbus::ObjectPath& ipconfig_path,
    const DictionaryValueCallback& callback) {
  const base::DictionaryValue* dict = NULL;
  if (!ipconfigs_.GetDictionaryWithoutPathExpansion(ipconfig_path.value(),
                                                    &dict))
    return;
  base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&FakeShillIPConfigClient::PassProperties,
                              weak_ptr_factory_.GetWeakPtr(),
                              dict,
                              callback));
}

void FakeShillIPConfigClient::SetProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    const base::Value& value,
    const VoidDBusMethodCallback& callback) {
  base::DictionaryValue* dict = NULL;
  if (ipconfigs_.GetDictionaryWithoutPathExpansion(ipconfig_path.value(),
                                                   &dict)) {
    // Update existing ip config stub object's properties.
    dict->SetWithoutPathExpansion(name, value.DeepCopy());
  } else {
    // Create a new stub ipconfig object, and update its properties.
    base::DictionaryValue* dvalue = new base::DictionaryValue;
    dvalue->SetWithoutPathExpansion(name, value.DeepCopy());
    ipconfigs_.SetWithoutPathExpansion(ipconfig_path.value(),
                                       dvalue);
  }
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
}

void FakeShillIPConfigClient::ClearProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    const VoidDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
}

void FakeShillIPConfigClient::Remove(const dbus::ObjectPath& ipconfig_path,
                                     const VoidDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
}

ShillIPConfigClient::TestInterface*
FakeShillIPConfigClient::GetTestInterface() {
  return this;
}

// ShillIPConfigClient::TestInterface overrides

void FakeShillIPConfigClient::AddIPConfig(
    const std::string& ip_config_path,
    const base::DictionaryValue& properties) {
  ipconfigs_.SetWithoutPathExpansion(ip_config_path, properties.DeepCopy());
}

// Private methods

void FakeShillIPConfigClient::PassProperties(
    const base::DictionaryValue* values,
    const DictionaryValueCallback& callback) const {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, *values);
}

}  // namespace chromeos
