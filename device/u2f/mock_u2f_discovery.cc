// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/mock_u2f_discovery.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"

namespace device {

MockU2fDiscoveryObserver::MockU2fDiscoveryObserver() = default;

MockU2fDiscoveryObserver::~MockU2fDiscoveryObserver() = default;

MockU2fDiscovery::MockU2fDiscovery() = default;

MockU2fDiscovery::~MockU2fDiscovery() = default;

bool MockU2fDiscovery::AddDevice(std::unique_ptr<U2fDevice> device) {
  return U2fDiscovery::AddDevice(std::move(device));
}

bool MockU2fDiscovery::RemoveDevice(base::StringPiece device_id) {
  return U2fDiscovery::RemoveDevice(device_id);
}

base::ObserverList<U2fDiscovery::Observer>& MockU2fDiscovery::GetObservers() {
  return observers_;
}

void MockU2fDiscovery::StartSuccess() {
  NotifyDiscoveryStarted(true);
}

void MockU2fDiscovery::StartFailure() {
  NotifyDiscoveryStarted(false);
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
