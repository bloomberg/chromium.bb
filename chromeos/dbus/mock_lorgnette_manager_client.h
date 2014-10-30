// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_LORGNETTE_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_LORGNETTE_MANAGER_CLIENT_H_

#include "lorgnette_manager_client.h"

#include <gmock/gmock.h>

namespace chromeos {

class MockLorgnetteManagerClient : public LorgnetteManagerClient {
 public:
  MockLorgnetteManagerClient();
  virtual ~MockLorgnetteManagerClient();

  MOCK_METHOD1(ListScanners, void(const ListScannersCallback& callback));
  MOCK_METHOD4(ScanImage, void(std::string device_name,
                               base::PlatformFile file,
                               const ScanProperties& properties,
                               const ScanImageCallback& callback));
  MOCK_METHOD1(Init, void(dbus::Bus* bus));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_LORGNETTE_MANAGER_CLIENT_H_
