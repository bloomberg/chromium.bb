// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_external_data_manager.h"

#include <memory>

#include "base/location.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_store.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/resource_cache.h"

namespace policy {

namespace {

const char kCacheKey[] = "data";

}  // namespace

UserCloudExternalDataManager::UserCloudExternalDataManager(
    const GetChromePolicyDetailsCallback& get_policy_details,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    const base::FilePath& cache_path,
    CloudPolicyStore* policy_store)
    : CloudExternalDataManagerBase(get_policy_details, backend_task_runner),
      resource_cache_(new ResourceCache(cache_path,
                                        backend_task_runner,
                                        /* max_cache_size */ base::nullopt)) {
  SetPolicyStore(policy_store);
  SetExternalDataStore(std::make_unique<CloudExternalDataStore>(
      kCacheKey, backend_task_runner, resource_cache_));
}

UserCloudExternalDataManager::~UserCloudExternalDataManager() {
  SetExternalDataStore(std::unique_ptr<CloudExternalDataStore>());
  backend_task_runner_->DeleteSoon(FROM_HERE, resource_cache_);
}

}  // namespace policy
