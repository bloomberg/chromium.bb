// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/policy/cloud_policy_core.h"
#include "chrome/browser/policy/cloud_policy_store.h"

namespace chromeos {
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
  explicit DeviceLocalAccountPolicyBroker(
      scoped_ptr<DeviceLocalAccountPolicyStore> store);
  ~DeviceLocalAccountPolicyBroker();

  const std::string& account_id() const;

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
  const std::string account_id_;
  scoped_ptr<DeviceLocalAccountPolicyStore> store_;
  CloudPolicyCore core_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyBroker);
};

// Manages user policy blobs for device-local accounts present on the device.
// The actual policy blobs are brokered by session_manager (to prevent file
// manipulation), and we're making signature checks on the policy blobs to
// ensure they're issued by the device owner.
class DeviceLocalAccountPolicyService
    : public chromeos::DeviceSettingsService::Observer,
      public CloudPolicyStore::Observer {
 public:
  // Interface for interested parties to observe policy changes.
  class Observer {
   public:
    virtual ~Observer() {}

    // Policy for the given account has changed.
    virtual void OnPolicyUpdated(const std::string& account_id) = 0;

    // The list of accounts has been updated.
    virtual void OnDeviceLocalAccountsChanged() = 0;
  };

  DeviceLocalAccountPolicyService(
      chromeos::SessionManagerClient* session_manager_client,
      chromeos::DeviceSettingsService* device_settings_service);
  virtual ~DeviceLocalAccountPolicyService();

  // Initializes the cloud policy service connection.
  void Connect(DeviceManagementService* device_management_service);

  // Prevents further policy fetches from the cloud.
  void Disconnect();

  // Get the policy broker for a given account. Returns NULL if that account is
  // not valid.
  DeviceLocalAccountPolicyBroker* GetBrokerForAccount(
      const std::string& account_id);

  // Indicates whether policy has been successfully fetched for the given
  // account.
  bool IsPolicyAvailableForAccount(const std::string& account_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // DeviceSettingsService::Observer:
  virtual void OwnershipStatusChanged() OVERRIDE;
  virtual void DeviceSettingsUpdated() OVERRIDE;

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

 private:
  typedef std::map<std::string, DeviceLocalAccountPolicyBroker*>
      PolicyBrokerMap;

  // Re-queries the list of defined device-local accounts from device settings
  // and updates |policy_brokers_| to match that list.
  void UpdateAccountList(
      const enterprise_management::ChromeDeviceSettingsProto& device_settings);

  // Creates a broker for the given account ID.
  scoped_ptr<DeviceLocalAccountPolicyBroker> CreateBroker(
      const std::string& account_id);

  // Deletes brokers in |map| and clears it.
  void DeleteBrokers(PolicyBrokerMap* map);

  // Find the broker for a given |store|. Returns NULL if |store| is unknown.
  DeviceLocalAccountPolicyBroker* GetBrokerForStore(CloudPolicyStore* store);

  // Creates and initializes a cloud policy client for |account_id|. Returns
  // NULL if the device doesn't have credentials in device settings (i.e. is not
  // enterprise-enrolled).
  scoped_ptr<CloudPolicyClient> CreateClientForAccount(
      const std::string& account_id);

  chromeos::SessionManagerClient* session_manager_client_;
  chromeos::DeviceSettingsService* device_settings_service_;

  DeviceManagementService* device_management_service_;

  // The device-local account policy brokers, keyed by account ID.
  PolicyBrokerMap policy_brokers_;

  ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
