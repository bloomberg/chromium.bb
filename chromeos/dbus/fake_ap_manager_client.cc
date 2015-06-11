// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_ap_manager_client.h"

#include "base/location.h"
#include "base/thread_task_runner_handle.h"

namespace chromeos {

namespace {
void ObjectPathDBBusMethodCallbackThunk(
    const ObjectPathDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, dbus::ObjectPath());
}

void VoidDBBusMethodCallbackThunk(const VoidDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS);
}
}  // namespace

FakeApManagerClient::FakeApManagerClient() {
}

FakeApManagerClient::~FakeApManagerClient() {
}

void FakeApManagerClient::Init(dbus::Bus* bus) {
}

void FakeApManagerClient::AddObserver(Observer* observer) {
}

void FakeApManagerClient::RemoveObserver(Observer* observer) {
}

void FakeApManagerClient::CreateService(
    const ObjectPathDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ObjectPathDBBusMethodCallbackThunk, callback));
}

void FakeApManagerClient::RemoveService(
    const dbus::ObjectPath& object_path,
    const VoidDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&VoidDBBusMethodCallbackThunk, callback));
}

void FakeApManagerClient::StartService(const dbus::ObjectPath& object_path,
                                       const VoidDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&VoidDBBusMethodCallbackThunk, callback));
}

void FakeApManagerClient::StopService(const dbus::ObjectPath& object_path,
                                      const VoidDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&VoidDBBusMethodCallbackThunk, callback));
}

ApManagerClient::ConfigProperties* FakeApManagerClient::GetConfigProperties(
    const dbus::ObjectPath& object_path) {
  return nullptr;
}

const ApManagerClient::DeviceProperties*
FakeApManagerClient::GetDeviceProperties(const dbus::ObjectPath& object_path) {
  return nullptr;
}

const ApManagerClient::ServiceProperties*
FakeApManagerClient::GetServiceProperties(const dbus::ObjectPath& object_path) {
  return nullptr;
}

}  // namespace chromeos
