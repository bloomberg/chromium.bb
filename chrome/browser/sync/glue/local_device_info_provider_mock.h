// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_LOCAL_DEVICE_INFO_PROVIDER_MOCK_H_
#define CHROME_BROWSER_SYNC_GLUE_LOCAL_DEVICE_INFO_PROVIDER_MOCK_H_

#include "components/sync_driver/device_info.h"
#include "components/sync_driver/local_device_info_provider.h"

namespace browser_sync {

class LocalDeviceInfoProviderMock
    : public sync_driver::LocalDeviceInfoProvider {
 public:
  // Creates uninitialized provider.
  LocalDeviceInfoProviderMock();
  // Creates initialized provider with the specified device info.
  LocalDeviceInfoProviderMock(
      const std::string& guid,
      const std::string& client_name,
      const std::string& chrome_version,
      const std::string& sync_user_agent,
      const sync_pb::SyncEnums::DeviceType device_type,
      const std::string& signin_scoped_device_id);
  virtual ~LocalDeviceInfoProviderMock();

  virtual const sync_driver::DeviceInfo* GetLocalDeviceInfo() const OVERRIDE;
  virtual std::string GetLocalSyncCacheGUID() const OVERRIDE;
  virtual void Initialize(
      const std::string& cache_guid,
      const std::string& signin_scoped_device_id) OVERRIDE;
  virtual scoped_ptr<Subscription> RegisterOnInitializedCallback(
    const base::Closure& callback) OVERRIDE;

  void SetInitialized(bool is_initialized);

 private:
  bool is_initialized_;

  scoped_ptr<sync_driver::DeviceInfo> local_device_info_;
  base::CallbackList<void(void)> callback_list_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_LOCAL_DEVICE_INFO_PROVIDER_MOCK_H_
