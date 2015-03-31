// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_amplifier_client.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace chromeos {

namespace {

void OnBoolDBusMethod(const BoolDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, true);
}

void OnVoidDBusMethod(const VoidDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS);
}

}  // anonymous namespace

FakeAmplifierClient::FakeAmplifierClient() {
}

FakeAmplifierClient::~FakeAmplifierClient() {
}

void FakeAmplifierClient::Init(dbus::Bus* bus) {
}

void FakeAmplifierClient::AddObserver(Observer* observer) {
}

void FakeAmplifierClient::RemoveObserver(Observer* observer) {
}

void FakeAmplifierClient::Initialize(const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&OnBoolDBusMethod, callback));
}

void FakeAmplifierClient::SetStandbyMode(
    bool standby,
    const VoidDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&OnVoidDBusMethod, callback));
}

void FakeAmplifierClient::SetVolume(double db_spl,
                                    const VoidDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&OnVoidDBusMethod, callback));
}

}  // namespace chromeos
