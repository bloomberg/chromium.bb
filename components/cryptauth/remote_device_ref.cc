// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/remote_device_ref.h"

namespace cryptauth {

RemoteDeviceRef::RemoteDeviceRef(std::shared_ptr<RemoteDevice> remote_device)
    : remote_device_(std::move(remote_device)) {}

RemoteDeviceRef::RemoteDeviceRef(RemoteDeviceRef& other) = default;

RemoteDeviceRef::~RemoteDeviceRef() = default;

SoftwareFeatureState RemoteDeviceRef::GetSoftwareFeatureState(
    const SoftwareFeature& software_feature) const {
  return remote_device_->GetSoftwareFeatureState(software_feature);
}

std::string RemoteDeviceRef::GetDeviceId() const {
  return remote_device_->GetDeviceId();
}

std::string RemoteDeviceRef::GetTruncatedDeviceIdForLogs() const {
  return remote_device_->GetTruncatedDeviceIdForLogs();
}

bool RemoteDeviceRef::operator==(const RemoteDeviceRef& other) const {
  return *remote_device_ == *other.remote_device_;
}

bool RemoteDeviceRef::operator<(const RemoteDeviceRef& other) const {
  return *remote_device_ < *other.remote_device_;
}

}  // namespace cryptauth