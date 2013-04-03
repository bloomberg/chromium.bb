// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/configuration_policy_handler_list.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/proxy_policy_provider.h"
#endif

class PrefRegistrySimple;
class PrefRegistrySyncable;
class PrefService;
class Profile;

namespace net {
class CertTrustAnchorProvider;
class URLRequestContextGetter;
}

namespace policy {

class ConfigurationPolicyProvider;
class DeviceManagementService;
class PolicyService;
class PolicyStatisticsCollector;

#if defined(OS_CHROMEOS)
class AppPackUpdater;
class DeviceCloudPolicyManagerChromeOS;
class DeviceLocalAccountPolicyProvider;
class DeviceLocalAccountPolicyService;
class EnterpriseInstallAttributes;
class NetworkConfigurationUpdater;
class UserCloudPolicyManagerChromeOS;
#endif

// Manages the lifecycle of browser-global policy infrastructure, such as the
// platform policy providers, device- and the user-cloud policy infrastructure.
class BrowserPolicyConnector {
 public:
  // Builds an uninitialized BrowserPolicyConnector, suitable for testing.
  // Init() should be called to create and start the policy machinery.
  BrowserPolicyConnector();

  // Invoke Shutdown() before deleting, see below.
  virtual ~BrowserPolicyConnector();

  // Finalizes the initialization of the connector. This call can be skipped on
  // tests that don't require the full policy system running.
  void Init(PrefService* local_state,
            scoped_refptr<net::URLRequestContextGetter> request_context);

  // Stops the policy providers and cleans up the connector before it can be
  // safely deleted. This must be invoked before the destructor and while the
  // threads are still running. The policy providers are still valid but won't
  // update anymore after this call.
  void Shutdown();

  // Returns true if Init() has been called but Shutdown() hasn't been yet.
  bool is_initialized() const { return is_initialized_; }

  // Creates a new policy service for the given profile.
  scoped_ptr<PolicyService> CreatePolicyService(Profile* profile);

  // Returns the browser-global PolicyService, that contains policies for the
  // whole browser.
  PolicyService* GetPolicyService();

#if defined(OS_CHROMEOS)
  // Returns true if this device is managed by an enterprise (as opposed to
  // a local owner).
  bool IsEnterpriseManaged();

  // Returns the enterprise domain if device is managed.
  std::string GetEnterpriseDomain();

  // Returns the device mode. For ChromeOS this function will return the mode
  // stored in the lockbox, or DEVICE_MODE_CONSUMER if the lockbox has been
  // locked empty, or DEVICE_MODE_UNKNOWN if the device has not been owned yet.
  // For other OSes the function will always return DEVICE_MODE_CONSUMER.
  DeviceMode GetDeviceMode();
#endif

  // Schedules initialization of the cloud policy backend services, if the
  // services are already constructed.
  void ScheduleServiceInitialization(int64 delay_milliseconds);

#if defined(OS_CHROMEOS)
  // Initializes the user cloud policy infrastructure.
  // If |wait_for_policy_fetch| is true, the user policy will only become fully
  // initialized after a policy fetch is attempted. Note that Profile creation
  // is blocked until this initialization is complete.
  void InitializeUserPolicy(const std::string& user_name,
                            bool is_public_account,
                            bool wait_for_policy_fetch);
#endif

  const ConfigurationPolicyHandlerList* GetHandlerList() const;

  // Works out the user affiliation by checking the given |user_name| against
  // the installation attributes.
  UserAffiliation GetUserAffiliation(const std::string& user_name);

  DeviceManagementService* device_management_service() {
    return device_management_service_.get();
  }

#if defined(OS_CHROMEOS)
  AppPackUpdater* GetAppPackUpdater();

  NetworkConfigurationUpdater* GetNetworkConfigurationUpdater();

  net::CertTrustAnchorProvider* GetCertTrustAnchorProvider();

  DeviceCloudPolicyManagerChromeOS* GetDeviceCloudPolicyManager() {
    return device_cloud_policy_manager_.get();
  }
  UserCloudPolicyManagerChromeOS* GetUserCloudPolicyManager() {
    return user_cloud_policy_manager_.get();
  }
  DeviceLocalAccountPolicyService* GetDeviceLocalAccountPolicyService() {
    return device_local_account_policy_service_.get();
  }
  EnterpriseInstallAttributes* GetInstallAttributes() {
    return install_attributes_.get();
  }
#endif

