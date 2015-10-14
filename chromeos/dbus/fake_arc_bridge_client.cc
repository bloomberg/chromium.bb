// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_arc_bridge_client.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"

namespace chromeos {

namespace {

void OnVoidDBusMethod(const VoidDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS);
}

}  // anonymous namespace

FakeArcBridgeClient::FakeArcBridgeClient() {
}

FakeArcBridgeClient::~FakeArcBridgeClient() {
}

void FakeArcBridgeClient::Init(dbus::Bus* bus) {
}

void FakeArcBridgeClient::Ping(const VoidDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OnVoidDBusMethod, callback));
}

}  // namespace chromeos
