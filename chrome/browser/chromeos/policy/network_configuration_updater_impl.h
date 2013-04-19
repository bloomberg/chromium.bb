// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_IMPL_H_

#include <string>

#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "chrome/browser/policy/policy_service.h"
#include "chromeos/network/onc/onc_constants.h"

namespace base {
class Value;
}

namespace policy {

class PolicyMap;

// This implementation pushes policies to the
// ManagedNetworkConfigurationHandler. User policies are only pushed after
// OnUserPolicyInitialized() was called.
// TODO(pneubeck): Certificates are not implemented yet, they are silently
// ignored.
class NetworkConfigurationUpdaterImpl : public NetworkConfigurationUpdater {
 public:
  explicit NetworkConfigurationUpdaterImpl(PolicyService* policy_service);
  virtual ~NetworkConfigurationUpdaterImpl();

  // NetworkConfigurationUpdater overrides.

  virtual void OnUserPolicyInitialized() OVERRIDE;
  virtual void set_allow_trusted_certificates_from_policy(bool allow) OVERRIDE {
  }
  virtual net::CertTrustAnchorProvider* GetCertTrustAnchorProvider() OVERRIDE;

 private:
  // Callback that's called by |policy_service_| if the respective ONC policy
  // changed.
  void OnPolicyChanged(chromeos::onc::ONCSource onc_source,
                       const base::Value* previous,
                       const base::Value* current);

  void ApplyNetworkConfiguration(chromeos::onc::ONCSource onc_source);

  // Whether the user policy is already available.
  bool user_policy_initialized_;

  // Wraps the policy service we read network configuration from.
  PolicyChangeRegistrar policy_change_registrar_;

  // The policy service storing the ONC policies.
  PolicyService* policy_service_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationUpdaterImpl);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_IMPL_H_
