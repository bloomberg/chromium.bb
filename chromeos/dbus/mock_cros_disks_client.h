// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_CROS_DISKS_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_CROS_DISKS_CLIENT_H_
#pragma once

#include <string>

#include "chromeos/dbus/cros_disks_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockCrosDisksClient : public CrosDisksClient {
 public:
  MockCrosDisksClient();
  virtual ~MockCrosDisksClient();

  MOCK_METHOD6(Mount, void(const std::string&,
                           const std::string&,
                           const std::string&,
                           MountType,
                           const MountCallback&,
                           const ErrorCallback&));
  MOCK_METHOD3(Unmount, void(const std::string&,
                             const UnmountCallback&,
                             const ErrorCallback&));
  MOCK_METHOD2(EnumerateAutoMountableDevices,
               void(const EnumerateAutoMountableDevicesCallback&,
                    const ErrorCallback&));
  MOCK_METHOD4(FormatDevice, void(const std::string&,
                                  const std::string&,
                                  const FormatDeviceCallback&,
                                  const ErrorCallback&));
  MOCK_METHOD3(GetDeviceProperties, void(const std::string&,
                                         const GetDevicePropertiesCallback&,
                                         const ErrorCallback&));
  MOCK_METHOD2(SetUpConnections, void(const MountEventHandler&,
                                      const MountCompletedHandler&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_CROS_DISKS_CLIENT_H_
