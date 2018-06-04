// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cicerone_client.h"

#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeCiceroneClient::FakeCiceroneClient() {}
FakeCiceroneClient::~FakeCiceroneClient() = default;

void FakeCiceroneClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeCiceroneClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool FakeCiceroneClient::IsContainerStartedSignalConnected() {
  return is_container_started_signal_connected_;
}

bool FakeCiceroneClient::IsContainerShutdownSignalConnected() {
  return is_container_shutdown_signal_connected_;
}

void FakeCiceroneClient::LaunchContainerApplication(
    const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
    DBusMethodCallback<vm_tools::cicerone::LaunchContainerApplicationResponse>
        callback) {
  vm_tools::cicerone::LaunchContainerApplicationResponse response;
  response.set_success(true);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeCiceroneClient::GetContainerAppIcons(
    const vm_tools::cicerone::ContainerAppIconRequest& request,
    DBusMethodCallback<vm_tools::cicerone::ContainerAppIconResponse> callback) {
  vm_tools::cicerone::ContainerAppIconResponse response;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeCiceroneClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace chromeos
