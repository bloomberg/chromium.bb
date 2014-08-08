// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_BROWSER_POLICY_CONNECTOR_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_BROWSER_POLICY_CONNECTOR_CHROMEOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

class PrefRegistrySimple;
class PrefService;

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class AppPackUpdater;
class ConsumerManagementService;
class DeviceCloudPolicyInitializer;
class DeviceCloudPolicyInvalidator;
class DeviceCloudPolicyManagerChromeOS;
class DeviceLocalAccountPolicyService;
class DeviceManagementService;
class EnterpriseInstallAttributes;
class NetworkConfigurationUpdater;
class ProxyPolicyProvider;
class ServerBackedStateKeysBroker;

// Extends ChromeBrowserPolicyConnector with the setup specific to ChromeOS.
class BrowserPolicyConnectorChromeOS : public ChromeBrowserPolicyConnector {
 public:
  BrowserPolicyConnectorChromeOS();

  virtual ~BrowserPolicyConnectorChromeOS();

  virtual void Init(
      PrefService* local_state,
      scoped_refptr<net::URLRequestContextGetter> request_context) OVERRIDE;

  // Destroys the |device_cloud_policy_invalidator_|. This cannot wait until
  // Shutdown() because that method is only called during
  // BrowserProcessImpl::StartTearDown() but the invalidator may be observing
  // the global DeviceOAuth2TokenService that is destroyed earlier by
  // ChromeBrowserMainPartsChromeos::PostMainMessageLoopRun().
  void ShutdownInvalidator();

  virtual void Shutdown() OVERRIDE;

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

  // Works out the user affiliation by checking the given |user_name| against
  // the installation attributes.
  UserAffiliation GetUserAffiliation(const std::string& user_name);

  AppPackUpdater* GetAppPackUpdater();

  DeviceCloudPolicyManagerChromeOS* GetDeviceCloudPolicyManager() {
    return device_cloud_policy_manager_;
  }

  DeviceCloudPolicyInitializer* GetDeviceCloudPolicyInitializer() {
    return device_cloud_policy_initializer_.get();
  }

  DeviceLocalAccountPolicyService* GetDeviceLocalAccountPolicyService() {
    return device_local_account_policy_service_.get();
  }

  EnterpriseInstallAttributes* GetInstallAttributes() {
    return install_attributes_.get();
  }

  ServerBackedStateKeysBroker* GetStateKeysBroker() {
    return state_keys_broker_.get();
  }

  // The browser-global PolicyService is created before Profiles are ready, to
  // provide managed values for the local state PrefService. It includes a
  // policy provider that forwards policies from a delegate policy provider.
  // This call can be used to set the user policy provider as that delegate
  // once the Profile is ready, so that user policies can also affect local
  // state preferences.
  // Only one user policy provider can be set as a delegate at a time, and any
  // previously set delegate is removed. Passing NULL removes the current
  // delegate, if there is one.
  void SetUserPolicyDelegate(ConfigurationPolicyProvider* user_policy_provider);

  // Returns the device management service for consumer management.
  DeviceManagementService* GetDeviceManagementServiceForConsumer() const {
    return consumer_device_management_service_.get();
  }

  ConsumerManagementService* GetConsumerManagementService() const {
    return consumer_management_service_.get();
  }

  // Sets the install attributes for testing. Must be called before the browser
  // is created. RemoveInstallAttributesForTesting must be called after the test
  // to free the attributes.
  static void SetInstallAttributesForTesting(
      EnterpriseInstallAttributes* attributes);
  static void RemoveInstallAttributesForTesting();

  // Registers device refresh rate pref.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Set the timezone as soon as the policies are available.
  void SetTimezoneIfPolicyAvailable();

  void OnDeviceCloudPolicyManagerConnected();

  // Components of the device cloud policy implementation.
  scoped_ptr<ServerBackedStateKeysBroker> state_keys_broker_;
  scoped_ptr<EnterpriseInstallAttributes> install_attributes_;
  DeviceCloudPolicyManagerChromeOS* device_cloud_policy_manager_;
  scoped_ptr<DeviceCloudPolicyInitializer> device_cloud_policy_initializer_;
  scoped_ptr<DeviceLocalAccountPolicyService>
      device_local_account_policy_service_;
  scoped_ptr<DeviceCloudPolicyInvalidator> device_cloud_policy_invalidator_;

  // This policy provider is used on Chrome OS to feed user policy into the
  // global PolicyService instance. This works by installing the cloud policy
  // provider of the primary profile as the delegate of the ProxyPolicyProvider,
  // after login.
  // The provider is owned by the base class; this field is just a typed weak
  // pointer to get to the ProxyPolicyProvider at SetUserPolicyDelegate().
  ProxyPolicyProvider* global_user_cloud_policy_provider_;

  scoped_ptr<AppPackUpdater> app_pack_updater_;
  scoped_ptr<NetworkConfigurationUpdater> network_configuration_updater_;

  scoped_ptr<DeviceManagementService> consumer_device_management_service_;
  scoped_ptr<ConsumerManagementService> consumer_management_service_;

  base::WeakPtrFactory<BrowserPolicyConnectorChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPolicyConnectorChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_BROWSER_POLICY_CONNECTOR_CHROMEOS_H_
