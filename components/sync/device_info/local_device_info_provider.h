// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_H_
#define COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "components/version_info/version_info.h"

namespace syncer {

class DeviceInfo;

// Interface for providing sync specific information about the
// local device.
class LocalDeviceInfoProvider {
 public:
  using Subscription = base::CallbackList<void(void)>::Subscription;

  virtual ~LocalDeviceInfoProvider() {}

  virtual version_info::Channel GetChannel() const = 0;

  // Returns sync's representation of the local device info, or nullptr if the
  // device info is unavailable (e.g. Initialize() hasn't been called). The
  // returned object is fully owned by LocalDeviceInfoProvider; it must not be
  // freed by the caller and should not be stored.
  virtual const DeviceInfo* GetLocalDeviceInfo() const = 0;

  // Constructs a user agent string (ASCII) suitable for use by the syncapi
  // for any HTTP communication. This string is used by the sync backend for
  // classifying client types when calculating statistics.
  virtual std::string GetSyncUserAgent() const = 0;

  // Registers a callback to be called when local device info becomes available.
  // The callback will remain registered until the
  // returned Subscription is destroyed, which must occur before the
  // CallbackList is destroyed.
  virtual std::unique_ptr<Subscription> RegisterOnInitializedCallback(
      const base::RepeatingClosure& callback) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_H_
