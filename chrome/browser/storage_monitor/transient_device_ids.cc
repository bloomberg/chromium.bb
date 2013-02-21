// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TransientDeviceIds implementation.

#include "chrome/browser/storage_monitor/transient_device_ids.h"

#include "base/logging.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"

namespace chrome {

TransientDeviceIds::TransientDeviceIds() : next_transient_id_(1) {}

TransientDeviceIds::~TransientDeviceIds() {}

uint64 TransientDeviceIds::GetTransientIdForDeviceId(
    const std::string& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(MediaStorageUtil::IsRemovableDevice(device_id));

  bool inserted =
      id_map_.insert(std::make_pair(device_id, next_transient_id_)).second;
  if (inserted) {
    // Inserted a device that has never been seen before.
    ++next_transient_id_;
  }
  return id_map_[device_id];
}

const std::string TransientDeviceIds::DeviceIdFromTransientId(
    uint64 transient_id) const {
  for (DeviceIdToTransientIdMap::const_iterator iter = id_map_.begin();
       iter != id_map_.end(); ++iter) {
    if (iter->second == transient_id)
      return iter->first;
  }
  return std::string();
}

}  // namespace chrome
