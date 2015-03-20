// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_privet_daemon_manager_client.h"

#include "base/message_loop/message_loop.h"

namespace chromeos {

FakePrivetDaemonManagerClient::FakePrivetDaemonManagerClient() {
}

FakePrivetDaemonManagerClient::~FakePrivetDaemonManagerClient() {
}

void FakePrivetDaemonManagerClient::Init(dbus::Bus* bus) {
}

void FakePrivetDaemonManagerClient::AddObserver(Observer* observer) {
}

void FakePrivetDaemonManagerClient::RemoveObserver(Observer* observer) {
}

void FakePrivetDaemonManagerClient::SetDescription(
    const std::string& description,
    const VoidDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS));
}

const PrivetDaemonManagerClient::Properties*
FakePrivetDaemonManagerClient::GetProperties() {
  return nullptr;
}

}  // namespace chromeos
