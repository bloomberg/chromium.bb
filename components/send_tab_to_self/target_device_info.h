// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_INFO_H_
#define COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_INFO_H_

#include <string>

#include "components/sync/protocol/sync.pb.h"

namespace send_tab_to_self {
// Device information for generating send tab to self UI.
struct TargetDeviceInfo {
 public:
  TargetDeviceInfo(const std::string& cache_guid,
                   const sync_pb::SyncEnums::DeviceType device_type);
  TargetDeviceInfo(const TargetDeviceInfo& other) = default;
  ~TargetDeviceInfo() = default;

  bool operator==(const TargetDeviceInfo& rhs) const;

  // Device guid.
  std::string cache_guid;
  // Device type.
  sync_pb::SyncEnums::DeviceType device_type;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_TARGET_DEVICE_INFO_H_
