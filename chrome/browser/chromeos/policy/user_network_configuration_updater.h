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
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace base {
class ListValue;
}

namespace user_manager {
class User;
}

namespace chromeos {

namespace onc {
class CertificateImporter;
}
}

namespace net {
class NSSCertDatabase;
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
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
      Profile* profile,
      bool allow_trusted_certs_from_policy,
      const user_manager::User& user,
      PolicyService* policy_service,
      chromeos::ManagedNetworkConfigurationHandler* network_config_handler);

  void AddTrustedCertsObserver(WebTrustedCertsObserver* observer);
  void RemoveTrustedCertsObserver(WebTrustedCertsObserver* observer);

  // Sets |certs| to the list of Web trusted server and CA certificates from the
  // last received policy.
  void GetWebTrustedCertificates(net::CertificateList* certs) const;

  // Helper method to expose |SetCertificateImporter| for usage in tests.
  void SetCertificateImporterForTest(
      scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer);

 private:
  class CrosTrustAnchorProvider;

  UserNetworkConfigurationUpdater(
      Profile* profile,
      bool allow_trusted_certs_from_policy,
      const user_manager::User& user,
      PolicyService* policy_service,
      chromeos::ManagedNetworkConfigurationHandler* network_config_handler);

  // NetworkConfigurationUpdater:
  virtual void ImportCertificates(
      const base::ListValue& certificates_onc) OVERRIDE;
  virtual void ApplyNetworkPolicy(
      base::ListValue* network_configs_onc,
      base::DictionaryValue* global_network_config) OVERRIDE;

  // content::NotificationObserver implementation. Observes the profile to which
  // |this| belongs to for PROFILE_ADDED notification.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Creates onc::CertImporter with |database| and passes it to
  // |SetCertificateImporter|.
  void CreateAndSetCertificateImporter(net::NSSCertDatabase* database);

  // Sets the certificate importer that should be used to import certificate
  // policies. If there is |pending_certificates_onc_|, it gets imported.
  void SetCertificateImporter(
      scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer);

  void NotifyTrustAnchorsChanged();

  // Whether Web trust is allowed or not.
  bool allow_trusted_certificates_from_policy_;

  // The user for whom the user policy will be applied.
  const user_manager::User* user_;

  ObserverList<WebTrustedCertsObserver, true> observer_list_;

  // Contains the certificates of the last import that requested web trust. Must
  // be empty if Web trust from policy is not allowed.
  net::CertificateList web_trust_certs_;

  // If |ImportCertificates| is called before |SetCertificateImporter|, gets set
  // to a copy of the policy for which the import was requested.
  // The policy will be processed when the certificate importer is set.
  scoped_ptr<base::ListValue> pending_certificates_onc_;

  // Certificate importer to be used for importing policy defined certificates.
  // Set by |SetCertificateImporter|.
  scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<UserNetworkConfigurationUpdater> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserNetworkConfigurationUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_NETWORK_CONFIGURATION_UPDATER_H_
