// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill/fake_shill_ipconfig_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

FakeShillIPConfigClient::FakeShillIPConfigClient() {}

FakeShillIPConfigClient::~FakeShillIPConfigClient() = default;

void FakeShillIPConfigClient::AddPropertyChangedObserver(
    const dbus::ObjectPath& ipconfig_path,
    ShillPropertyChangedObserver* observer) {}

void FakeShillIPConfigClient::RemovePropertyChangedObserver(
    const dbus::ObjectPath& ipconfig_path,
    ShillPropertyChangedObserver* observer) {}

void FakeShillIPConfigClient::GetProperties(
    const dbus::ObjectPath& ipconfig_path,
    DictionaryValueCallback callback) {
  const base::Value* dict = ipconfigs_.FindDictKey(ipconfig_path.value());
  if (!dict)
    return;
  std::unique_ptr<base::DictionaryValue> dict_copy =
      base::DictionaryValue::From(dict->CreateDeepCopy());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), DBUS_METHOD_CALL_SUCCESS,
                                std::move(*dict_copy)));
}

void FakeShillIPConfigClient::SetProperty(const dbus::ObjectPath& ipconfig_path,
                                          const std::string& name,
                                          const base::Value& value,
                                          VoidDBusMethodCallback callback) {
  base::Value* dict = ipconfigs_.FindKeyOfType(ipconfig_path.value(),
                                               base::Value::Type::DICTIONARY);
  if (!dict) {
    dict = ipconfigs_.SetKey(ipconfig_path.value(),
                             base::Value(base::Value::Type::DICTIONARY));
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

}  // namespace chromeos
