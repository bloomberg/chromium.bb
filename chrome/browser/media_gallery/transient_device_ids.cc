// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TransientDeviceIds implementation.

#include "chrome/browser/media_gallery/transient_device_ids.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"

namespace chrome {

TransientDeviceIds::TransientDeviceIds() : next_transient_id_(0) {}

TransientDeviceIds::~TransientDeviceIds() {}

std::string TransientDeviceIds::GetTransientIdForDeviceId(
    const std::string& device_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DeviceIdToTransientIdMap::const_iterator it = id_map_.find(device_id);
  CHECK(it != id_map_.end());
  return base::Uint64ToString(it->second);
}

void TransientDeviceIds::DeviceAttached(const std::string& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool inserted =
      id_map_.insert(std::make_pair(device_id, next_transient_id_)).second;
  if (inserted) {
    // Inserted a device that has never been seen before.
    ++next_transient_id_;
  }
}

}  // namespace chrome
