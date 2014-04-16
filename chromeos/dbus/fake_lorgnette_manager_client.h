// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/lorgnette_manager_client.h"

namespace chromeos {

// Lorgnette LorgnetteManagerClient implementation used on Linux desktop,
// which does nothing.
class CHROMEOS_EXPORT FakeLorgnetteManagerClient
    : public LorgnetteManagerClient {
 public:
  FakeLorgnetteManagerClient();
  virtual ~FakeLorgnetteManagerClient();

  virtual void Init(dbus::Bus* bus) OVERRIDE;

  virtual void ListScanners(const ListScannersCallback& callback) OVERRIDE;
  virtual void ScanImage(std::string device_name,
                         base::PlatformFile file,
                         const ScanProperties& properties,
                         const ScanImageCallback& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeLorgnetteManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_
