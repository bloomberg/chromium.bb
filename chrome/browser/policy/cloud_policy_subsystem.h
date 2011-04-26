// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_SUBSYSTEM_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_SUBSYSTEM_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/common/notification_observer.h"
#include "net/base/network_change_notifier.h"

class PrefService;

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class CloudPolicyCacheBase;
class CloudPolicyController;
class CloudPolicyIdentityStrategy;
class ConfigurationPolicyProvider;
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

  CloudPolicySubsystem(CloudPolicyIdentityStrategy* identity_strategy,
                       CloudPolicyCacheBase* policy_cache);
  virtual ~CloudPolicySubsystem();

  // net::NetworkChangeNotifier::IPAddressObserver:
  virtual void OnIPAddressChanged() OVERRIDE;

  // Initializes the subsystem.
  void Initialize(PrefService* prefs,
                  net::URLRequestContextGetter* request_context);

  // Shuts the subsystem down. This must be called before threading and network
  // infrastructure goes away.
  void Shutdown();

  // Returns the externally visible state and corresponding error details.
  PolicySubsystemState state();
  ErrorDetails error_details();

  // Stops all auto-retrying error handling behavior inside the policy
  // subsystem.
  void StopAutoRetry();

  ConfigurationPolicyProvider* GetManagedPolicyProvider();
  ConfigurationPolicyProvider* GetRecommendedPolicyProvider();

  // Registers cloud policy related prefs.
  static void RegisterPrefs(PrefService* pref_service);

 private:
  // Updates the policy controller with a new refresh rate value.
  void UpdatePolicyRefreshRate();

  // Returns a weak pointer to this subsystem's PolicyNotifier.
  PolicyNotifier* notifier() {
    return notifier_.get();
  }

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The pref service that controls the refresh rate.
  PrefService* prefs_;

  // Tracks the pref value for the policy refresh rate.
  IntegerPrefMember policy_refresh_rate_;

  // Cloud policy infrastructure stuff.
  scoped_ptr<PolicyNotifier> notifier_;
  scoped_ptr<DeviceManagementService> device_management_service_;
  scoped_ptr<DeviceTokenFetcher> device_token_fetcher_;
  scoped_ptr<CloudPolicyCacheBase> cloud_policy_cache_;
  scoped_ptr<CloudPolicyController> cloud_policy_controller_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicySubsystem);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_SUBSYSTEM_H_
