// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_upstart_client.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_auth_policy_client.h"

namespace chromeos {

FakeUpstartClient::FakeUpstartClient() {}

FakeUpstartClient::~FakeUpstartClient() {}

void FakeUpstartClient::Init(dbus::Bus* bus) {}

void FakeUpstartClient::StartAuthPolicyService() {
  static_cast<FakeAuthPolicyClient*>(
      DBusThreadManager::Get()->GetAuthPolicyClient())
      ->set_started(true);
}

}  // namespace chromeos
