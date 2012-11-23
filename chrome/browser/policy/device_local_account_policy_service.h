// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/policy/cloud_policy_store.h"

namespace chromeos {
class SessionManagerClient;
}

namespace enterprise_management {
class CloudPolicySettings;
class PolicyData;
class PolicyFetchResponse;
}

namespace policy {

class DeviceManagementService;

// This class manages the policy blob for a single device-local account, taking
// care of interaction with session_manager and doing validation.
class DeviceLocalAccountPolicyBroker {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnLoadComplete(const std::string& account_id) = 0;
  };

  DeviceLocalAccountPolicyBroker(
      const std::string& account_id,
      chromeos::SessionManagerClient* session_manager_client,
      chromeos::DeviceSettingsService* device_settings_service,
      Delegate* delegate);
  ~DeviceLocalAccountPolicyBroker();

  const std::string& account_id() const { return account_id_; }
  const enterprise_management::PolicyData* policy_data() const {
    return policy_data_.get();
  }
  const enterprise_management::CloudPolicySettings* policy_settings() const {
    return policy_settings_.get();
  }
  CloudPolicyStore::Status status() const { return status_; }
  CloudPolicyValidatorBase::Status validation_status() const {
    return validation_status_;
  }

  // Loads the policy from session_manager.
  void Load();

  // Send policy to session_manager to store it.
  void Store(const enterprise_management::PolicyFetchResponse& policy);

 private:
  // Called back by |session_manager_client_| after policy retrieval. Checks for
  // success and triggers policy validation.
  void ValidateLoadedPolicyBlob(const std::string& policy_blob);

  // Updates state after validation and notifies the delegate.
  void UpdatePolicy(UserCloudPolicyValidator* validator);

  // Sends the policy blob to session_manager for storing after validation.
  void StoreValidatedPolicy(UserCloudPolicyValidator* validator);

  // Called back when a store operation completes, updates state and reloads the
  // policy if applicable.
  void HandleStoreResult(bool result);

  // Gets the owner key and triggers policy validation.
  void CheckKeyAndValidate(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy,
      const UserCloudPolicyValidator::CompletionCallback& callback);

  // Triggers policy validation.
  void Validate(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy,
      const UserCloudPolicyValidator::CompletionCallback& callback,
      chromeos::DeviceSettingsService::OwnershipStatus ownership_status,
      bool is_owner);

  const std::string account_id_;
  chromeos::SessionManagerClient* session_manager_client_;
  chromeos::DeviceSettingsService* device_settings_service_;
  Delegate* delegate_;

  base::WeakPtrFactory<DeviceLocalAccountPolicyBroker> weak_factory_;

  // Current policy.
  scoped_ptr<enterprise_management::PolicyData> policy_data_;
  scoped_ptr<enterprise_management::CloudPolicySettings> policy_settings_;

  // Status codes.
  CloudPolicyStore::Status status_;
  CloudPolicyValidatorBase::Status validation_status_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyBroker);
};

// Manages user policy blobs for device-local accounts present on the device.
// The actual policy blobs are brokered by session_manager (to prevent file
// manipulation), and we're making signature checks on the policy blobs to
// ensure they're issued by the device owner.
class DeviceLocalAccountPolicyService
    : public chromeos::DeviceSettingsService::Observer,
      public DeviceLocalAccountPolicyBroker::Delegate {
 public:
  // Interface for interested parties to observe policy changes.
  class Observer {
   public:
    virtual ~Observer() {}

    // Policy for the given account has changed.
    virtual void OnPolicyChanged(const std::string& account_id) = 0;

    // The list of accounts has been updated.
    virtual void OnDeviceLocalAccountsChanged() = 0;
  };

  DeviceLocalAccountPolicyService(
      chromeos::SessionManagerClient* session_manager_client,
      chromeos::DeviceSettingsService* device_settings_service);
  virtual ~DeviceLocalAccountPolicyService();

  // Initializes the cloud policy service connection.
  void Initialize(DeviceManagementService* device_management_service);

  // Prevents further policy fetches from the cloud.
  void Shutdown();

  // Get the policy broker for a given account. Returns NULL if that account is
  // not valid.
  DeviceLocalAccountPolicyBroker* GetBrokerForAccount(
      const std::string& account_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // DeviceSettingsService::Observer:
  virtual void OwnershipStatusChanged() OVERRIDE;
  virtual void DeviceSettingsUpdated() OVERRIDE;

 private:
  // Re-queries the list of defined device-local accounts from device settings
  // and updates |policy_brokers_| to match that list.
  void UpdateAccountList(
      const enterprise_management::ChromeDeviceSettingsProto& device_settings);

  // DeviceLocalAccountPolicyBroker::Delegate:
  virtual void OnLoadComplete(const std::string& account_id) OVERRIDE;

  chromeos::SessionManagerClient* session_manager_client_;
  chromeos::DeviceSettingsService* device_settings_service_;

  DeviceManagementService* device_management_service_;

  // The device-local account policy brokers, keyed by account ID.
  typedef std::map<std::string, DeviceLocalAccountPolicyBroker*>
      PolicyBrokerMap;
  PolicyBrokerMap policy_brokers_;

  ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
