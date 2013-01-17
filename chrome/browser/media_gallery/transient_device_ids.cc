// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TransientDeviceIds implementation.

#include "chrome/browser/media_gallery/transient_device_ids.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/system_monitor/media_storage_util.h"

namespace chrome {

TransientDeviceIds::TransientDeviceIds() : next_transient_id_(0) {}

TransientDeviceIds::~TransientDeviceIds() {}

std::string TransientDeviceIds::GetTransientIdForDeviceId(
    const std::string& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(MediaStorageUtil::IsRemovableDevice(device_id));

  bool inserted =
      id_map_.insert(std::make_pair(device_id, next_transient_id_)).second;
  if (inserted) {
    // Inserted a device that has never been seen before.
    ++next_transient_id_;
  }
  return base::Uint64ToString(id_map_[device_id]);
}

}  // namespace chrome
