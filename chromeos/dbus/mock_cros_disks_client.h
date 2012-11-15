// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_CROS_DISKS_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_CROS_DISKS_CLIENT_H_

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
  MOCK_METHOD4(Unmount, void(const std::string&,
                             UnmountOptions,
                             const UnmountCallback&,
                             const UnmountCallback&));
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
  // Warning: if this method is mocked, SendMountEvent and
  // SendMountCompletedEvent may not work.
  MOCK_METHOD2(SetUpConnections, void(const MountEventHandler&,
                                      const MountCompletedHandler&));

  // Used to simulate signals sent by cros disks layer.
  // Invokes handlers set in |SetUpConnections|.
  bool SendMountEvent(MountEventType event, const std::string& path);
  bool SendMountCompletedEvent(MountError error_code,
                               const std::string& source_path,
                               MountType mount_type,
                               const std::string& mount_path);

 private:
  // Default implementation of SetupConnections.
  // Sets internal MounteEvent and MountCompleted handlers.
  void SetUpConnectionsInternal(
      const MountEventHandler& mount_event_handler,
      const MountCompletedHandler& mount_completed_handler);

  MountEventHandler mount_event_handler_;
  MountCompletedHandler mount_completed_handler_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_CROS_DISKS_CLIENT_H_
