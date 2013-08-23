// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_NETWORK_CONFIGURATION_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_NETWORK_CONFIGURATION_UPDATER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/policy/network_configuration_updater.h"

namespace chromeos {
class User;
}

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace policy {

class PolicyCertVerifier;
class PolicyService;

// Implements additional special handling of ONC user policies. Namely string
// expansion with the user's name (or email address, etc.) and handling of "Web"
// trust of certificates. Web trusted certificates are pushed to the
// PolicyCertVerifier if set.
class UserNetworkConfigurationUpdater : public NetworkConfigurationUpdater {
 public:
  virtual ~UserNetworkConfigurationUpdater();

  // Creates an updater that applies the ONC user policy from |policy_service|
  // for user |user| once the policy service is completely initialized and on
  // each policy change. Imported certificates, that request it, are only
  // granted Web trust if |allow_trusted_certs_from_policy| is true. A reference
  // to |user| is stored. It must outlive the returned updater.
  static scoped_ptr<UserNetworkConfigurationUpdater> CreateForUserPolicy(
      bool allow_trusted_certs_from_policy,
      const chromeos::User& user,
      scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer,
      PolicyService* policy_service,
      chromeos::ManagedNetworkConfigurationHandler* network_config_handler);

  // Sets the CertVerifier on which the current list of Web trusted server and
  // CA certificates will be set. Policy updates will trigger further calls to
  // |cert_verifier| later. |cert_verifier| must be valid until
  // SetPolicyCertVerifier is called again (with another CertVerifier or NULL)
  // or until this Updater is destructed. |cert_verifier|'s methods are only
  // called on the IO thread. This function must be called on the UI thread.
  void SetPolicyCertVerifier(PolicyCertVerifier* cert_verifier);

  // Sets |certs| to the list of Web trusted server and CA certificates from the
  // last received policy.
  void GetWebTrustedCertificates(net::CertificateList* certs) const;

 private:
  class CrosTrustAnchorProvider;

  UserNetworkConfigurationUpdater(
      bool allow_trusted_certs_from_policy,
      const chromeos::User& user,
      scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer,
      PolicyService* policy_service,
      chromeos::ManagedNetworkConfigurationHandler* network_config_handler);

  virtual void ImportCertificates(
      const base::ListValue& certificates_onc) OVERRIDE;

  virtual void ApplyNetworkPolicy(
      base::ListValue* network_configs_onc) OVERRIDE;

  // Push |web_trust_certs_| to |cert_verifier_| if necessary.
  void SetTrustAnchors();

  // Whether Web trust is allowed or not. Only relevant for user policies.
  bool allow_trusted_certificates_from_policy_;

  // The user for whom the user policy will be applied. Is NULL if this Updater
  // is used for device policy.
  const chromeos::User* user_;

  // Calls to this object are only allowed on the IO Thread.
  PolicyCertVerifier* cert_verifier_;

  // Contains the certificates of the last import that requested web trust. Must
  // be empty if Web trust from policy is not allowed.
  net::CertificateList web_trust_certs_;

  DISALLOW_COPY_AND_ASSIGN(UserNetworkConfigurationUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_NETWORK_CONFIGURATION_UPDATER_H_
