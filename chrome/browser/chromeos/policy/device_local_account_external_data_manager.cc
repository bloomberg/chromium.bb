// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_local_account_external_data_manager.h"

#include <memory>

#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_store.h"
#include "chrome/browser/chromeos/policy/device_local_account_external_data_service.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/policy_constants.h"

namespace policy {

DeviceLocalAccountExternalDataManager::DeviceLocalAccountExternalDataManager(
    const std::string& account_id,
    const GetChromePolicyDetailsCallback& get_policy_details,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    ResourceCache* resource_cache)
    : CloudExternalDataManagerBase(get_policy_details, backend_task_runner) {
  SetExternalDataStore(std::make_unique<CloudExternalDataStore>(
      account_id, backend_task_runner, resource_cache));
}

DeviceLocalAccountExternalDataManager::
    ~DeviceLocalAccountExternalDataManager() {
  SetExternalDataStore(std::unique_ptr<CloudExternalDataStore>());
}

void DeviceLocalAccountExternalDataManager::OnPolicyStoreLoaded() {
  CloudExternalDataManagerBase::OnPolicyStoreLoaded();
  // Proactively try to download and cache all external data referenced by
  // policies.
  FetchAll();
}

}  // namespace policy
