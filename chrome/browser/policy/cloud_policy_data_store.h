// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_DATA_STORE_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_DATA_STORE_H_
#pragma once

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace em = enterprise_management;

// Stores in memory all the data that is used in the cloud policy subsystem,
// and manages notification about changes to these fields.
// TODO(gfeher): The policy data stored in CloudPolicyCacheBase is currently
// an exception, move that here.
class CloudPolicyDataStore {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Notifies observers that the effective token for fetching policy
    // (device_token_, token_cache_loaded_) has changed.
    virtual void OnDeviceTokenChanged() = 0;

    // Authentication credentials for talking to the device management service
    // (gaia_token_) changed.
    virtual void OnCredentialsChanged() = 0;

    // Called from the destructor of CloudPolicyData.
    virtual void OnDataStoreGoingAway() = 0;
  };

  // Create CloudPolicyData with constants initialized for fetching user
  // policies.
  static CloudPolicyDataStore* CreateForUserPolicies();

  // Create CloudPolicyData with constants initialized for fetching device
  // policies.
  static CloudPolicyDataStore* CreateForDevicePolicies();

  virtual ~CloudPolicyDataStore();

  base::WeakPtr<CloudPolicyDataStore> GetWeakPtr();

  // Sets the device token, and token_policy_cache_loaded and sends out
  // notifications. Also ensures that setting the token should first happen
  // from the cache.
  void SetDeviceToken(const std::string& device_token,
                      bool from_cache);

  // Sets the gaia token and sends out notifications.
  void SetGaiaToken(const std::string& gaia_token);

  // Clears device and user credentials.
  void Reset();

  // Only used in tests.
  void SetupForTesting(const std::string& device_token,
                       const std::string& device_id,
                       const std::string& user_name,
                       const std::string& gaia_token,
                       bool token_cache_loaded);

  void set_device_id(const std::string& device_id);
  void set_user_name(const std::string& user_name);

  std::string device_id() const;
  std::string device_token() const;
  std::string gaia_token() const;
  std::string machine_id() const;
  std::string machine_model() const;
  em::DeviceRegisterRequest_Type policy_register_type() const;
  std::string policy_type() const;
  bool token_cache_loaded() const;
  std::string user_name() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyCredentialsChanged();
  void NotifyDeviceTokenChanged();

 private:
  CloudPolicyDataStore(
      const em::DeviceRegisterRequest_Type policy_register_type,
      const std::string& policy_type,
      const std::string& machine_model,
      const std::string& machine_id);

  // Data necessary for constructing register requests.
  std::string gaia_token_;
  std::string user_name_;

  // Data necessary for constructing policy requests.
  std::string device_token_;

  // Constants that won't change over the life-time of a cloud policy
  // subsystem.
  const em::DeviceRegisterRequest_Type policy_register_type_;
  const std::string policy_type_;
  const std::string machine_model_;
  const std::string machine_id_;

  // Data used for constructiong both register and policy requests.
  std::string device_id_;

  bool token_cache_loaded_;

  ObserverList<Observer, true> observer_list_;

  base::WeakPtrFactory<CloudPolicyDataStore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyDataStore);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_DATA_STORE_H_
