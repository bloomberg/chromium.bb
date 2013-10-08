// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/policy_service.h"
#include "components/onc/onc_constants.h"

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

// Implements the common part of tracking a OpenNetworkConfiguration device or
// user policy. Pushes the network configs to the
// ManagedNetworkConfigurationHandler, which in turn writes configurations to
// Shill. Certificates are imported with the chromeos::onc::CertificateImporter.
// For user policies the subclass UserNetworkConfigurationUpdater must be used.
// Does not handle proxy settings.
class NetworkConfigurationUpdater : public PolicyService::Observer {
 public:
  virtual ~NetworkConfigurationUpdater();

  // Creates an updater that applies the ONC device policy from |policy_service|
  // once the policy service is completely initialized and on each policy
  // change.
  static scoped_ptr<NetworkConfigurationUpdater> CreateForDevicePolicy(
      scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer,
      PolicyService* policy_service,
      chromeos::ManagedNetworkConfigurationHandler* network_config_handler);

  // PolicyService::Observer overrides
  virtual void OnPolicyUpdated(const PolicyNamespace& ns,
                               const PolicyMap& previous,
                               const PolicyMap& current) OVERRIDE;
  virtual void OnPolicyServiceInitialized(PolicyDomain domain) OVERRIDE;

 protected:
  NetworkConfigurationUpdater(
      onc::ONCSource onc_source,
      std::string policy_key,
      scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer,
      PolicyService* policy_service,
      chromeos::ManagedNetworkConfigurationHandler* network_config_handler);

  void Init();

  // Imports the certificates part of the policy.
  virtual void ImportCertificates(const base::ListValue& certificates_onc);

  // Pushes the network part of the policy to the
  // ManagedNetworkConfigurationHandler. This can be overridden by subclasses to
  // modify |network_configs_onc| before the actual application.
  virtual void ApplyNetworkPolicy(base::ListValue* network_configs_onc);

  onc::ONCSource onc_source_;

  // Pointer to the global singleton or a test instance.
  chromeos::ManagedNetworkConfigurationHandler* network_config_handler_;

  scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer_;

 private:
  // Called if the ONC policy changed.
  void OnPolicyChanged(const base::Value* previous, const base::Value* current);

  // Apply the observed policy, i.e. both networks and certificates.
  void ApplyPolicy();

  std::string LogHeader() const;

  std::string policy_key_;

  // Used to register for notifications from the |policy_service_|.
  PolicyChangeRegistrar policy_change_registrar_;

  // Used to retrieve the policies.
  PolicyService* policy_service_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_H_
