// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_TOKEN_FETCHER_H_
#define CHROME_BROWSER_POLICY_DEVICE_TOKEN_FETCHER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/delayed_work_scheduler.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/policy_notifier.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace policy {

class CloudPolicyCacheBase;
class CloudPolicyDataStore;
class DeviceManagementService;

namespace em = enterprise_management;

// Fetches the device token that can be used for policy requests with the device
// management server, either from disk if it already has been successfully
// requested, otherwise from the device management server. An instance of the
// fetcher is shared as a singleton by all users of the device management token
// to ensure they all get the same token.
class DeviceTokenFetcher
    : public DeviceManagementBackend::DeviceRegisterResponseDelegate {
 public:
  // |service| is used to talk to the device management service and |cache| is
  // used to persist whether the device is unmanaged.
  DeviceTokenFetcher(DeviceManagementService* service,
                     CloudPolicyCacheBase* cache,
                     CloudPolicyDataStore* data_store,
                     PolicyNotifier* notifier);
  // Version for tests that allows to set timing parameters.
  // Takes ownership of |scheduler|.
  DeviceTokenFetcher(DeviceManagementService* service,
                     CloudPolicyCacheBase* cache,
                     CloudPolicyDataStore* data_store,
                     PolicyNotifier* notifier,
                     DelayedWorkScheduler* scheduler);
  virtual ~DeviceTokenFetcher();

  // Starts fetching a token.
  // Declared virtual so it can be overridden by mocks.
  virtual void FetchToken();

  virtual void SetUnmanagedState();
  virtual void SetSerialNumberInvalidState();

  // Cancels any pending work on this fetcher and resets it to inactive state.
  void Reset();

  // DeviceManagementBackend::DeviceRegisterResponseDelegate method overrides:
  virtual void HandleRegisterResponse(
      const em::DeviceRegisterResponse& response) OVERRIDE;
  virtual void OnError(DeviceManagementBackend::ErrorCode code) OVERRIDE;

 private:
  friend class DeviceTokenFetcherTest;

  // The different states that the fetcher can be in during the process of
  // getting the device token. |state_| is initialized to INACTIVE, depending
  // on the result of a token fetching attempt can transition to either of
  // TOKEN_AVAILABLE, UNMANAGED, or ERROR. The first attempt must be triggered
  // externally. When |state_| is UNMANAGED, a new fetching attempt is
  // performed every |unmanaged_device_refresh_rate_ms_|; when it's ERROR,
  // a new attempt is done after |effective_token_fetch_error_delay_ms_|.
  enum FetcherState {
    // Fetcher inactive.
    STATE_INACTIVE,
    // Token available.
    STATE_TOKEN_AVAILABLE,
    // Device unmanaged.
    STATE_UNMANAGED,
    // The device is not enlisted for the domain.
    STATE_BAD_SERIAL,
    // Error, retry later.
    STATE_ERROR,
    // Temporary error. Retry sooner.
    STATE_TEMPORARY_ERROR,
    // Server rejected the auth token.
    STATE_BAD_AUTH
  };

  // Common initialization helper.
  void Initialize(DeviceManagementService* service,
                  CloudPolicyCacheBase* cache,
                  CloudPolicyDataStore* data,
                  PolicyNotifier* notifier,
                  DelayedWorkScheduler* scheduler);

  // Moves the fetcher into a new state.
  void SetState(FetcherState state);

  // Resets |backend_|, then uses |auth_token_| and |device_id_| to perform
  // an actual token fetch.
  void FetchTokenInternal();

  // DelayedWorkScheduler::Client:
  virtual void DoWork();

  // Service and backend. A new backend is created whenever the fetcher gets
  // reset.
  DeviceManagementService* service_;  // weak
  scoped_ptr<DeviceManagementBackend> backend_;

  // Reference to the cache. Used to persist and read unmanaged state.
  CloudPolicyCacheBase* cache_;

  PolicyNotifier* notifier_;

  // Refresh parameters.
  int64 token_fetch_error_delay_ms_;
  int64 token_fetch_error_max_delay_ms_;
  int64 effective_token_fetch_error_delay_ms_;
  int64 unmanaged_device_refresh_rate_ms_;

  // State the fetcher is currently in.
  FetcherState state_;

  CloudPolicyDataStore* data_store_;

  scoped_ptr<DelayedWorkScheduler> scheduler_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_TOKEN_FETCHER_H_
