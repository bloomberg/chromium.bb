// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/fake_bus.h"

#include "base/location.h"

namespace dbus {

FakeBus::FakeBus(const Bus::Options& options) : Bus(options) {
}

FakeBus::~FakeBus() {
}

}  // namespace dbus
