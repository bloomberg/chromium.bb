// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/configuration_policy_handler_list.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/proxy_policy_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;
class TokenService;

namespace policy {

class AppPackUpdater;
class CloudPolicyDataStore;
class CloudPolicyProvider;
class CloudPolicySubsystem;
class ConfigurationPolicyProvider;
class DeviceManagementService;
class NetworkConfigurationUpdater;
class PolicyService;
class UserCloudPolicyManager;
class UserPolicyTokenCache;

// Manages the lifecycle of browser-global policy infrastructure, such as the
// platform policy providers, device- and the user-cloud policy infrastructure.
// TODO(gfeher,mnissler): Factor out device and user specific methods into their
// respective classes.
class BrowserPolicyConnector : public content::NotificationObserver {
 public:
  // Builds an uninitialized BrowserPolicyConnector, suitable for testing.
  // Init() should be called to create and start the policy machinery.
  BrowserPolicyConnector();
  virtual ~BrowserPolicyConnector();

  // Creates the policy providers and finalizes the initialization of the
  // connector. This call can be skipped on tests that don't require the full
  // policy system running.
  void Init();

  // Creates a UserCloudPolicyManager for the given profile, or returns NULL if
  // it is not supported on this platform. Ownership is transferred to the
  // caller.
  scoped_ptr<UserCloudPolicyManager> CreateCloudPolicyManager(Profile* profile);

  // Creates a new policy service for the given profile, or a global one if
  // it is NULL. Ownership is transferred to the caller.
  scoped_ptr<PolicyService> CreatePolicyService(Profile* profile);

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

  // Triggers registration for device policy, using the |owner_email| account.
  // |token| is an oauth token to authenticate the registration request, and
  // |known_machine_id| is true if the server should do additional checks based
  // on the machine_id used for the request.
  void RegisterForDevicePolicy(const std::string& owner_email,
                               const std::string& token,
                               bool known_machine_id,
                               bool reregister);

  // Returns true if this device is managed by an enterprise (as opposed to
  // a local owner).
  bool IsEnterpriseManaged();

  // Locks the device to an enterprise domain.
  EnterpriseInstallAttributes::LockResult LockDevice(const std::string& user);

  // Returns the device serial number, or an empty string if not available.
  static std::string GetSerialNumber();

  // Returns the enterprise domain if device is managed.
  std::string GetEnterpriseDomain();

  // Returns the device mode. For ChromeOS this function will return the mode
  // stored in the lockbox, or DEVICE_MODE_CONSUMER if the lockbox has been
  // locked empty, or DEVICE_MODE_UNKNOWN if the device has not been owned yet.
  // For other OSes the function will always return DEVICE_MODE_CONSUMER.
  DeviceMode GetDeviceMode();

  // Reset the device policy machinery. This stops any automatic retry behavior
  // and clears the error flags, so potential retries have a chance to succeed.
  void ResetDevicePolicy();

  // Initiates device and user policy fetches, if possible. Pending fetches
  // will be cancelled.
  void FetchCloudPolicy();

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

  // The data stores should be considered read-only for everyone except for
  // tests.
  CloudPolicyDataStore* GetDeviceCloudPolicyDataStore();
  CloudPolicyDataStore* GetUserCloudPolicyDataStore();

  const ConfigurationPolicyHandlerList* GetHandlerList() const;

  // Works out the user affiliation by checking the given |user_name| against
  // the installation attributes.
  UserAffiliation GetUserAffiliation(const std::string& user_name);

  AppPackUpdater* GetAppPackUpdater();

  NetworkConfigurationUpdater* GetNetworkConfigurationUpdater();

  DeviceManagementService* device_management_service() {
    return device_management_service_.get();
  }

  // Sets a |provider| that will be included in PolicyServices returned by
  // CreatePolicyService. This is a static method because local state is
  // created immediately after the connector, and tests don't have a chance to
  // inject the provider otherwise. |provider| must outlive the connector, and
  // its ownership is not taken.
  static void SetPolicyProviderForTesting(
      ConfigurationPolicyProvider* provider);

  // Gets the URL of the DM server (either the default or a URL provided via the
  // command line).
  static std::string GetDeviceManagementUrl();

 private:
  // content::NotificationObserver method overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Initializes the device cloud policy infrasturcture.
  void InitializeDevicePolicy();

  // Complete initialization once the message loops are running and the
  // local_state is initialized.
  void CompleteInitialization();

  // Set the timezone as soon as the policies are available.
  void SetTimezoneIfPolicyAvailable();

  static ConfigurationPolicyProvider* CreatePlatformProvider();

  // Used to convert policies to preferences. The providers declared below
  // trigger policy updates during destruction via OnProviderGoingAway(), which
  // will result in |handler_list_| being consulted for policy translation.
  // Therefore, it's important to destroy |handler_list_| after the providers.
  ConfigurationPolicyHandlerList handler_list_;

  scoped_ptr<ConfigurationPolicyProvider> platform_provider_;

  scoped_ptr<CloudPolicyProvider> managed_cloud_provider_;
  scoped_ptr<CloudPolicyProvider> recommended_cloud_provider_;

#if defined(OS_CHROMEOS)
  scoped_ptr<CloudPolicyDataStore> device_data_store_;
  scoped_ptr<CloudPolicySubsystem> device_cloud_policy_subsystem_;
  scoped_ptr<EnterpriseInstallAttributes> install_attributes_;
#endif

  scoped_ptr<UserPolicyTokenCache> user_policy_token_cache_;
  scoped_ptr<CloudPolicyDataStore> user_data_store_;
  scoped_ptr<CloudPolicySubsystem> user_cloud_policy_subsystem_;

  // Components of the new-style cloud policy implementation.
  // TODO(mnissler): Remove the old-style components above once we have
  // completed the switch to the new cloud policy implementation.
  scoped_ptr<DeviceManagementService> device_management_service_;

  ProxyPolicyProvider user_cloud_policy_provider_;

  // Used to initialize the device policy subsystem once the message loops
  // are spinning.
  base::WeakPtrFactory<BrowserPolicyConnector> weak_ptr_factory_;

  // Registers the provider for notification of successful Gaia logins.
  content::NotificationRegistrar registrar_;

  // Weak reference to the TokenService we are listening to for user cloud
  // policy authentication tokens.
  TokenService* token_service_;

#if defined(OS_CHROMEOS)
  scoped_ptr<AppPackUpdater> app_pack_updater_;
  scoped_ptr<NetworkConfigurationUpdater> network_configuration_updater_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BrowserPolicyConnector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
