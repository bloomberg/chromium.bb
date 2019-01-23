// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace syncer {

class DeviceInfoTracker;
class LocalDeviceInfoProvider;
class ModelTypeControllerDelegate;

// Abstract interface for a keyed service responsible for implementing sync
// datatype DEVICE_INFO and exposes information about the local device (as
// understood by sync) as well as remove syncing devices.
class DeviceInfoSyncService : public KeyedService {
 public:
  ~DeviceInfoSyncService() override;

  // Interface to get information about the local syncing device.
  virtual LocalDeviceInfoProvider* GetLocalDeviceInfoProvider() = 0;

  // Interface to get information about all syncing devices.
  virtual DeviceInfoTracker* GetDeviceInfoTracker() = 0;

  // Returns the ModelTypeControllerDelegate for DEVICE_INFO.
  virtual base::WeakPtr<ModelTypeControllerDelegate>
  GetControllerDelegate() = 0;

  // Used by ProfileSyncService when sync is starting.
  // TODO(crbug.com/906611): Ideally this API could be avoided if |session_name|
  // were part of DataTypeActivationRequest, such that the bridge would receive
  // the information directly during sync start. This could also allow removing
  // LocalDeviceInfoProvider::RegisterOnInitializedCallback().
  virtual void InitLocalCacheGuid(const std::string& cache_guid,
                                  const std::string& session_name) = 0;

  // Used by ProfileSyncService when sync is disabled.
  virtual void ClearLocalCacheGuid() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_SERVICE_H_
