// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/configuration_policy_handler_list.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class TestingBrowserProcess;
class TokenService;

namespace policy {

class CloudPolicyProvider;
class CloudPolicySubsystem;
class ConfigurationPolicyProvider;
class NetworkConfigurationUpdater;
class UserPolicyTokenCache;

// Manages the lifecycle of browser-global policy infrastructure, such as the
// platform policy providers, device- and the user-cloud policy infrastructure.
// TODO(gfeher,mnissler): Factor out device and user specific methods into their
// respective classes.
class BrowserPolicyConnector : public content::NotificationObserver {
 public:
  // Indicates the type of token passed to SetDeviceCredentials.
  enum TokenType {
    TOKEN_TYPE_GAIA,  // A gaia service token.
    TOKEN_TYPE_OAUTH, // An OAuth v2 access token.
  };

  // Builds an uninitialized BrowserPolicyConnector, suitable for testing.
  // Init() should be called to create and start the policy machinery.
  BrowserPolicyConnector();
  virtual ~BrowserPolicyConnector();

  // Creates the policy providers and finalizes the initialization of the
  // connector. This call can be skipped on tests that don't require the full
  // policy system running.
  void Init();

  ConfigurationPolicyProvider* GetManagedPlatformProvider() const;
  ConfigurationPolicyProvider* GetManagedCloudProvider() const;
  ConfigurationPolicyProvider* GetRecommendedPlatformProvider() const;
  ConfigurationPolicyProvider* GetRecommendedCloudProvider() const;

  // Returns a weak pointer to the CloudPolicySubsystem corresponding to the
  // device policy managed by this policy connector, or NULL if no such
  // subsystem exists (i.e. when running outside ChromeOS).
  CloudPolicySubsystem* device_cloud_policy_subsystem() {
#if defined(OS_CHROMEOS)
    return device_cloud_policy_subsystem_.get();
#else
    return NULL;
#endif
  }

  // Returns a weak pointer to the CloudPolicySubsystem corresponding to the
  // user policy managed by this policy connector, or NULL if no such
  // subsystem exists (i.e. when user cloud policy is not active due to
  // unmanaged or not logged in).
  CloudPolicySubsystem* user_cloud_policy_subsystem() {
    return user_cloud_policy_subsystem_.get();
  }

  // Triggers registration for device policy.
  void RegisterForDevicePolicy(const std::string& owner_email,
                               const std::string& token,
                               TokenType token_type);

  // Returns true if this device is managed by an enterprise (as opposed to
  // a local owner).
  bool IsEnterpriseManaged();

  // Locks the device to an enterprise domain.
  EnterpriseInstallAttributes::LockResult LockDevice(const std::string& user);

  // Returns the enterprise domain if device is managed.
  std::string GetEnterpriseDomain();

  // Reset the device policy machinery. This stops any automatic retry behavior
  // and clears the error flags, so potential retries have a chance to succeed.
  void ResetDevicePolicy();

  // Initiates device and user policy fetches, if possible. Pending fetches
  // will be cancelled.
  void FetchCloudPolicy();

  // Refreshes policies on each existing provider.
  void RefreshPolicies();

  // Schedules initialization of the cloud policy backend services, if the
  // services are already constructed.
  void ScheduleServiceInitialization(int64 delay_milliseconds);

  // Initializes the user cloud policy infrastructure.
  // If |wait_for_policy_fetch| is true, the user policy will only become fully
  // initialized after a policy fetch is attempted. Note that Profile creation
  // is blocked until this initialization is complete.
  void InitializeUserPolicy(const std::string& user_name,
                            bool wait_for_policy_fetch);

  // Installs a token service for user policy.
  void SetUserPolicyTokenService(TokenService* token_service);

  // Registers for user policy (if not already registered), using the passed
  // OAuth V2 token for authentication. |oauth_token| can be empty to signal
  // that an attempt to fetch the token was made but failed, or that oauth
  // isn't being used.
  void RegisterForUserPolicy(const std::string& oauth_token);

  const CloudPolicyDataStore* GetDeviceCloudPolicyDataStore() const;
  const CloudPolicyDataStore* GetUserCloudPolicyDataStore() const;

  const ConfigurationPolicyHandlerList* GetHandlerList() const;

  // Works out the user affiliation by checking the given |user_name| against
  // the installation attributes.
  policy::CloudPolicyDataStore::UserAffiliation GetUserAffiliation(
      const std::string& user_name);

 private:
  // content::NotificationObserver method overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Initializes the device cloud policy infrasturcture.
  void InitializeDevicePolicy();

  // Activates the device cloud policy subsystem. This will be posted as a task
  // from InitializeDevicePolicy since it needs to wait for the message loops to
  // be running.
  void InitializeDevicePolicySubsystem();

  static ConfigurationPolicyProvider* CreateManagedPlatformProvider();
  static ConfigurationPolicyProvider* CreateRecommendedPlatformProvider();

  scoped_ptr<ConfigurationPolicyProvider> managed_platform_provider_;
  scoped_ptr<ConfigurationPolicyProvider> recommended_platform_provider_;

  scoped_ptr<CloudPolicyProvider> managed_cloud_provider_;
  scoped_ptr<CloudPolicyProvider> recommended_cloud_provider_;

#if defined(OS_CHROMEOS)
  scoped_ptr<CloudPolicyDataStore> device_data_store_;
  scoped_ptr<CloudPolicySubsystem> device_cloud_policy_subsystem_;
  scoped_ptr<EnterpriseInstallAttributes> install_attributes_;

  scoped_ptr<NetworkConfigurationUpdater> network_configuration_updater_;
#endif

  scoped_ptr<UserPolicyTokenCache> user_policy_token_cache_;
  scoped_ptr<CloudPolicyDataStore> user_data_store_;
  scoped_ptr<CloudPolicySubsystem> user_cloud_policy_subsystem_;

  // Used to initialize the device policy subsystem once the message loops
  // are spinning.
  base::WeakPtrFactory<BrowserPolicyConnector> weak_ptr_factory_;

  // Registers the provider for notification of successful Gaia logins.
  content::NotificationRegistrar registrar_;

  // Weak reference to the TokenService we are listening to for user cloud
  // policy authentication tokens.
  TokenService* token_service_;

  // Used to convert policies to preferences.
  ConfigurationPolicyHandlerList handler_list_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPolicyConnector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
