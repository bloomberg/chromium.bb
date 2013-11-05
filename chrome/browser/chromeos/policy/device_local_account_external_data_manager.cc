// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_local_account_external_data_manager.h"

#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_store.h"
#include "chrome/browser/chromeos/policy/device_local_account_external_data_service.h"
#include "chrome/browser/policy/cloud/resource_cache.h"
#include "policy/policy_constants.h"

namespace policy {

DeviceLocalAccountExternalDataManager::DeviceLocalAccountExternalDataManager(
    const std::string& account_id,
    const PolicyDefinitionList* policy_definitions,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    ResourceCache* resource_cache)
    : CloudExternalDataManagerBase(policy_definitions,
                                   backend_task_runner,
                                   io_task_runner) {
  SetExternalDataStore(make_scoped_ptr(new CloudExternalDataStore(
      account_id, backend_task_runner, resource_cache)));
}

DeviceLocalAccountExternalDataManager::
    ~DeviceLocalAccountExternalDataManager() {
  SetExternalDataStore(scoped_ptr<CloudExternalDataStore>());
}

void DeviceLocalAccountExternalDataManager::OnPolicyStoreLoaded() {
  CloudExternalDataManagerBase::OnPolicyStoreLoaded();
  // Proactively try to download and cache all external data referenced by
  // policies.
  FetchAll();
}

}  // namespace policy
