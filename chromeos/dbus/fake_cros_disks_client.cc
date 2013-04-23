// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cros_disks_client.h"

namespace chromeos {

FakeCrosDisksClient::FakeCrosDisksClient() {
}

FakeCrosDisksClient::~FakeCrosDisksClient() {}

void FakeCrosDisksClient::Mount(const std::string& source_path,
                                const std::string& source_format,
                                const std::string& mount_label,
                                MountType type,
                                const MountCallback& callback,
                                const ErrorCallback& error_callback) {
}

void FakeCrosDisksClient::Unmount(const std::string& device_path,
                                  UnmountOptions options,
                                  const UnmountCallback& callback,
                                  const UnmountCallback& error_callback) {
}

void FakeCrosDisksClient::EnumerateAutoMountableDevices(
    const EnumerateAutoMountableDevicesCallback& callback,
    const ErrorCallback& error_callback) {
}

void FakeCrosDisksClient::FormatDevice(const std::string& device_path,
                                       const std::string& filesystem,
                                       const FormatDeviceCallback& callback,
                                       const ErrorCallback& error_callback) {
}

void FakeCrosDisksClient::GetDeviceProperties(
    const std::string& device_path,
    const GetDevicePropertiesCallback& callback,
    const ErrorCallback& error_callback) {
}

void FakeCrosDisksClient::SetUpConnections(
    const MountEventHandler& mount_event_handler,
    const MountCompletedHandler& mount_completed_handler) {
  mount_event_handler_ = mount_event_handler;
  mount_completed_handler_ = mount_completed_handler;
}

bool FakeCrosDisksClient::SendMountEvent(MountEventType event,
                                         const std::string& path) {
  if (mount_event_handler_.is_null())
    return false;
  mount_event_handler_.Run(event, path);
  return true;
}

bool FakeCrosDisksClient::SendMountCompletedEvent(
    MountError error_code,
    const std::string& source_path,
    MountType mount_type,
    const std::string& mount_path) {
  if (mount_completed_handler_.is_null())
    return false;
  mount_completed_handler_.Run(error_code, source_path, mount_type, mount_path);
  return true;
}

}  // namespace chromeos
