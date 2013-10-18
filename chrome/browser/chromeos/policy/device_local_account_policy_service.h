// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/cloud/cloud_policy_core.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {
class CrosSettings;
class DeviceSettingsService;
class SessionManagerClient;
}

namespace policy {

class CloudPolicyClient;
class DeviceLocalAccountPolicyStore;
class DeviceManagementService;

// The main switching central that downloads, caches, refreshes, etc. policy for
// a single device-local account.
class DeviceLocalAccountPolicyBroker {
 public:
  // |task_runner| is the runner for policy refresh tasks.
  explicit DeviceLocalAccountPolicyBroker(
      const std::string& user_id,
      scoped_ptr<DeviceLocalAccountPolicyStore> store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~DeviceLocalAccountPolicyBroker();

  const std::string& user_id() const { return user_id_; }

  CloudPolicyCore* core() { return &core_; }
  const CloudPolicyCore* core() const { return &core_; }

  // Establish a cloud connection for the service.
  void Connect(scoped_ptr<CloudPolicyClient> client);

  // Destroy the cloud connection, stopping policy refreshes.
  void Disconnect();

  // Reads the refresh delay from policy and configures the refresh scheduler.
  void UpdateRefreshDelay();

  // Retrieves the display name for the account as stored in policy. Returns an
  // empty string if the policy is not present.
  std::string GetDisplayName() const;

 private:
  const std::string user_id_;
  scoped_ptr<DeviceLocalAccountPolicyStore> store_;
  CloudPolicyCore core_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyBroker);
};

// Manages user policy blobs for device-local accounts present on the device.
// The actual policy blobs are brokered by session_manager (to prevent file
// manipulation), and we're making signature checks on the policy blobs to
// ensure they're issued by the device owner.
class DeviceLocalAccountPolicyService : public CloudPolicyStore::Observer {
 public:
  // Interface for interested parties to observe policy changes.
  class Observer {
   public:
    virtual ~Observer() {}

    // Policy for the given |user_id| has changed.
    virtual void OnPolicyUpdated(const std::string& user_id) = 0;

    // The list of accounts has been updated.
    virtual void OnDeviceLocalAccountsChanged() = 0;
  };

  DeviceLocalAccountPolicyService(
      chromeos::SessionManagerClient* session_manager_client,
      chromeos::DeviceSettingsService* device_settings_service,
      chromeos::CrosSettings* cros_settings);
  virtual ~DeviceLocalAccountPolicyService();

  // Initializes the cloud policy service connection.
  void Connect(DeviceManagementService* device_management_service);

  // Prevents further policy fetches from the cloud.
  void Disconnect();

  // Get the policy broker for a given |user_id|. Returns NULL if that |user_id|
  // does not belong to an existing device-local account.
  DeviceLocalAccountPolicyBroker* GetBrokerForUser(const std::string& user_id);

  // Indicates whether policy has been successfully fetched for the given
  // |user_id|.
  bool IsPolicyAvailableForUser(const std::string& user_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

 private:
  struct PolicyBrokerWrapper {
    PolicyBrokerWrapper();

    // Return the |broker|, creating it first if necessary.
    DeviceLocalAccountPolicyBroker* GetBroker();

    // Fire up the cloud connection for fetching policy for the account from the
    // cloud if this is an enterprise-managed device.
    void ConnectIfPossible();

    // Destroy the cloud connection.
    void Disconnect();

    // Delete the broker.
    void DeleteBroker();

    std::string user_id;
    std::string account_id;
    DeviceLocalAccountPolicyService* parent;
    DeviceLocalAccountPolicyBroker* broker;
  };

  typedef std::map<std::string, PolicyBrokerWrapper> PolicyBrokerMap;

  // Re-queries the list of defined device-local accounts from device settings
  // and updates |policy_brokers_| to match that list.
  void UpdateAccountList();

  // Calls |UpdateAccountList| if there are no previous calls pending.
  void UpdateAccountListIfNonePending();

  // Deletes brokers in |map| and clears it.
  void DeleteBrokers(PolicyBrokerMap* map);

  // Find the broker for a given |store|. Returns NULL if |store| is unknown.
  DeviceLocalAccountPolicyBroker* GetBrokerForStore(CloudPolicyStore* store);

  chromeos::SessionManagerClient* session_manager_client_;
  chromeos::DeviceSettingsService* device_settings_service_;
  chromeos::CrosSettings* cros_settings_;

  DeviceManagementService* device_management_service_;

  // The device-local account policy brokers, keyed by user ID.
  PolicyBrokerMap policy_brokers_;

  ObserverList<Observer, true> observers_;

  scoped_ptr<chromeos::CrosSettings::ObserverSubscription>
      local_accounts_subscription_;

  // Weak pointer factory for cros_settings_->PrepareTrustedValues() callbacks.
  base::WeakPtrFactory<DeviceLocalAccountPolicyService>
      cros_settings_callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
