// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_LOCAL_DEVICE_METADATA_MANAGER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_LOCAL_DEVICE_METADATA_MANAGER_H_

#include <unordered_map>

#include "base/macros.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "components/cryptauth/remote_device_ref.h"

namespace chromeos {

namespace secure_channel {

// Provides the ability set/fetch data associated with the local device.
class LocalDeviceMetadataManager {
 public:
  LocalDeviceMetadataManager();
  virtual ~LocalDeviceMetadataManager();

  // Sets the default local device metadata; this function is intended to be
  // called when the user logs in.
  void SetDefaultLocalDeviceDataMetadata(
      cryptauth::RemoteDeviceRef default_local_device_metadata);

  // Return value is base::nullopt if SetDefaultLocalDeviceDataMetadata() has
  // not yet been called.
  const base::Optional<cryptauth::RemoteDeviceRef>&
  GetDefaultLocalDeviceMetadata() const;

  // Sets the local device metadata for the request with id |request_id|. This
  // function is intended to be used before the user logs in; before log-in,
  // the local device can be represented by several RemoteDeviceRef objects,
  // each specific to a different user's account.
  void SetLocalDeviceMetadataForRequest(
      const base::UnguessableToken& request_id,
      cryptauth::RemoteDeviceRef default_local_device_metadata);

  // Return value is base::nullopt if SetLocalDeviceMetadataForRequest() has
  // not yet been called for the |request_id|.
  base::Optional<cryptauth::RemoteDeviceRef> GetLocalDeviceMetadataForRequest(
      const base::UnguessableToken& request_id) const;

 private:
  base::Optional<cryptauth::RemoteDeviceRef> default_local_device_metadata_;
  std::unordered_map<base::UnguessableToken,
                     cryptauth::RemoteDeviceRef,
                     base::UnguessableTokenHash>
      request_id_to_metadata_map_;

  DISALLOW_COPY_AND_ASSIGN(LocalDeviceMetadataManager);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_LOCAL_DEVICE_METADATA_MANAGER_H_
