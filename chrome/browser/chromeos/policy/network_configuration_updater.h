// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_H_

#include <string>

#include "chrome/browser/chromeos/cros/network_constants.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/network_ui_data.h"
#include "chrome/browser/policy/policy_service.h"
#include "chromeos/network/onc/onc_constants.h"

namespace base {
class Value;
}

namespace net {
class CertTrustAnchorProvider;
}

namespace policy {

class PolicyMap;

// Keeps track of the network configuration policy settings and Shill's
// profiles. Requests the NetworkLibrary to apply the ONC of the network
// policies every time one of the relevant policies or Shill's profiles changes
// or OnUserPolicyInitialized() is called. If the user policy is available,
// always both the device and the user policy are applied. Otherwise only the
// device policy is applied.
class NetworkConfigurationUpdater
    : public chromeos::NetworkLibrary::NetworkProfileObserver {
 public:
  NetworkConfigurationUpdater(PolicyService* policy_service,
                              chromeos::NetworkLibrary* network_library);
  virtual ~NetworkConfigurationUpdater();

  // NetworkProfileObserver overrides.
  virtual void OnProfileListChanged() OVERRIDE;

  // Notifies this updater that the user policy is initialized. Before this
  // function is called, the user policy is not applied. Afterwards, always both
  // device and user policy are applied as described in the class comment. This
  // function also triggers an immediate policy application of both device and
  // user policy.
  void OnUserPolicyInitialized();

  // Web trust isn't given to certificates imported from ONC by default. Setting
  // |allow| to true allows giving Web trust to the certificates that
  // request it.
  void set_allow_trusted_certificates_from_policy(bool allow) {
    allow_trusted_certificates_from_policy_ = allow;
  }

  // Returns a CertTrustAnchorProvider that provides the list of server and
  // CA certificates with the Web trust flag set that were retrieved from the
  // last user ONC policy update.
  // This getter must be used on the UI thread, and the provider must be used
  // on the IO thread. It is only valid as long as the
  // NetworkConfigurationUpdater is valid; the NetworkConfigurationUpdater
  // outlives all the profiles, and deletes the provider on the IO thread.
  net::CertTrustAnchorProvider* GetCertTrustAnchorProvider();

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
                                 chromeos::onc::ONCSource onc_source);

  // Wraps the policy service we read network configuration from.
  PolicyChangeRegistrar policy_change_registrar_;

  // Network library to write network configuration to.
  chromeos::NetworkLibrary* network_library_;

  // Whether the user policy is already available.
  bool user_policy_initialized_;

  // Whether Web trust is allowed or not.
  bool allow_trusted_certificates_from_policy_;

  // The policy service storing the ONC policies.
  PolicyService* policy_service_;

  // An implementation of CertTrustAnchorProvider. Owned by the updater, but
  // lives on the IO thread.
  net::CertTrustAnchorProvider* cert_trust_provider_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_H_
