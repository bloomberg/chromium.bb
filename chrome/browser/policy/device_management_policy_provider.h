// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_POLICY_PROVIDER_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"

namespace policy {

class DeviceManagementBackend;
class DeviceManagementPolicyCache;
class DeviceTokenFetcher;

// Provides policy fetched from the device management server. With the exception
// of the Provide method, which can be called on the FILE thread, all public
// methods must be called on the UI thread.
class DeviceManagementPolicyProvider
    :  public ConfigurationPolicyProvider,
       public NotificationObserver,
       public DeviceManagementBackend::DevicePolicyResponseDelegate,
       public base::SupportsWeakPtr<DeviceManagementPolicyProvider> {
 public:
  explicit DeviceManagementPolicyProvider(
      const PolicyDefinitionList* policy_list);
  virtual ~DeviceManagementPolicyProvider();

  // ConfigurationPolicyProvider implementation:
  virtual bool Provide(ConfigurationPolicyStoreInterface* store);

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // DevicePolicyResponseDelegate implementation:
  virtual void HandlePolicyResponse(
      const em::DevicePolicyResponse& response);
  virtual void OnError(DeviceManagementBackend::ErrorCode code);

  // True if a policy request has been sent to the device management backend
  // server and no response or error has yet been received.
  bool IsPolicyRequestPending() const { return policy_request_pending_; }

 protected:
  friend class DeviceManagementPolicyProviderTest;

  // Called only by unit tests, this constructor allows the caller to specify in
  // |backend| the device management backend to which policy requests are
  // sent. The provider takes over ownership of |backend|.  The directory
  // specified in |storage_dir| is used to store the device token and persisted
  // policy, rather than the user's data directory.
  DeviceManagementPolicyProvider(
      const PolicyDefinitionList* policy_list,
      DeviceManagementBackend* backend,
      const FilePath& storage_dir);

 private:
  class InitializeAfterIOThreadExistsTask;

  // Returns the device management backend to use for backend requests, lazily
  // creating a new one if one doesn't already exist.
  DeviceManagementBackend* GetBackend();

  // Called by constructors to perform shared initialization. Initialization
  // requiring the IOThread must not be performed directly in this method,
  // rather must be deferred until the IOThread is fully initialized. This is
  // the case in InitializeAfterIOThreadExists.
  void Initialize();

  // Called by a deferred task posted to the UI thread to complete the portion
  // of initialization that requires the IOThread.
  void InitializeAfterIOThreadExists();

  // Sends a request to the device manager backend to fetch policy if one isn't
  // already outstanding.
  void SendPolicyRequest();

  // True if policy must be re-fetched because the cached policy is too old or
  // its time stamp is invalid.
  bool IsPolicyStale() const;

  // Provides the URL at which requests are sent to from the device management
  // backend.
  static std::string GetDeviceManagementURL();

  // Returns the path to the sub-directory in the user data directory
  // in which device management persistent state is stored.
  static FilePath GetOrCreateDeviceManagementDir(
      const FilePath& user_data_dir);

  scoped_ptr<DeviceManagementBackend> backend_;
  scoped_ptr<DeviceManagementPolicyCache> cache_;
  scoped_refptr<DeviceTokenFetcher> token_fetcher_;
  NotificationRegistrar registrar_;
  FilePath storage_dir_;
  bool policy_request_pending_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_POLICY_PROVIDER_H_
