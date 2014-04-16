// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_lorgnette_manager_client.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"

namespace chromeos {

FakeLorgnetteManagerClient::FakeLorgnetteManagerClient() {}

FakeLorgnetteManagerClient::~FakeLorgnetteManagerClient() {}

void FakeLorgnetteManagerClient::Init(dbus::Bus* bus) {}

void FakeLorgnetteManagerClient::ListScanners(
    const ListScannersCallback& callback) {
  std::map<std::string, ScannerTableEntry> scanners;
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, false, scanners));
}

void FakeLorgnetteManagerClient::ScanImage(
    std::string device_name,
    base::PlatformFile file,
    const ScanProperties& properties,
    const ScanImageCallback& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, false));
}

}  // namespace chromeos
