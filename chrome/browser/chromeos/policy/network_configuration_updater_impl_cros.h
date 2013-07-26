// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_IMPL_CROS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_IMPL_CROS_H_

#include <string>

#include "chrome/browser/chromeos/cros/network_constants.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "chrome/browser/policy/policy_service.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_constants.h"

namespace base {
class Value;
}

namespace chromeos {
namespace onc {
class CertificateImporter;
}
}

namespace policy {

class PolicyMap;

// DEPRECATED: will be replaced by NetworkConfigurationImpl.
// This implementation pushes policies through the NetworkLibrary. It applies
// network policies every time one of the relevant policies or Shill's profiles
// changed or OnUserPolicyInitialized() is called. If the user policy is
// available, always both the device and the user policy are applied. Otherwise
// only the device policy is applied.
class NetworkConfigurationUpdaterImplCros
    : public NetworkConfigurationUpdater,
      public chromeos::NetworkLibrary::NetworkProfileObserver,
      public PolicyService::Observer {
 public:
  // The pointer |device_policy_service| is stored. The caller must guarantee
  // that it's outliving the updater.
  NetworkConfigurationUpdaterImplCros(
      PolicyService* device_policy_service,
      chromeos::NetworkLibrary* network_library,
      scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer);
  virtual ~NetworkConfigurationUpdaterImplCros();

  // NetworkProfileObserver overrides.
  virtual void OnProfileListChanged() OVERRIDE;

  // NetworkConfigurationUpdater overrides.

  // In this implementation, this function applies both device and user policy.
  virtual void SetUserPolicyService(
      bool allow_trusted_certs_from_policy,
      const std::string& hashed_username,
      PolicyService* user_policy_service) OVERRIDE;

  virtual void UnsetUserPolicyService() OVERRIDE;

  // PolicyService::Observer overrides for both device and user policies.
  virtual void OnPolicyUpdated(const PolicyNamespace& ns,
                               const PolicyMap& previous,
                               const PolicyMap& current) OVERRIDE;
  virtual void OnPolicyServiceInitialized(PolicyDomain domain) OVERRIDE;

 private:
  // Callback that's called by |policy_service_| if the respective ONC policy
  // changed.
  void OnPolicyChanged(chromeos::onc::ONCSource onc_source,
                       const base::Value* previous,
                       const base::Value* current);

  // Retrieves the ONC policies from |policy_service_| and pushes the
  // configurations to |network_library_|. Ensures that a device policy is
  // always overwritten by a user policy.
  void ApplyNetworkConfigurations();

  // Push the policy stored at |policy_key| for |onc_source| to
  // |network_library_|.
  void ApplyNetworkConfiguration(const std::string& policy_key,
                                 chromeos::onc::ONCSource onc_source,
                                 PolicyService* policy_service);

  // Wraps the policy service we read network configuration from.
  PolicyChangeRegistrar policy_change_registrar_;

  // Network library to write network configuration to.
  chromeos::NetworkLibrary* network_library_;

  scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer_;

  // Needed to check whether user policies are ready.
  // Unowned.
  PolicyService* user_policy_service_;

  // The device policy service storing the ONC policies. Also needed to check
  // whether device policies are ready.
  // Unowned.
  PolicyService* device_policy_service_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationUpdaterImplCros);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_IMPL_CROS_H_
