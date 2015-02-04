// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_privet_daemon_client.h"

#include <string>

#include "base/callback.h"

namespace chromeos {

FakePrivetDaemonClient::FakePrivetDaemonClient() {
}

FakePrivetDaemonClient::~FakePrivetDaemonClient() {
}

void FakePrivetDaemonClient::Init(dbus::Bus* bus) {
}

void FakePrivetDaemonClient::AddObserver(Observer* observer) {
}

void FakePrivetDaemonClient::RemoveObserver(Observer* observer) {
}

std::string FakePrivetDaemonClient::GetWifiBootstrapState() {
  // Simulate Wi-Fi being configured already.
  return privetd::kWiFiBootstrapStateMonitoring;
}

void FakePrivetDaemonClient::Ping(const PingCallback& callback) {
  callback.Run(true /* success */);
}

}  // namespace chromeos
