// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_STORE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_STORE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store_base.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {
class DeviceSettingsService;
class SessionManagerClient;
}

namespace enterprise_management {
class PolicyFetchResponse;
}

namespace policy {

class DeviceLocalAccountPolicyBroker;

// CloudPolicyStore implementation for device-local account policy. Stores/loads
// policy to/from session_manager.
class DeviceLocalAccountPolicyStore
    : public UserCloudPolicyStoreBase {
 public:
  DeviceLocalAccountPolicyStore(
      const std::string& account_id,
      chromeos::SessionManagerClient* client,
      chromeos::DeviceSettingsService* device_settings_service,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  virtual ~DeviceLocalAccountPolicyStore();

  const std::string& account_id() const { return account_id_; }

  // CloudPolicyStore:
  virtual void Store(
      const enterprise_management::PolicyFetchResponse& policy) OVERRIDE;
  virtual void Load() OVERRIDE;

 private:
  // Called back by |session_manager_client_| after policy retrieval. Checks for
  // success and triggers policy validation.
  void ValidateLoadedPolicyBlob(const std::string& policy_blob);

  // Updates state after validation and notifies observers.
  void UpdatePolicy(UserCloudPolicyValidator* validator);

  // Sends the policy blob to session_manager for storing after validation.
  void StoreValidatedPolicy(UserCloudPolicyValidator* validator);

  // Called back when a store operation completes, updates state and reloads the
  // policy if applicable.
  void HandleStoreResult(bool result);

  // Gets the owner key and triggers policy validation.
  void CheckKeyAndValidate(
      bool valid_timestamp_required,
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy,
      const UserCloudPolicyValidator::CompletionCallback& callback);

  // Triggers policy validation.
  void Validate(
      bool valid_timestamp_required,
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy,
      const UserCloudPolicyValidator::CompletionCallback& callback,
      chromeos::DeviceSettingsService::OwnershipStatus ownership_status);

  const std::string account_id_;
  chromeos::SessionManagerClient* session_manager_client_;
  chromeos::DeviceSettingsService* device_settings_service_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::WeakPtrFactory<DeviceLocalAccountPolicyStore> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyStore);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_STORE_H_
