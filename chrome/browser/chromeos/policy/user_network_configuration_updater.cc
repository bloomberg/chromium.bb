// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/network/onc/onc_parsed_certificates.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"

using chromeos::onc::OncParsedCertificates;

namespace policy {

UserNetworkConfigurationUpdater::~UserNetworkConfigurationUpdater() {}

// static
std::unique_ptr<UserNetworkConfigurationUpdater>
UserNetworkConfigurationUpdater::CreateForUserPolicy(
    Profile* profile,
    bool allow_trusted_certs_from_policy,
    const user_manager::User& user,
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler) {
  std::unique_ptr<UserNetworkConfigurationUpdater> updater(
      new UserNetworkConfigurationUpdater(
          profile, allow_trusted_certs_from_policy, user, policy_service,
          network_config_handler));
  updater->Init();
  return updater;
}

void UserNetworkConfigurationUpdater::SetClientCertificateImporterForTest(
    std::unique_ptr<chromeos::onc::CertificateImporter>
        client_certificate_importer) {
  SetClientCertificateImporter(std::move(client_certificate_importer));
}

void UserNetworkConfigurationUpdater::AddPolicyProvidedCertsObserver(
    PolicyCertificateProvider::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void UserNetworkConfigurationUpdater::RemovePolicyProvidedCertsObserver(
    PolicyCertificateProvider::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

UserNetworkConfigurationUpdater::UserNetworkConfigurationUpdater(
    Profile* profile,
    bool allow_trusted_certs_from_policy,
    const user_manager::User& user,
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler)
    : NetworkConfigurationUpdater(onc::ONC_SOURCE_USER_POLICY,
                                  key::kOpenNetworkConfiguration,
                                  policy_service,
                                  network_config_handler),
      allow_trusted_certificates_from_policy_(allow_trusted_certs_from_policy),
      user_(&user),
      certs_(std::make_unique<OncParsedCertificates>()),
      weak_factory_(this) {
  // The updater is created with |client_certificate_importer_| unset and is
  // responsible for creating it. This requires |GetNSSCertDatabaseForProfile|
  // call, which is not safe before the profile initialization is finalized.
  // Thus, listen for PROFILE_ADDED notification, on which |cert_importer_|
  // creation should start.
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_ADDED,
                 content::Source<Profile>(profile));
}

net::CertificateList
UserNetworkConfigurationUpdater::GetAllServerAndAuthorityCertificates() const {
  return GetServerAndAuthorityCertificates(base::BindRepeating(
      [](const OncParsedCertificates::ServerOrAuthorityCertificate& cert) {
        return true;
      }));
}

net::CertificateList
UserNetworkConfigurationUpdater::GetWebTrustedCertificates() const {
  if (!allow_trusted_certificates_from_policy_)
    return net::CertificateList();

  return GetServerAndAuthorityCertificates(base::BindRepeating(
      [](const OncParsedCertificates::ServerOrAuthorityCertificate& cert) {
        return cert.web_trust_requested();
      }));
}

net::CertificateList
UserNetworkConfigurationUpdater::GetCertificatesWithoutWebTrust() const {
  if (!allow_trusted_certificates_from_policy_)
    return GetAllServerAndAuthorityCertificates();

  return GetServerAndAuthorityCertificates(base::BindRepeating(
      [](const OncParsedCertificates::ServerOrAuthorityCertificate& cert) {
        return !cert.web_trust_requested();
      }));
}

void UserNetworkConfigurationUpdater::ImportCertificates(
    const base::ListValue& certificates_onc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<OncParsedCertificates> incoming_certs =
      std::make_unique<OncParsedCertificates>(certificates_onc);

  bool server_or_authority_certs_changed =
      certs_->server_or_authority_certificates() !=
      incoming_certs->server_or_authority_certificates();
  bool client_certs_changed =
      certs_->client_certificates() != incoming_certs->client_certificates();

  if (!server_or_authority_certs_changed && !client_certs_changed)
    return;

  certs_ = std::move(incoming_certs);

  if (server_or_authority_certs_changed)
    NotifyPolicyProvidedCertsChanged();

  // If certificate importer is not yet set, the import of client certificates
  // will be re-triggered in SetClientCertificateImporter.
  if (client_certificate_importer_ && client_certs_changed) {
    client_certificate_importer_->ImportClientCertificates(
        certs_->client_certificates(), base::DoNothing());
  }
}

void UserNetworkConfigurationUpdater::ApplyNetworkPolicy(
    base::ListValue* network_configs_onc,
    base::DictionaryValue* global_network_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(user_);
  chromeos::onc::ExpandStringPlaceholdersInNetworksForUser(user_,
                                                           network_configs_onc);

  // Call on UserSessionManager to send the user's password to session manager
  // if the password substitution variable exists in the ONC.
  bool send_password =
      chromeos::onc::HasUserPasswordSubsitutionVariable(network_configs_onc);
  chromeos::UserSessionManager::GetInstance()->OnUserNetworkPolicyParsed(
      send_password);

  network_config_handler_->SetPolicy(onc_source_,
                                     user_->username_hash(),
                                     *network_configs_onc,
                                     *global_network_config);
}

void UserNetworkConfigurationUpdater::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_PROFILE_ADDED);
  Profile* profile = content::Source<Profile>(source).ptr();

  GetNSSCertDatabaseForProfile(
      profile, base::AdaptCallbackForRepeating(
                   base::BindOnce(&UserNetworkConfigurationUpdater::
                                      CreateAndSetClientCertificateImporter,
                                  weak_factory_.GetWeakPtr())));
}

void UserNetworkConfigurationUpdater::CreateAndSetClientCertificateImporter(
    net::NSSCertDatabase* database) {
  DCHECK(database);
  SetClientCertificateImporter(
      std::make_unique<chromeos::onc::CertificateImporterImpl>(
          base::CreateSingleThreadTaskRunnerWithTraits(
              {content::BrowserThread::IO}),
          database));
}

void UserNetworkConfigurationUpdater::SetClientCertificateImporter(
    std::unique_ptr<chromeos::onc::CertificateImporter>
        client_certificate_importer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool initial_client_certificate_importer =
      client_certificate_importer_ == nullptr;
  client_certificate_importer_ = std::move(client_certificate_importer);

  if (initial_client_certificate_importer &&
      !certs_->client_certificates().empty()) {
    client_certificate_importer_->ImportClientCertificates(
        certs_->client_certificates(), base::DoNothing());
  }
}

void UserNetworkConfigurationUpdater::NotifyPolicyProvidedCertsChanged() {
  net::CertificateList all_server_and_authority_certs =
      GetAllServerAndAuthorityCertificates();
  net::CertificateList trust_anchors = GetWebTrustedCertificates();

  for (auto& observer : observer_list_) {
    observer.OnPolicyProvidedCertsChanged(all_server_and_authority_certs,
                                          trust_anchors);
  }
}

net::CertificateList
UserNetworkConfigurationUpdater::GetServerAndAuthorityCertificates(
    ServerOrAuthorityCertPredicate predicate) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net::CertificateList certificates;

  for (const OncParsedCertificates::ServerOrAuthorityCertificate&
           server_or_authority_cert :
       certs_->server_or_authority_certificates()) {
    if (predicate.Run(server_or_authority_cert))
      certificates.push_back(server_or_authority_cert.certificate());
  }

  return certificates;
}
}  // namespace policy
