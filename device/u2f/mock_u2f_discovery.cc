// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/mock_u2f_discovery.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"

namespace device {

MockU2fDiscovery::MockDelegate::MockDelegate() : weak_factory_(this) {}

MockU2fDiscovery::MockDelegate::~MockDelegate() = default;

void MockU2fDiscovery::MockDelegate::OnDeviceAdded(
    std::unique_ptr<U2fDevice> device) {
  OnDeviceAddedStr(device->GetId());
}

void MockU2fDiscovery::MockDelegate::OnDeviceRemoved(
    base::StringPiece device_id) {
  OnDeviceRemovedStr(std::string(device_id));
}

base::WeakPtr<MockU2fDiscovery::MockDelegate>
MockU2fDiscovery::MockDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

MockU2fDiscovery::MockU2fDiscovery() = default;

MockU2fDiscovery::~MockU2fDiscovery() = default;

void MockU2fDiscovery::AddDevice(std::unique_ptr<U2fDevice> device) {
  if (delegate_)
    delegate_->OnDeviceAdded(std::move(device));
}

void MockU2fDiscovery::RemoveDevice(base::StringPiece device_id) {
  if (delegate_)
    delegate_->OnDeviceRemoved(device_id);
}

void MockU2fDiscovery::StartSuccess() {
  if (delegate_)
    delegate_->OnStarted(true);
}

void MockU2fDiscovery::StartFailure() {
  if (delegate_)
    delegate_->OnStarted(false);
}

// static
void MockU2fDiscovery::StartSuccessAsync() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockU2fDiscovery::StartSuccess, base::Unretained(this)));
}

// static
void MockU2fDiscovery::StartFailureAsync() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockU2fDiscovery::StartFailure, base::Unretained(this)));
}

}  // namespace device
