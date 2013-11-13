// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/net/onc_utils.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"
#include "policy/policy_constants.h"

namespace policy {

UserNetworkConfigurationUpdater::~UserNetworkConfigurationUpdater() {}

// static
scoped_ptr<UserNetworkConfigurationUpdater>
UserNetworkConfigurationUpdater::CreateForUserPolicy(
    bool allow_trusted_certs_from_policy,
    const chromeos::User& user,
    scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer,
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler) {
  scoped_ptr<UserNetworkConfigurationUpdater> updater(
      new UserNetworkConfigurationUpdater(allow_trusted_certs_from_policy,
                                          user,
                                          certificate_importer.Pass(),
                                          policy_service,
                                          network_config_handler));
  updater->Init();
  return updater.Pass();
}

void UserNetworkConfigurationUpdater::AddTrustedCertsObserver(
    WebTrustedCertsObserver* observer) {
  observer_list_.AddObserver(observer);
}

void UserNetworkConfigurationUpdater::RemoveTrustedCertsObserver(
    WebTrustedCertsObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

UserNetworkConfigurationUpdater::UserNetworkConfigurationUpdater(
    bool allow_trusted_certs_from_policy,
    const chromeos::User& user,
    scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer,
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler)
    : NetworkConfigurationUpdater(onc::ONC_SOURCE_USER_POLICY,
                                  key::kOpenNetworkConfiguration,
                                  certificate_importer.Pass(),
                                  policy_service,
                                  network_config_handler),
      allow_trusted_certificates_from_policy_(allow_trusted_certs_from_policy),
      user_(&user) {}

void UserNetworkConfigurationUpdater::GetWebTrustedCertificates(
    net::CertificateList* certs) const {
  *certs = web_trust_certs_;
}

void UserNetworkConfigurationUpdater::ImportCertificates(
    const base::ListValue& certificates_onc) {
  web_trust_certs_.clear();
  certificate_importer_->ImportCertificates(
      certificates_onc,
      onc_source_,
      allow_trusted_certificates_from_policy_ ? &web_trust_certs_ : NULL);

  NotifyTrustAnchorsChanged();
}

void UserNetworkConfigurationUpdater::ApplyNetworkPolicy(
    base::ListValue* network_configs_onc,
    base::DictionaryValue* global_network_config) {
  DCHECK(user_);
  chromeos::onc::ExpandStringPlaceholdersInNetworksForUser(user_,
                                                           network_configs_onc);
  network_config_handler_->SetPolicy(onc_source_,
                                     user_->username_hash(),
                                     *network_configs_onc,
                                     *global_network_config);
}

void UserNetworkConfigurationUpdater::NotifyTrustAnchorsChanged() {
  FOR_EACH_OBSERVER(WebTrustedCertsObserver,
                    observer_list_,
                    OnTrustAnchorsChanged(web_trust_certs_));
}

}  // namespace policy
