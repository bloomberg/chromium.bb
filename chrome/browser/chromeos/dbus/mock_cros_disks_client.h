// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_MOCK_CROS_DISKS_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_MOCK_CROS_DISKS_CLIENT_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/dbus/cros_disks_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockCrosDisksClient : public CrosDisksClient {
 public:
  MockCrosDisksClient();
  virtual ~MockCrosDisksClient();

  MOCK_METHOD4(Mount, void(const std::string&, MountType, MountCallback,
                           ErrorCallback));
  MOCK_METHOD3(Unmount, void(const std::string&, UnmountCallback,
                             ErrorCallback));
  MOCK_METHOD2(EnumerateAutoMountableDevices, void(
      EnumerateAutoMountableDevicesCallback, ErrorCallback));
  MOCK_METHOD4(FormatDevice, void(const std::string&, const std::string&,
                                  FormatDeviceCallback, ErrorCallback));
  MOCK_METHOD3(GetDeviceProperties, void(
      const std::string&, GetDevicePropertiesCallback, ErrorCallback));
  MOCK_METHOD2(SetUpConnections, void(MountEventHandler,
                                      MountCompletedHandler));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_MOCK_CROS_DISKS_CLIENT_H_
