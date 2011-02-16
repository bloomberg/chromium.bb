// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_POLICY_PROVIDER_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/device_token_fetcher.h"

class Profile;
class TokenService;

namespace policy {

class CloudPolicyCache;
class DeviceManagementBackend;

// Provides policy fetched from the device management server. With the exception
// of the Provide method, which can be called on the FILE thread, all public
// methods must be called on the UI thread.
class DeviceManagementPolicyProvider
    :  public ConfigurationPolicyProvider,
       public DeviceManagementBackend::DevicePolicyResponseDelegate,
       public DeviceTokenFetcher::Observer {
 public:
  DeviceManagementPolicyProvider(const PolicyDefinitionList* policy_list,
                                 DeviceManagementBackend* backend,
                                 Profile* profile);

  virtual ~DeviceManagementPolicyProvider();

  // ConfigurationPolicyProvider implementation:
  virtual bool Provide(ConfigurationPolicyStoreInterface* store);
  virtual bool IsInitializationComplete() const;

  // DevicePolicyResponseDelegate implementation:
  virtual void HandlePolicyResponse(
      const em::DevicePolicyResponse& response);  // deprecated.
  virtual void HandleCloudPolicyResponse(
      const em::CloudPolicyResponse& response);
  virtual void OnError(DeviceManagementBackend::ErrorCode code);

  // DeviceTokenFetcher::Observer implementation:
  virtual void OnTokenSuccess();
  virtual void OnTokenError();
  virtual void OnNotManaged();

  // Sets the refresh rate at which to re-fetch policy information.
  void SetRefreshRate(int64 refresh_rate_milliseconds);

 private:
  // Indicates the current state the provider is in.
  enum ProviderState {
    // The provider is initializing, policy information not yet available.
    STATE_INITIALIZING,
    // This device is not managed through policy.
    STATE_UNMANAGED,
    // The token is valid, but policy is yet to be fetched.
    STATE_TOKEN_VALID,
    // Policy information is available and valid.
    STATE_POLICY_VALID,
    // The token was found to be invalid and needs to be obtained again.
    STATE_TOKEN_RESET,
    // There has been an error fetching the token, retry later.
    STATE_TOKEN_ERROR,
    // The service returned an error when requesting policy, ask again later.
    STATE_POLICY_ERROR,
  };

  class RefreshTask;

  friend class DeviceManagementPolicyProviderTest;

  // More configurable constructor for use by test cases.
  DeviceManagementPolicyProvider(const PolicyDefinitionList* policy_list,
                                 DeviceManagementBackend* backend,
                                 Profile* profile,
                                 int64 policy_refresh_rate_ms,
                                 int policy_refresh_deviation_factor_percent,
                                 int64 policy_refresh_deviation_max_ms,
                                 int64 policy_refresh_error_delay_ms,
                                 int64 token_fetch_error_delay_ms,
                                 int64 unmanaged_device_refresh_rate_ms);

  // Called by constructors to perform shared initialization. Initialization
  // requiring the IOThread must not be performed directly in this method,
  // rather must be deferred until the IOThread is fully initialized. This is
  // the case in InitializeAfterIOThreadExists.
  void Initialize(DeviceManagementBackend* backend,
                  Profile* profile,
                  int64 policy_refresh_rate_ms,
                  int policy_refresh_deviation_factor_percent,
                  int64 policy_refresh_deviation_max_ms,
                  int64 policy_refresh_error_delay_ms,
                  int64 token_fetch_error_delay_ms,
                  int64 unmanaged_device_refresh_rate_ms);

  // ConfigurationPolicyProvider overrides:
  virtual void AddObserver(ConfigurationPolicyProvider::Observer* observer);
  virtual void RemoveObserver(ConfigurationPolicyProvider::Observer* observer);

  // Sends a request to the device manager backend to fetch policy if one isn't
  // already outstanding.
  void SendPolicyRequest();

  // Triggers policy refresh, re-requesting device token and policy information
  // as necessary.
  void RefreshTaskExecute();

  // Cancels the refresh task.
  void CancelRefreshTask();

  // Notify observers about a policy update.
  void NotifyCloudPolicyUpdate();

  // The path of the device token file.
  FilePath GetTokenPath();

  // Used only by tests.
  void SetDeviceTokenFetcher(DeviceTokenFetcher* token_fetcher);

  // Switches to a new state and triggers any appropriate actions.
  void SetState(ProviderState new_state);

  // Check whether the current state is one in which the token is available.
  bool TokenAvailable() const;

  // Computes the refresh delay to use.
  int64 GetRefreshDelay();

  // Provides the URL at which requests are sent to from the device management
  // backend.
  static std::string GetDeviceManagementURL();

  // Returns the path to the sub-directory in the user data directory
  // in which device management persistent state is stored.
  static FilePath GetOrCreateDeviceManagementDir(
      const FilePath& user_data_dir);

  scoped_ptr<DeviceManagementBackend> backend_;
  Profile* profile_;  // weak
  scoped_ptr<CloudPolicyCache> cache_;
  bool fallback_to_old_protocol_;
  scoped_refptr<DeviceTokenFetcher> token_fetcher_;
  DeviceTokenFetcher::ObserverRegistrar registrar_;
  ObserverList<ConfigurationPolicyProvider::Observer, true> observer_list_;
  FilePath storage_dir_;
  ProviderState state_;
  bool initial_fetch_done_;
  RefreshTask* refresh_task_;
  int64 policy_refresh_rate_ms_;
  int policy_refresh_deviation_factor_percent_;
  int64 policy_refresh_deviation_max_ms_;
  int64 policy_refresh_error_delay_ms_;
  int64 effective_policy_refresh_error_delay_ms_;
  int64 token_fetch_error_delay_ms_;
  int64 effective_token_fetch_error_delay_ms_;
  int64 unmanaged_device_refresh_rate_ms_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_POLICY_PROVIDER_H_
