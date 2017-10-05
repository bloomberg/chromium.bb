// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "chromeos/dbus/lorgnette_manager_client.h"

namespace chromeos {

// Lorgnette LorgnetteManagerClient implementation used on Linux desktop,
// which does nothing.
class CHROMEOS_EXPORT FakeLorgnetteManagerClient
    : public LorgnetteManagerClient {
 public:
  FakeLorgnetteManagerClient();
  ~FakeLorgnetteManagerClient() override;

  void Init(dbus::Bus* bus) override;

  void ListScanners(const ListScannersCallback& callback) override;
  void ScanImageToString(
      std::string device_name,
      const ScanProperties& properties,
      const ScanImageToStringCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeLorgnetteManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_
