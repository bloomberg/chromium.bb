// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_NETWORK_CONFIGURATION_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_NETWORK_CONFIGURATION_UPDATER_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "chromeos/network/onc/onc_parsed_certificates.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/cert/scoped_nss_types.h"

class Profile;

namespace base {
class ListValue;
}

namespace user_manager {
class User;
}

namespace net {
class NSSCertDatabase;
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;
}  // namespace net

namespace chromeos {

namespace onc {
class CertificateImporter;
}
}

namespace policy {

class PolicyService;

// Implements additional special handling of ONC user policies. Namely string
// expansion with the user's name (or email address, etc.) and handling of "Web"
// trust of certificates.
class UserNetworkConfigurationUpdater : public NetworkConfigurationUpdater,
                                        public KeyedService,
                                        public content::NotificationObserver {
 public:
  class PolicyProvidedCertsObserver {
   public:
    // Is called every time the list of policy-set server and authority
    // certificates changes.
    virtual void OnPolicyProvidedCertsChanged(
        const net::CertificateList& all_server_and_authority_certs,
        const net::CertificateList& trust_anchors) = 0;
  };

  ~UserNetworkConfigurationUpdater() override;

  // Creates an updater that applies the ONC user policy from |policy_service|
  // for user |user| once the policy service is completely initialized and on
  // each policy change. Server and authority certificates that request Web
  // trust are are only granted Web trust if |allow_trusted_certs_from_policy|
  // is true. A reference to |user| is stored. It must outlive the returned
  // updater.
  static std::unique_ptr<UserNetworkConfigurationUpdater> CreateForUserPolicy(
      Profile* profile,
      bool allow_trusted_certs_from_policy,
      const user_manager::User& user,
      PolicyService* policy_service,
      chromeos::ManagedNetworkConfigurationHandler* network_config_handler);

  void AddPolicyProvidedCertsObserver(PolicyProvidedCertsObserver* observer);
  void RemovePolicyProvidedCertsObserver(PolicyProvidedCertsObserver* observer);

  // Returns all server and authority certificates successfully parsed from ONC,
  // independent of their trust bits.
  net::CertificateList GetAllServerAndAuthorityCertificates() const;

  // Returns the server and authority certificates which were successfully
  // parsed from ONC and were granted web trust. This means that the
  // certificates had the "Web" trust bit set, and this
  // UserNetworkConfigurationUpdater instance was created with
  // |allow_trusted_certs_from_policy| = true.
  net::CertificateList GetWebTrustedCertificates() const;

  // Helper method to expose |SetClientCertificateImporter| for usage in tests.
  // Note that the CertificateImporter is only used for importing client
  // certificates.
  void SetClientCertificateImporterForTest(
      std::unique_ptr<chromeos::onc::CertificateImporter> certificate_importer);

 private:
  class CrosTrustAnchorProvider;

  UserNetworkConfigurationUpdater(
      Profile* profile,
      bool allow_trusted_certs_from_policy,
      const user_manager::User& user,
      PolicyService* policy_service,
      chromeos::ManagedNetworkConfigurationHandler* network_config_handler);

  // NetworkConfigurationUpdater:
  void ImportCertificates(const base::ListValue& certificates_onc) override;
  void ApplyNetworkPolicy(
      base::ListValue* network_configs_onc,
      base::DictionaryValue* global_network_config) override;

  // content::NotificationObserver implementation. Observes the profile to which
  // |this| belongs to for PROFILE_ADDED notification.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Creates onc::CertImporter with |database| and passes it to
  // |SetClientCertificateImporter|.
  void CreateAndSetClientCertificateImporter(net::NSSCertDatabase* database);

  // Sets the certificate importer that should be used to import certificate
  // policies. If there is |pending_certificates_onc_|, it gets imported.
  void SetClientCertificateImporter(
      std::unique_ptr<chromeos::onc::CertificateImporter> certificate_importer);

  void NotifyPolicyProvidedCertsChanged();

  // Whether Web trust is allowed or not.
  bool allow_trusted_certificates_from_policy_;

  // The user for whom the user policy will be applied.
  const user_manager::User* user_;

  base::ObserverList<PolicyProvidedCertsObserver, true>::Unchecked
      observer_list_;

  // Holds certificates from the last parsed ONC policy.
  std::unique_ptr<chromeos::onc::OncParsedCertificates> certs_;

  // Certificate importer to be used for importing policy defined client
  // certificates. Set by |SetClientCertificateImporter|.
  std::unique_ptr<chromeos::onc::CertificateImporter>
      client_certificate_importer_;

  content::NotificationRegistrar registrar_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<UserNetworkConfigurationUpdater> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserNetworkConfigurationUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_NETWORK_CONFIGURATION_UPDATER_H_
