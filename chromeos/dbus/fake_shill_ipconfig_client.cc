// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_shill_ipconfig_client.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
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
                                      VoidDBusMethodCallback callback) {}

void FakeShillIPConfigClient::GetProperties(
    const dbus::ObjectPath& ipconfig_path,
    const DictionaryValueCallback& callback) {
  const base::DictionaryValue* dict = NULL;
  if (!ipconfigs_.GetDictionaryWithoutPathExpansion(ipconfig_path.value(),
                                                    &dict))
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FakeShillIPConfigClient::PassProperties,
                            weak_ptr_factory_.GetWeakPtr(), dict, callback));
}

void FakeShillIPConfigClient::SetProperty(const dbus::ObjectPath& ipconfig_path,
                                          const std::string& name,
                                          const base::Value& value,
                                          VoidDBusMethodCallback callback) {
  base::DictionaryValue* dict = NULL;
  if (!ipconfigs_.GetDictionaryWithoutPathExpansion(ipconfig_path.value(),
                                                    &dict)) {
    dict = ipconfigs_.SetDictionaryWithoutPathExpansion(
        ipconfig_path.value(), base::MakeUnique<base::DictionaryValue>());
  }
  // Update existing ip config stub object's properties.
  dict->SetKey(name, value.Clone());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeShillIPConfigClient::ClearProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeShillIPConfigClient::Remove(const dbus::ObjectPath& ipconfig_path,
                                     VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

ShillIPConfigClient::TestInterface*
FakeShillIPConfigClient::GetTestInterface() {
  return this;
}

// ShillIPConfigClient::TestInterface overrides

void FakeShillIPConfigClient::AddIPConfig(
    const std::string& ip_config_path,
    const base::DictionaryValue& properties) {
  ipconfigs_.SetKey(ip_config_path, properties.Clone());
}

// Private methods

void FakeShillIPConfigClient::PassProperties(
    const base::DictionaryValue* values,
    const DictionaryValueCallback& callback) const {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, *values);
}

}  // namespace chromeos