  // Allows setting a DeviceManagementService (for injecting mocks in
  // unit tests).
  void SetDeviceManagementServiceForTesting(
      scoped_ptr<DeviceManagementService> service);

  // Sets a |provider| that will be included in PolicyServices returned by
  // CreatePolicyService. This is a static method because local state is
  // created immediately after the connector, and tests don't have a chance to
  // inject the provider otherwise. |provider| must outlive the connector, and
  // its ownership is not taken though the connector will initialize and shut it
  // down.
  static void SetPolicyProviderForTesting(
      ConfigurationPolicyProvider* provider);

  // Gets the URL of the DM server (either the default or a URL provided via the
  // command line).
  static std::string GetDeviceManagementUrl();

  // Check whether a user is known to be non-enterprise. Domains such as
  // gmail.com and googlemail.com are known to not be managed. Also returns
  // false if the username is empty.
  static bool IsNonEnterpriseUser(const std::string& username);

  // Returns true if |profile| has used certificates installed via policy
  // to establish a secure connection before. This means that it may have
  // cached content from an untrusted source.
  static bool UsedPolicyCertificates(Profile* profile);

  // Registers refresh rate prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Registers Profile prefs related to policy features.
  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

 private:
  // Set the timezone as soon as the policies are available.
  void SetTimezoneIfPolicyAvailable();

  // Creates a new PolicyService with the shared policy providers and the given
  // |user_cloud_policy_provider| and |managed_mode_policy_provider|, which are
  // optional.
  scoped_ptr<PolicyService> CreatePolicyServiceWithProviders(
      ConfigurationPolicyProvider* user_cloud_policy_provider,
      ConfigurationPolicyProvider* managed_mode_policy_provider);

  static ConfigurationPolicyProvider* CreatePlatformProvider();

  // Whether Init() but not Shutdown() has been invoked.
  bool is_initialized_;

  PrefService* local_state_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // Used to convert policies to preferences. The providers declared below
  // may trigger policy updates during shutdown, which will result in
  // |handler_list_| being consulted for policy translation.
  // Therefore, it's important to destroy |handler_list_| after the providers.
  ConfigurationPolicyHandlerList handler_list_;

  scoped_ptr<ConfigurationPolicyProvider> platform_provider_;

  // Components of the new-style cloud policy implementation.
  // TODO(mnissler): Remove the old-style components below once we have
  // completed the switch to the new cloud policy implementation.
#if defined(OS_CHROMEOS)
  scoped_ptr<EnterpriseInstallAttributes> install_attributes_;
  scoped_ptr<DeviceCloudPolicyManagerChromeOS> device_cloud_policy_manager_;
  scoped_ptr<DeviceLocalAccountPolicyService>
      device_local_account_policy_service_;
  scoped_ptr<DeviceLocalAccountPolicyProvider>
      device_local_account_policy_provider_;
  scoped_ptr<UserCloudPolicyManagerChromeOS> user_cloud_policy_manager_;

  // This policy provider is used on Chrome OS to feed user policy into the
  // global PolicyService instance. This works by installing
  // |user_cloud_policy_manager_| or |device_local_account_policy_provider_|,
  // respectively as the delegate after login.
  ProxyPolicyProvider global_user_cloud_policy_provider_;
#endif

  // Must be deleted before all the policy providers.
  scoped_ptr<PolicyService> policy_service_;

  scoped_ptr<PolicyStatisticsCollector> policy_statistics_collector_;

  scoped_ptr<DeviceManagementService> device_management_service_;

  // Used to initialize the device policy subsystem once the message loops
  // are spinning.
  base::WeakPtrFactory<BrowserPolicyConnector> weak_ptr_factory_;

#if defined(OS_CHROMEOS)
  scoped_ptr<AppPackUpdater> app_pack_updater_;
  scoped_ptr<NetworkConfigurationUpdater> network_configuration_updater_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BrowserPolicyConnector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
