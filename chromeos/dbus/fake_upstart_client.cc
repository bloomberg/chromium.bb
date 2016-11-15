// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_upstart_client.h"

namespace chromeos {

FakeUpstartClient::FakeUpstartClient() {}

FakeUpstartClient::~FakeUpstartClient() {}

void FakeUpstartClient::Init(dbus::Bus* bus) {}

void FakeUpstartClient::StartAuthPolicyService() {}

}  // namespace chromeos
