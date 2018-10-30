// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_diagnosticsd_client.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeDiagnosticsdClient::FakeDiagnosticsdClient() = default;

FakeDiagnosticsdClient::~FakeDiagnosticsdClient() = default;

void FakeDiagnosticsdClient::Init(dbus::Bus* bus) {}

void FakeDiagnosticsdClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), false /* service_is_available */));
}

void FakeDiagnosticsdClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false /* result */));
}

}  // namespace chromeos
