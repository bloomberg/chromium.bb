// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_runtime_probe_client.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

FakeRuntimeProbeClient::FakeRuntimeProbeClient() : weak_ptr_factory_(this) {}

FakeRuntimeProbeClient::~FakeRuntimeProbeClient() = default;

void FakeRuntimeProbeClient::ProbeCategories(
    const runtime_probe::ProbeRequest& request,
    RuntimeProbeCallback callback) {
  runtime_probe::ProbeResult result;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

}  // namespace chromeos
