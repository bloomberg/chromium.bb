// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_
#define CHROME_BROWSER_SYNC_GLUE_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/sync/glue/local_device_info_provider.h"

namespace browser_sync {

class LocalDeviceInfoProviderImpl : public LocalDeviceInfoProvider {
 public:
  LocalDeviceInfoProviderImpl();
  virtual ~LocalDeviceInfoProviderImpl();

  // LocalDeviceInfoProvider implementation.
  virtual const DeviceInfo* GetLocalDeviceInfo() const OVERRIDE;
  virtual std::string GetLocalSyncCacheGUID() const OVERRIDE;
  virtual void Initialize(
      const std::string& cache_guid,
      const std::string& signin_scoped_device_id) OVERRIDE;
  virtual scoped_ptr<Subscription> RegisterOnInitializedCallback(
    const base::Closure& callback) OVERRIDE;

 private:
  void InitializeContinuation(const DeviceInfo& local_info);

  std::string cache_guid_;
  scoped_ptr<DeviceInfo> local_device_info_;
  base::CallbackList<void(void)> callback_list_;
  base::WeakPtrFactory<LocalDeviceInfoProviderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalDeviceInfoProviderImpl);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_
