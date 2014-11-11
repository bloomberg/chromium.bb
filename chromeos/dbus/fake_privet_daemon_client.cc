// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_privet_daemon_client.h"

#include "base/callback.h"

namespace chromeos {

FakePrivetDaemonClient::FakePrivetDaemonClient() {
}

FakePrivetDaemonClient::~FakePrivetDaemonClient() {
}

void FakePrivetDaemonClient::Init(dbus::Bus* bus) {
}

void FakePrivetDaemonClient::GetSetupStatus(
    const GetSetupStatusCallback& callback) {
  callback.Run(privetd::kSetupCompleted);
}

}  // namespace chromeos
