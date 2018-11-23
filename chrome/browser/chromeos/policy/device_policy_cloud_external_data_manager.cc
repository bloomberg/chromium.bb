// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_cloud_external_data_manager.h"

#include <stdint.h>
#include <utility>

#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_store.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/resource_cache.h"

namespace policy {

namespace {

const char kCacheKey[] = "device_policy_external_data";

// Maximum size of the device policy external data cache directory set to 10MB.
const int64_t kCacheMaxSize = 10 * 1024 * 1024;
// Only used for tests.
int64_t g_cache_max_size_override = 0;

}  // namespace

DevicePolicyCloudExternalDataManager::DevicePolicyCloudExternalDataManager(
    const GetChromePolicyDetailsCallback& get_policy_details,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    const base::FilePath& cache_path,
    CloudPolicyStore* policy_store)
    : CloudExternalDataManagerBase(get_policy_details, backend_task_runner) {
  int cache_max_size = kCacheMaxSize;
  if (g_cache_max_size_override != 0)
    cache_max_size = g_cache_max_size_override;
  resource_cache_ = std::make_unique<ResourceCache>(
      cache_path, backend_task_runner, cache_max_size);

  SetPolicyStore(policy_store);
  SetExternalDataStore(std::make_unique<CloudExternalDataStore>(
      kCacheKey, backend_task_runner, resource_cache_.get()));
}

DevicePolicyCloudExternalDataManager::~DevicePolicyCloudExternalDataManager() {
  SetExternalDataStore(nullptr);
  // Delete resource_cache_ on the background task runner to ensure that the
  // delete is sequenced with the external data store's usage.
  backend_task_runner_->DeleteSoon(FROM_HERE, std::move(resource_cache_));
}

void DevicePolicyCloudExternalDataManager::SetCacheMaxSizeForTesting(
    int64_t cache_max_size) {
  g_cache_max_size_override = cache_max_size;
}

}  // namespace policy
