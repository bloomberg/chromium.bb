// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_external_data_manager.h"

#include "base/location.h"
#include "base/memory/scoped_ptr.h"
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
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    const base::FilePath& cache_path,
    CloudPolicyStore* policy_store)
    : CloudExternalDataManagerBase(get_policy_details,
                                   backend_task_runner,
                                   io_task_runner),
      resource_cache_(new ResourceCache(cache_path, backend_task_runner)) {
  SetPolicyStore(policy_store);
  SetExternalDataStore(make_scoped_ptr(new CloudExternalDataStore(
      kCacheKey, backend_task_runner, resource_cache_)));
}

UserCloudExternalDataManager::~UserCloudExternalDataManager() {
  SetExternalDataStore(scoped_ptr<CloudExternalDataStore>());
  backend_task_runner_->DeleteSoon(FROM_HERE, resource_cache_);
}

}  // namespace policy
