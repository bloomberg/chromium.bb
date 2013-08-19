// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_IMPL_H_

#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "chrome/browser/policy/policy_service.h"
#include "chromeos/network/onc/onc_constants.h"

namespace base {
class Value;
}

namespace chromeos {
class ManagedNetworkConfigurationHandler;

namespace onc {
class CertificateImporter;
}
}

namespace policy {

class PolicyMap;

// This implementation pushes policies to the
// ManagedNetworkConfigurationHandler. User policies are only pushed after
// OnUserPolicyInitialized() was called.
class NetworkConfigurationUpdaterImpl : public NetworkConfigurationUpdater,
                                        public PolicyService::Observer {
 public:
  NetworkConfigurationUpdaterImpl(
      PolicyService* device_policy_service,
      chromeos::ManagedNetworkConfigurationHandler* network_config_handler,
      scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer);
  virtual ~NetworkConfigurationUpdaterImpl();

  // NetworkConfigurationUpdater overrides.
  virtual void SetUserPolicyService(
      bool allow_trusted_certs_from_policy,
      const chromeos::User* user,
      PolicyService* user_policy_service) OVERRIDE;

  virtual void UnsetUserPolicyService() OVERRIDE;

  // PolicyService::Observer overrides for both device and user policies.
  virtual void OnPolicyUpdated(const PolicyNamespace& ns,
                               const PolicyMap& previous,
                               const PolicyMap& current) OVERRIDE;
  virtual void OnPolicyServiceInitialized(PolicyDomain domain) OVERRIDE;

  private:
   // Called if the ONC policy from |onc_source| changed.
   void OnPolicyChanged(chromeos::onc::ONCSource onc_source,
                        const base::Value* previous,
                        const base::Value* current);

   void ApplyNetworkConfiguration(chromeos::onc::ONCSource onc_source);

   // Used to register for notifications from the |user_policy_service_|.
   scoped_ptr<PolicyChangeRegistrar> user_policy_change_registrar_;

   // Used to register for notifications from the |device_policy_service_|.
   PolicyChangeRegistrar device_policy_change_registrar_;

   // Used to retrieve user policies.
   PolicyService* user_policy_service_;

   // Used to retrieve device policies.
   PolicyService* device_policy_service_;

   // The user for whom the user policy will be applied. The User object must be
   // valid until UnsetUserPolicyService() is called.
   const chromeos::User* user_;

   // Pointer to the global singleton or a test instance.
   chromeos::ManagedNetworkConfigurationHandler* network_config_handler_;

   scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer_;

   DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationUpdaterImpl);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_IMPL_H_
