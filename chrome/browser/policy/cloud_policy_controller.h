// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_CONTROLLER_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_CONTROLLER_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/policy/cloud_policy_identity_strategy.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/device_token_fetcher.h"

class Profile;
class TokenService;

namespace policy {

class CloudPolicyCacheBase;
class DeviceManagementBackend;

// Coordinates the actions of DeviceTokenFetcher, CloudPolicyIdentityStrategy,
// DeviceManagementBackend, and CloudPolicyCache: calls their methods and
// listens to their callbacks/notifications.
class CloudPolicyController
    : public DeviceManagementBackend::DevicePolicyResponseDelegate,
      public DeviceTokenFetcher::Observer,
      public CloudPolicyIdentityStrategy::Observer {
 public:
  // Takes ownership of |backend|; the other parameters are weak pointers.
  CloudPolicyController(DeviceManagementService* service,
                        CloudPolicyCacheBase* cache,
                        DeviceTokenFetcher* token_fetcher,
                        CloudPolicyIdentityStrategy* identity_strategy,
                        PolicyNotifier* notifier);
  virtual ~CloudPolicyController();

  // Sets the refresh rate at which to re-fetch policy information.
  void SetRefreshRate(int64 refresh_rate_milliseconds);

  // Triggers an immediate retry of of the current operation.
  void Retry();

  // Stops all auto-retrying error handling behavior inside the policy
  // subsystem.
  void StopAutoRetry();

  // DevicePolicyResponseDelegate implementation:
  virtual void HandlePolicyResponse(
      const em::DevicePolicyResponse& response);
  virtual void OnError(DeviceManagementBackend::ErrorCode code);

  // DeviceTokenFetcher::Observer implementation:
  virtual void OnDeviceTokenAvailable();

  // CloudPolicyIdentityStrategy::Observer implementation:
  virtual void OnDeviceTokenChanged();
  virtual void OnCredentialsChanged();

 private:
  // Indicates the current state the controller is in.
  enum ControllerState {
    // The controller is initializing, policy information not yet available.
    STATE_TOKEN_UNAVAILABLE,
    // The device is not managed. Should retry fetching the token after delay.
    STATE_TOKEN_UNMANAGED,
    // The token is not valid and should be refetched with exponential back-off.
    STATE_TOKEN_ERROR,
    // The token is valid, but policy is yet to be fetched.
    STATE_TOKEN_VALID,
    // Policy information is available and valid.
    STATE_POLICY_VALID,
    // The service returned an error when requesting policy, will retry.
    STATE_POLICY_ERROR,
    // The service returned an error that is not going to go away soon.
    STATE_POLICY_UNAVAILABLE
  };

  friend class CloudPolicyControllerTest;

  // More configurable constructor for use by test cases.
  CloudPolicyController(DeviceManagementService* service,
                        CloudPolicyCacheBase* cache,
                        DeviceTokenFetcher* token_fetcher,
                        CloudPolicyIdentityStrategy* identity_strategy,
                        PolicyNotifier* notifier,
                        int64 policy_refresh_rate_ms,
                        int policy_refresh_deviation_factor_percent,
                        int64 policy_refresh_deviation_max_ms,
                        int64 policy_refresh_error_delay_ms);

  // Called by constructors to perform shared initialization.
  void Initialize(DeviceManagementService* service,
                  CloudPolicyCacheBase* cache,
                  DeviceTokenFetcher* token_fetcher,
                  CloudPolicyIdentityStrategy* identity_strategy,
                  PolicyNotifier* notifier,
                  int64 policy_refresh_rate_ms,
                  int policy_refresh_deviation_factor_percent,
                  int64 policy_refresh_deviation_max_ms,
                  int64 policy_refresh_error_delay_ms);

  // Asks the token fetcher to fetch a new token.
  void FetchToken();

  // Sends a request to the device management backend to fetch policy if one
  // isn't already outstanding.
  void SendPolicyRequest();

  // Called back from the delayed work task. Calls |DoWork()|.
  void DoDelayedWork();

  // Performs whatever action is required in the current state,
  // e.g. refreshing policy.
  void DoWork();

  // Cancels the delayed work task.
  void CancelDelayedWork();

  // Switches to a new state and triggers any appropriate actions.
  void SetState(ControllerState new_state);

  // Computes the policy refresh delay to use.
  int64 GetRefreshDelay();

  DeviceManagementService* service_;
  CloudPolicyCacheBase* cache_;
  CloudPolicyIdentityStrategy* identity_strategy_;
  DeviceTokenFetcher* token_fetcher_;
  scoped_ptr<DeviceManagementBackend> backend_;
  ControllerState state_;
  PolicyNotifier* notifier_;

  int64 policy_refresh_rate_ms_;
  int policy_refresh_deviation_factor_percent_;
  int64 policy_refresh_deviation_max_ms_;
  int64 policy_refresh_error_delay_ms_;
  int64 effective_policy_refresh_error_delay_ms_;

  CancelableTask* delayed_work_task_;
  ScopedRunnableMethodFactory<CloudPolicyController> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyController);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_CONTROLLER_H_
