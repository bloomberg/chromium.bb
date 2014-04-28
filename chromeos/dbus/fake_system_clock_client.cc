// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_system_clock_client.h"

namespace chromeos {

FakeSystemClockClient::FakeSystemClockClient() {
}

FakeSystemClockClient::~FakeSystemClockClient() {
}

void FakeSystemClockClient::Init(dbus::Bus* bus) {
}

void FakeSystemClockClient::AddObserver(Observer* observer) {
}

void FakeSystemClockClient::RemoveObserver(Observer* observer) {
}

bool FakeSystemClockClient::HasObserver(Observer* observer) {
  return false;
}

void FakeSystemClockClient::SetTime(int64 time_in_seconds) {
}

bool FakeSystemClockClient::CanSetTime() {
  return true;
}

} // namespace chromeos
