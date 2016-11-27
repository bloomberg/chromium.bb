// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_BROWSER_POLICY_CONNECTOR_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_BROWSER_POLICY_CONNECTOR_CHROMEOS_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/users/affiliation.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

class InstallAttributes;

namespace attestation {
class AttestationFlow;
}
}

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class AffiliatedCloudPolicyInvalidator;
class AffiliatedInvalidationServiceProvider;
class AffiliatedRemoteCommandsInvalidator;
class BluetoothPolicyHandler;
class DeviceActiveDirectoryPolicyManager;
class DeviceCloudPolicyInitializer;
class DeviceLocalAccountPolicyService;
struct EnrollmentConfig;
class NetworkConfigurationUpdater;
class ProxyPolicyProvider;
class ServerBackedStateKeysBroker;

// Extends ChromeBrowserPolicyConnector with the setup specific to Chrome OS.
class BrowserPolicyConnectorChromeOS
    : public ChromeBrowserPolicyConnector,
      public DeviceCloudPolicyManagerChromeOS::Observer {
 public:
  BrowserPolicyConnectorChromeOS();

  ~BrowserPolicyConnectorChromeOS() override;

  void Init(
      PrefService* local_state,
      scoped_refptr<net::URLRequestContextGetter> request_context) override;

  // Shutdown() is called from BrowserProcessImpl::StartTearDown() but |this|
  // observes some objects that get destroyed earlier. PreShutdown() is called
  // from ChromeBrowserMainPartsChromeos::PostMainMessageLoopRun(), allowing the
  // connection to these dependencies to be severed earlier.
  void PreShutdown();

  void Shutdown() override;

  // Checks whether this devices is under any kind of enterprise management.
  bool IsEnterpriseManaged() const;

  // Checks whether this is a cloud (DM server) managed enterprise device.
  bool IsCloudManaged() const;

  // Checks whether this is an Active Directory managed enterprise device.
  bool IsActiveDirectoryManaged() const;

  // Returns the enterprise domain if device is managed.
  std::string GetEnterpriseDomain() const;

  // Returns the Kerberos realm (aka Windows Domain) if the device is managed by
  // Active Directory.
  std::string GetRealm() const;

  // Returns the device asset ID if it is set.
  std::string GetDeviceAssetID() const;

  // Returns the cloud directory API ID or an empty string if it is not set.
  std::string GetDirectoryApiID() const;

  // Returns the device mode. For Chrome OS this function will return the mode
  // stored in the lockbox, or DEVICE_MODE_CONSUMER if the lockbox has been
  // locked empty, or DEVICE_MODE_UNKNOWN if the device has not been owned yet.
  // For other OSes the function will always return DEVICE_MODE_CONSUMER.
  DeviceMode GetDeviceMode() const;

  // Get the enrollment configuration for the device as decided by various
  // factors. See DeviceCloudPolicyInitializer::GetPrescribedEnrollmentConfig()
  // for details.
  EnrollmentConfig GetPrescribedEnrollmentConfig() const;

  // May be nullptr, e.g. for devices managed by Active Directory.
  DeviceCloudPolicyManagerChromeOS* GetDeviceCloudPolicyManager() const {
    return device_cloud_policy_manager_;
  }

  // May be nullptr, e.g. for cloud-managed devices.
  DeviceActiveDirectoryPolicyManager* GetDeviceActiveDirectoryPolicyManager()
      const {
    return device_active_directory_policy_manager_;
  }

  // May be nullptr, e.g. for devices managed by Active Directory.
  DeviceCloudPolicyInitializer* GetDeviceCloudPolicyInitializer() const {
    return device_cloud_policy_initializer_.get();
  }

  // May be nullptr, e.g. for devices managed by Active Directory.
  DeviceLocalAccountPolicyService* GetDeviceLocalAccountPolicyService() const {
    return device_local_account_policy_service_.get();
  }

  chromeos::InstallAttributes* GetInstallAttributes() const {
    return install_attributes_.get();
  }

  // May be nullptr, e.g. for devices managed by Active Directory.
  ServerBackedStateKeysBroker* GetStateKeysBroker() const {
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

  // Sets the device cloud policy initializer for testing.
  void SetDeviceCloudPolicyInitializerForTesting(
      std::unique_ptr<DeviceCloudPolicyInitializer> initializer);

  // Sets the install attributes for testing. Must be called before the browser
  // is created. RemoveInstallAttributesForTesting must be called after the test
  // to free the attributes.
  static void SetInstallAttributesForTesting(
      chromeos::InstallAttributes* attributes);
  static void RemoveInstallAttributesForTesting();

  // Registers device refresh rate pref.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // DeviceCloudPolicyManagerChromeOS::Observer:
  void OnDeviceCloudPolicyManagerConnected() override;
  void OnDeviceCloudPolicyManagerDisconnected() override;

  chromeos::AffiliationIDSet GetDeviceAffiliationIDs() const;

 private:
  // Set the timezone as soon as the policies are available.
  void SetTimezoneIfPolicyAvailable();

  // Restarts the device cloud policy initializer, because the device's
  // registration status changed from registered to unregistered.
  void RestartDeviceCloudPolicyInitializer();

  // Creates an attestation flow using our async method handler and
  // cryptohome client.
  std::unique_ptr<chromeos::attestation::AttestationFlow>
  CreateAttestationFlow();

  // Components of the device cloud policy implementation.
  std::unique_ptr<ServerBackedStateKeysBroker> state_keys_broker_;
  std::unique_ptr<chromeos::InstallAttributes> install_attributes_;
  std::unique_ptr<AffiliatedInvalidationServiceProvider>
      affiliated_invalidation_service_provider_;
  DeviceCloudPolicyManagerChromeOS* device_cloud_policy_manager_ = nullptr;
  DeviceActiveDirectoryPolicyManager* device_active_directory_policy_manager_ =
      nullptr;
  PrefService* local_state_ = nullptr;
  std::unique_ptr<DeviceCloudPolicyInitializer>
      device_cloud_policy_initializer_;
  std::unique_ptr<DeviceLocalAccountPolicyService>
      device_local_account_policy_service_;
  std::unique_ptr<AffiliatedCloudPolicyInvalidator>
      device_cloud_policy_invalidator_;
  std::unique_ptr<AffiliatedRemoteCommandsInvalidator>
      device_remote_commands_invalidator_;

  std::unique_ptr<BluetoothPolicyHandler> bluetooth_policy_handler_;

  // This policy provider is used on Chrome OS to feed user policy into the
  // global PolicyService instance. This works by installing the cloud policy
  // provider of the primary profile as the delegate of the ProxyPolicyProvider,
  // after login.
  // The provider is owned by the base class; this field is just a typed weak
  // pointer to get to the ProxyPolicyProvider at SetUserPolicyDelegate().
  ProxyPolicyProvider* global_user_cloud_policy_provider_ = nullptr;

  std::unique_ptr<NetworkConfigurationUpdater> network_configuration_updater_;

  base::WeakPtrFactory<BrowserPolicyConnectorChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPolicyConnectorChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_BROWSER_POLICY_CONNECTOR_CHROMEOS_H_
