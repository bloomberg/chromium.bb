// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_cros_disks_client.h"

using testing::_;
using testing::Invoke;

namespace chromeos {

MockCrosDisksClient::MockCrosDisksClient() {
  ON_CALL(*this, SetUpConnections(_, _))
      .WillByDefault(Invoke(this,
                            &MockCrosDisksClient::SetUpConnectionsInternal));
}

MockCrosDisksClient::~MockCrosDisksClient() {}

bool MockCrosDisksClient::SendMountEvent(MountEventType event,
                                         const std::string& path) {
  if (mount_event_handler_.is_null())
    return false;
  mount_event_handler_.Run(event, path);
  return true;
}

bool MockCrosDisksClient::SendMountCompletedEvent(
    MountError error_code,
    const std::string& source_path,
    MountType mount_type,
    const std::string& mount_path) {
  if (mount_completed_handler_.is_null())
    return false;
  mount_completed_handler_.Run(error_code, source_path, mount_type, mount_path);
  return true;
}

void MockCrosDisksClient::SetUpConnectionsInternal(
    const MountEventHandler& mount_event_handler,
    const MountCompletedHandler& mount_completed_handler) {
  mount_event_handler_ = mount_event_handler;
  mount_completed_handler_ = mount_completed_handler;
}

}  // namespace chromeos
