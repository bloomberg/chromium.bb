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
#include "base/observer_list.h"
#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

namespace chromeos {
class User;
}

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace policy {

class PolicyService;

// Implements additional special handling of ONC user policies. Namely string
// expansion with the user's name (or email address, etc.) and handling of "Web"
// trust of certificates.
class UserNetworkConfigurationUpdater : public NetworkConfigurationUpdater,
                                        public BrowserContextKeyedService {
 public:
  class WebTrustedCertsObserver {
   public:
    // Is called everytime the list of imported certificates with Web trust is
    // changed.
    virtual void OnTrustAnchorsChanged(
        const net::CertificateList& trust_anchors) = 0;
  };

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

  void AddTrustedCertsObserver(WebTrustedCertsObserver* observer);
  void RemoveTrustedCertsObserver(WebTrustedCertsObserver* observer);

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
      base::ListValue* network_configs_onc,
      base::DictionaryValue* global_network_config) OVERRIDE;

  void NotifyTrustAnchorsChanged();

  // Whether Web trust is allowed or not. Only relevant for user policies.
  bool allow_trusted_certificates_from_policy_;

  // The user for whom the user policy will be applied. Is NULL if this Updater
  // is used for device policy.
  const chromeos::User* user_;

  ObserverList<WebTrustedCertsObserver, true> observer_list_;

  // Contains the certificates of the last import that requested web trust. Must
  // be empty if Web trust from policy is not allowed.
  net::CertificateList web_trust_certs_;

  DISALLOW_COPY_AND_ASSIGN(UserNetworkConfigurationUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_NETWORK_CONFIGURATION_UPDATER_H_
