// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_EXTERNAL_DATA_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_EXTERNAL_DATA_SERVICE_H_

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/policy/device_local_account_external_data_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "components/policy/core/common/cloud/resource_cache.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class CloudPolicyStore;

// Provides DeviceLocalAccountExternalDataManagers for all device-local
// accounts. This class owns the |resource_cache_| that the managers share.
class DeviceLocalAccountExternalDataService
    : public DeviceLocalAccountPolicyService::Observer {
 public:
  DeviceLocalAccountExternalDataService(
      DeviceLocalAccountPolicyService* parent,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  ~DeviceLocalAccountExternalDataService() override;

  // DeviceLocalAccountPolicyService::Observer:
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

  scoped_refptr<DeviceLocalAccountExternalDataManager>
      GetExternalDataManager(const std::string& account_id,
                             CloudPolicyStore* policy_store);

 private:
  typedef std::map<std::string,
                   scoped_refptr<DeviceLocalAccountExternalDataManager> >
      ExternalDataManagerMap;

  DeviceLocalAccountPolicyService* parent_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  std::unique_ptr<ResourceCache> resource_cache_;

  ExternalDataManagerMap external_data_managers_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountExternalDataService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_EXTERNAL_DATA_SERVICE_H_
