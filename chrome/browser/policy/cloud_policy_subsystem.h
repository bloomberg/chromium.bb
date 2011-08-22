// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_SUBSYSTEM_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_SUBSYSTEM_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/common/notification_observer.h"
#include "net/base/network_change_notifier.h"

class PrefService;

namespace policy {

class CloudPolicyCacheBase;
class CloudPolicyController;
class CloudPolicyDataStore;
class DeviceManagementService;
class DeviceTokenFetcher;
class PolicyNotifier;

// This class is a container for the infrastructure required to support cloud
// policy. It glues together the backend, the policy controller and manages the
// life cycle of the policy providers.
class CloudPolicySubsystem
    : public NotificationObserver,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  enum PolicySubsystemState {
    UNENROLLED,     // No enrollment attempt has been performed yet.
    BAD_GAIA_TOKEN, // The server rejected the GAIA auth token.
    UNMANAGED,      // This device is unmanaged.
    NETWORK_ERROR,  // A network error occurred, retrying makes sense.
    LOCAL_ERROR,    // Retrying is futile.
    TOKEN_FETCHED,  // Device has been successfully registered.
    SUCCESS         // Policy has been fetched successfully and is in effect.
  };

  enum ErrorDetails {
    NO_DETAILS,            // No error, so no error details either.
    DMTOKEN_NETWORK_ERROR, // DeviceTokenFetcher encountered a network error.
    POLICY_NETWORK_ERROR,  // CloudPolicyController encountered a network error.
    BAD_DMTOKEN,           // The server rejected the DMToken.
    POLICY_LOCAL_ERROR,    // The policy cache encountered a local error.
    SIGNATURE_MISMATCH,    // The policy cache detected a signature mismatch.
  };

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnPolicyStateChanged(PolicySubsystemState state,
                                      ErrorDetails error_details) = 0;
  };

  class ObserverRegistrar {
   public:
    ObserverRegistrar(CloudPolicySubsystem* cloud_policy_subsystem,
                      CloudPolicySubsystem::Observer* observer);
    ~ObserverRegistrar();

   private:
    PolicyNotifier* policy_notifier_;
    CloudPolicySubsystem::Observer* observer_;
    DISALLOW_COPY_AND_ASSIGN(ObserverRegistrar);
  };

  CloudPolicySubsystem(CloudPolicyDataStore* data_store,
                       CloudPolicyCacheBase* policy_cache);
  virtual ~CloudPolicySubsystem();

  // net::NetworkChangeNotifier::IPAddressObserver:
  virtual void OnIPAddressChanged() OVERRIDE;

  // Initializes the subsystem. The first network request will only be made
  // after |delay_milliseconds|. It can be scheduled to be happen earlier by
  // calling |ScheduleInitialization|.
  void CompleteInitialization(const char* refresh_pref_name,
                              int64 delay_milliseconds);

  // Shuts the subsystem down. This must be called before threading and network
  // infrastructure goes away.
  void Shutdown();

  // Returns the externally visible state and corresponding error details.
  PolicySubsystemState state();
  ErrorDetails error_details();

  // Resets the subsystem back to unenrolled state and cancels any pending
  // retry operations.
  void Reset();

  // Registers cloud policy related prefs.
  static void RegisterPrefs(PrefService* pref_service);

  // Schedule initialization of the policy backend service.
  void ScheduleServiceInitialization(int64 delay_milliseconds);

  // Only used in testing.
  CloudPolicyCacheBase* GetCloudPolicyCacheBase() const;

 private:
  friend class TestingCloudPolicySubsystem;

  CloudPolicySubsystem();

  void Initialize(CloudPolicyDataStore* data_store,
                  CloudPolicyCacheBase* policy_cache,
                  const std::string& device_management_url);

  // Updates the policy controller with a new refresh rate value.
  void UpdatePolicyRefreshRate(int64 refresh_rate);

  // Returns a weak pointer to this subsystem's PolicyNotifier.
  PolicyNotifier* notifier() {
    return notifier_.get();
  }

  // Factory methods that may be overridden in tests.
  virtual void CreateDeviceTokenFetcher();
  virtual void CreateCloudPolicyController();

  // NotificationObserver overrides.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Name of the preference to read the refresh rate from.
  const char* refresh_pref_name_;

  PrefChangeRegistrar pref_change_registrar_;

  CloudPolicyDataStore* data_store_;

  // Cloud policy infrastructure stuff.
  scoped_ptr<PolicyNotifier> notifier_;
  scoped_ptr<DeviceManagementService> device_management_service_;
  scoped_ptr<DeviceTokenFetcher> device_token_fetcher_;
  scoped_ptr<CloudPolicyCacheBase> cloud_policy_cache_;
  scoped_ptr<CloudPolicyController> cloud_policy_controller_;

  std::string device_management_url_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicySubsystem);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_SUBSYSTEM_H_
