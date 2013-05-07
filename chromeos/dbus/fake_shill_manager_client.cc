// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_shill_manager_client.h"

namespace chromeos {

FakeShillManagerClient::FakeShillManagerClient() {
}

FakeShillManagerClient::~FakeShillManagerClient() {
}

void FakeShillManagerClient::RequestScan(const std::string& type,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
}

ShillManagerClient::TestInterface* FakeShillManagerClient::GetTestInterface() {
  return NULL;
}

void FakeShillManagerClient::GetNetworksForGeolocation(
    const DictionaryValueCallback& callback) {
}

void FakeShillManagerClient::ConnectToBestServices(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
}

void FakeShillManagerClient::RemovePropertyChangedObserver(
    ShillPropertyChangedObserver* observer) {
}

void FakeShillManagerClient::VerifyAndEncryptData(
    const VerificationProperties& properties,
    const std::string& data,
    const StringCallback& callback,
    const ErrorCallback& error_callback) {
}

void FakeShillManagerClient::GetService(const base::DictionaryValue& properties,
                                        const ObjectPathCallback& callback,
                                        const ErrorCallback& error_callback) {
}

base::DictionaryValue* FakeShillManagerClient::CallGetPropertiesAndBlock() {
  return NULL;
}

void FakeShillManagerClient::AddPropertyChangedObserver(
    ShillPropertyChangedObserver* observer) {
}

void FakeShillManagerClient::DisableTechnology(
    const std::string& type,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
}

void FakeShillManagerClient::SetProperty(const std::string& name,
                                         const base::Value& value,
                                         const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
}

void FakeShillManagerClient::GetProperties(
    const DictionaryValueCallback& callback) {
}

void FakeShillManagerClient::VerifyAndEncryptCredentials(
    const VerificationProperties& properties,
    const std::string& service_path,
    const StringCallback& callback,
    const ErrorCallback& error_callback) {
}

void FakeShillManagerClient::EnableTechnology(
    const std::string& type,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
}

void FakeShillManagerClient::ConfigureService(
    const base::DictionaryValue& properties,
    const ObjectPathCallback& callback,
    const ErrorCallback& error_callback) {
}

void FakeShillManagerClient::VerifyDestination(
    const VerificationProperties& properties,
    const BooleanCallback& callback,
    const ErrorCallback& error_callback) {
}

void FakeShillManagerClient::ConfigureServiceForProfile(
    const dbus::ObjectPath& profile_path,
    const base::DictionaryValue& properties,
    const ObjectPathCallback& callback,
    const ErrorCallback& error_callback) {
}

} // namespace chromeos
