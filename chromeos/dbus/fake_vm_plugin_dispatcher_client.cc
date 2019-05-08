// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_vm_plugin_dispatcher_client.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

using namespace vm_tools::plugin_dispatcher;

namespace chromeos {

FakeVmPluginDispatcherClient::FakeVmPluginDispatcherClient() = default;

FakeVmPluginDispatcherClient::~FakeVmPluginDispatcherClient() = default;

void FakeVmPluginDispatcherClient::StartVm(
    const StartVmRequest& request,
    DBusMethodCallback<StartVmResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), StartVmResponse()));
}

void FakeVmPluginDispatcherClient::ListVms(
    const ListVmRequest& request,
    DBusMethodCallback<ListVmResponse> callback) {
  ListVmResponse response;
  response.add_vm_info()->set_name("vm");
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakeVmPluginDispatcherClient::StopVm(
    const StopVmRequest& request,
    DBusMethodCallback<StopVmResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), StopVmResponse()));
}

void FakeVmPluginDispatcherClient::SuspendVm(
    const SuspendVmRequest& request,
    DBusMethodCallback<SuspendVmResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), SuspendVmResponse()));
}

void FakeVmPluginDispatcherClient::ShowVm(
    const ShowVmRequest& request,
    DBusMethodCallback<ShowVmResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), ShowVmResponse()));
}

void FakeVmPluginDispatcherClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace chromeos
