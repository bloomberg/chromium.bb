// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_DATA_STORE_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_DATA_STORE_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/policy/device_status_collector.h"
#endif

namespace policy {

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
  };

  ~CloudPolicyDataStore();

  // Create CloudPolicyData with constants initialized for fetching user
  // policies.
  static CloudPolicyDataStore* CreateForUserPolicies();

  // Create CloudPolicyData with constants initialized for fetching device
  // policies.
  static CloudPolicyDataStore* CreateForDevicePolicies();

  // Sets the device token, and token_policy_cache_loaded and sends out
  // notifications. Also ensures that setting the token should first happen
  // from the cache.
  void SetDeviceToken(const std::string& device_token,
                      bool from_cache);

  // Sets the gaia token and sends out notifications.
  void SetGaiaToken(const std::string& gaia_token);

  // Sets an OAuth token to be used for registration.
  void SetOAuthToken(const std::string& oauth_token);

  // Clears device and user credentials.
  void Reset();

  // Only used in tests.
  void SetupForTesting(const std::string& device_token,
                       const std::string& device_id,
                       const std::string& user_name,
                       const std::string& gaia_token,
                       bool token_cache_loaded);

  void set_device_id(const std::string& device_id);
  void set_machine_id(const std::string& machine_id);
  void set_machine_model(const std::string& machine_model);
  void set_user_name(const std::string& user_name);
  void set_user_affiliation(UserAffiliation user_affiliation);
  void set_known_machine_id(bool known_machine_id);
  void set_policy_fetching_enabled(bool policy_fetching_enabled);
  void set_device_mode(DeviceMode device_mode);

#if defined(OS_CHROMEOS)
  void set_device_status_collector(DeviceStatusCollector* collector);
  DeviceStatusCollector* device_status_collector();
#endif

  const std::string& device_id() const;
  const std::string& device_token() const;
  const std::string& gaia_token() const;
  const std::string& oauth_token() const;
  bool has_auth_token() const;
  const std::string& machine_id() const;
  const std::string& machine_model() const;
  enterprise_management::DeviceRegisterRequest_Type
      policy_register_type() const;
  const std::string& policy_type() const;
  bool token_cache_loaded() const;
  bool policy_fetching_enabled() const;
  const std::string& user_name() const;
  UserAffiliation user_affiliation() const;
  bool known_machine_id() const;
  DeviceMode device_mode() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyCredentialsChanged();
  void NotifyDeviceTokenChanged();

 private:
  CloudPolicyDataStore(
      const enterprise_management::DeviceRegisterRequest_Type register_type,
      const std::string& policy_type);

  // Data necessary for constructing register requests.
  std::string gaia_token_;
  std::string oauth_token_;
  std::string user_name_;

  // Data necessary for constructing policy requests.
  std::string device_token_;
  UserAffiliation user_affiliation_;

  // Constants that won't change over the life-time of a cloud policy
  // subsystem.
  const enterprise_management::DeviceRegisterRequest_Type policy_register_type_;
  const std::string policy_type_;

  // Data used for constructiong both register and policy requests.
  std::string device_id_;
  std::string machine_model_;
  std::string machine_id_;
  bool known_machine_id_;

  bool token_cache_loaded_;
  bool policy_fetching_enabled_;

  DeviceMode device_mode_;

#if defined(OS_CHROMEOS)
  scoped_ptr<DeviceStatusCollector> device_status_collector_;
#endif

  ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyDataStore);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_DATA_STORE_H_
