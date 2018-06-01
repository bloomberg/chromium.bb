// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/local_device_metadata_manager.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace secure_channel {

LocalDeviceMetadataManager::LocalDeviceMetadataManager() = default;

LocalDeviceMetadataManager::~LocalDeviceMetadataManager() = default;

void LocalDeviceMetadataManager::SetDefaultLocalDeviceDataMetadata(
    cryptauth::RemoteDeviceRef default_local_device_metadata) {
  if (default_local_device_metadata_) {
    PA_LOG(WARNING) << "LocalDeviceMetadataManager::"
                    << "SetDefaultLocalDeviceDataMetadata(): Setting the "
                    << "default local device metadata, but one was already "
                    << "set. Overwriting the old one.";
  }

  default_local_device_metadata_ = default_local_device_metadata;
}

const base::Optional<cryptauth::RemoteDeviceRef>&
LocalDeviceMetadataManager::GetDefaultLocalDeviceMetadata() const {
  return default_local_device_metadata_;
}

void LocalDeviceMetadataManager::SetLocalDeviceMetadataForRequest(
    const base::UnguessableToken& request_id,
    cryptauth::RemoteDeviceRef local_device_metadata) {
  if (base::ContainsKey(request_id_to_metadata_map_, request_id)) {
    PA_LOG(ERROR) << "LocalDeviceMetadataManager::"
                  << "SetLocalDeviceMetadataForRequest(): Setting local device "
                  << "metadata for request ID " << request_id << ", but that "
                  << "request already had metadata set.";
    NOTREACHED();
  }

  request_id_to_metadata_map_.insert(
      std::make_pair(request_id, local_device_metadata));
}

base::Optional<cryptauth::RemoteDeviceRef>
LocalDeviceMetadataManager::GetLocalDeviceMetadataForRequest(
    const base::UnguessableToken& request_id) const {
  if (base::ContainsKey(request_id_to_metadata_map_, request_id))
    return request_id_to_metadata_map_.at(request_id);

  return base::nullopt;
}

}  // namespace secure_channel

}  // namespace chromeos
