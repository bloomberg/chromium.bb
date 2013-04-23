// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CROS_DISKS_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CROS_DISKS_CLIENT_H_

#include <string>

#include "chromeos/dbus/cros_disks_client.h"

namespace chromeos {

// A fake implementation of CrosDiskeClient. This class provides a fake behavior
// and the user of this class can raise a fake mouse events.
class FakeCrosDisksClient : public CrosDisksClient {
 public:
  FakeCrosDisksClient();
  virtual ~FakeCrosDisksClient();

  // CrosDisksClient overrides.
  virtual void Mount(const std::string& source_path,
                     const std::string& source_format,
                     const std::string& mount_label,
                     MountType type,
                     const MountCallback& callback,
                     const ErrorCallback& error_callback) OVERRIDE;
  virtual void Unmount(const std::string& device_path,
                       UnmountOptions options,
                       const UnmountCallback& callback,
                       const UnmountCallback& error_callback) OVERRIDE;
  virtual void EnumerateAutoMountableDevices(
      const EnumerateAutoMountableDevicesCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void FormatDevice(const std::string& device_path,
                            const std::string& filesystem,
                            const FormatDeviceCallback& callback,
                            const ErrorCallback& error_callback) OVERRIDE;
  virtual void GetDeviceProperties(
      const std::string& device_path,
      const GetDevicePropertiesCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void SetUpConnections(
      const MountEventHandler& mount_event_handler,
      const MountCompletedHandler& mount_completed_handler) OVERRIDE;

  // Used in tests to simulate signals sent by cros disks layer.
  // Invokes handlers set in |SetUpConnections|.
  bool SendMountEvent(MountEventType event, const std::string& path);
  bool SendMountCompletedEvent(MountError error_code,
                               const std::string& source_path,
                               MountType mount_type,
                               const std::string& mount_path);

 private:
  MountEventHandler mount_event_handler_;
  MountCompletedHandler mount_completed_handler_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CROS_DISKS_CLIENT_H_
