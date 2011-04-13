// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_TOKEN_FETCHER_H_
#define CHROME_BROWSER_POLICY_DEVICE_TOKEN_FETCHER_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/task.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/policy_notifier.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace policy {

class CloudPolicyCacheBase;
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
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnDeviceTokenAvailable() = 0;
  };

  // |service| is used to talk to the device management service and |cache| is
  // used to persist whether the device is unmanaged.
  DeviceTokenFetcher(DeviceManagementService* service,
                     CloudPolicyCacheBase* cache,
                     PolicyNotifier* notifier);
  // Version for tests that allows to set timing parameters.
  DeviceTokenFetcher(DeviceManagementService* service,
                     CloudPolicyCacheBase* cache,
                     PolicyNotifier* notifier,
                     int64 token_fetch_error_delay_ms,
                     int64 token_fetch_error_max_delay_ms,
                     int64 unmanaged_device_refresh_rate_ms);
  virtual ~DeviceTokenFetcher();

  // Starts fetching a token.
  // Declared virtual so it can be overridden by mocks.
  virtual void FetchToken(const std::string& auth_token,
                          const std::string& device_id,
                          em::DeviceRegisterRequest_Type policy_type,
                          const std::string& machine_id,
                          const std::string& machine_model);

  virtual void SetUnmanagedState();

  // Returns the device management token or the empty string if not available.
  // Declared virtual so it can be overridden by mocks.
  virtual const std::string& GetDeviceToken();

  // Disables the auto-retry-on-error behavior of this token fetcher.
  void StopAutoRetry();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // DeviceManagementBackend::DeviceRegisterResponseDelegate method overrides:
  virtual void HandleRegisterResponse(
      const em::DeviceRegisterResponse& response);
  virtual void OnError(DeviceManagementBackend::ErrorCode code);

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
                  PolicyNotifier* notifier,
                  int64 token_fetch_error_delay_ms,
                  int64 token_fetch_error_max_delay_ms,
                  int64 unmanaged_device_refresh_rate_ms);

  // Moves the fetcher into a new state.
  void SetState(FetcherState state);

  // Resets |backend_|, then uses |auth_token_| and |device_id_| to perform
  // an actual token fetch.
  void FetchTokenInternal();

  // Called back from the |retry_task_|.
  void ExecuteRetryTask();

  // Cancels the |retry_task_|.
  void CancelRetryTask();

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

  // Current device token.
  std::string device_token_;

  // Contains the AuthToken for the device management server.
  std::string auth_token_;
  // Device identifier to send to the server.
  std::string device_id_;
  // Contains policy type to send to the server.
  em::DeviceRegisterRequest_Type policy_type_;
  // Contains physical machine id to send to the server.
  std::string machine_id_;
  // Contains physical machine model to send to server.
  std::string machine_model_;

  // Task that has been scheduled to retry fetching a token.
  CancelableTask* retry_task_;

  ScopedRunnableMethodFactory<DeviceTokenFetcher> method_factory_;

  ObserverList<Observer, true> observer_list_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_TOKEN_FETCHER_H_
