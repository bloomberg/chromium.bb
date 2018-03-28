// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_concierge_client.h"

#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeConciergeClient::FakeConciergeClient() {}
FakeConciergeClient::~FakeConciergeClient() = default;

void FakeConciergeClient::CreateDiskImage(
    const vm_tools::concierge::CreateDiskImageRequest& request,
    DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse> callback) {
  create_disk_image_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
}

void FakeConciergeClient::StartTerminaVm(
    const vm_tools::concierge::StartVmRequest& request,
    DBusMethodCallback<vm_tools::concierge::StartVmResponse> callback) {
  start_termina_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
}

void FakeConciergeClient::StopVm(
    const vm_tools::concierge::StopVmRequest& request,
    DBusMethodCallback<vm_tools::concierge::StopVmResponse> callback) {
  stop_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
}

void FakeConciergeClient::StartContainer(
    const vm_tools::concierge::StartContainerRequest& request,
    DBusMethodCallback<vm_tools::concierge::StartContainerResponse> callback) {
  std::move(callback).Run(base::nullopt);
}

void FakeConciergeClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace chromeos
