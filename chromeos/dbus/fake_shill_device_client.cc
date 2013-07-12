// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_shill_device_client.h"

namespace chromeos {

FakeShillDeviceClient::FakeShillDeviceClient() {
}

FakeShillDeviceClient::~FakeShillDeviceClient() {
}

void FakeShillDeviceClient::AddPropertyChangedObserver(
    const dbus::ObjectPath& device_path,
    ShillPropertyChangedObserver* observer) {
}

void FakeShillDeviceClient::RemovePropertyChangedObserver(
    const dbus::ObjectPath& device_path,
    ShillPropertyChangedObserver* observer) {
}

void FakeShillDeviceClient::GetProperties(
    const dbus::ObjectPath& device_path,
    const DictionaryValueCallback& callback) {
}

void FakeShillDeviceClient::ProposeScan(
    const dbus::ObjectPath& device_path,
    const VoidDBusMethodCallback& callback) {
}

void FakeShillDeviceClient::SetProperty(const dbus::ObjectPath& device_path,
                                        const std::string& name,
                                        const base::Value& value,
                                        const base::Closure& callback,
                                        const ErrorCallback& error_callback) {
}

void FakeShillDeviceClient::ClearProperty(
    const dbus::ObjectPath& device_path,
    const std::string& name,
    const VoidDBusMethodCallback& callback) {
}

void FakeShillDeviceClient::AddIPConfig(
    const dbus::ObjectPath& device_path,
    const std::string& method,
    const ObjectPathDBusMethodCallback& callback) {
}

void FakeShillDeviceClient::RequirePin(const dbus::ObjectPath& device_path,
                                       const std::string& pin,
                                       bool require,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback) {
}

void FakeShillDeviceClient::EnterPin(const dbus::ObjectPath& device_path,
                                     const std::string& pin,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
}

void FakeShillDeviceClient::UnblockPin(const dbus::ObjectPath& device_path,
                                       const std::string& puk,
                                       const std::string& pin,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback) {
}

void FakeShillDeviceClient::ChangePin(const dbus::ObjectPath& device_path,
                                      const std::string& old_pin,
                                      const std::string& new_pin,
                                      const base::Closure& callback,
                                      const ErrorCallback& error_callback) {
}

void FakeShillDeviceClient::Register(const dbus::ObjectPath& device_path,
                                     const std::string& network_id,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
}

void FakeShillDeviceClient::SetCarrier(const dbus::ObjectPath& device_path,
                                       const std::string& carrier,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback) {
}

void FakeShillDeviceClient::Reset(const dbus::ObjectPath& device_path,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
}

FakeShillDeviceClient::TestInterface*
FakeShillDeviceClient::GetTestInterface() {
  return NULL;
}

}  // namespace chromeos
