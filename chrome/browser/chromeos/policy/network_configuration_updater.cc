// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/network_configuration_updater.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_map.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "chromeos/network/onc/onc_utils.h"
#include "policy/policy_constants.h"

namespace policy {

NetworkConfigurationUpdater::~NetworkConfigurationUpdater() {
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

// static
scoped_ptr<NetworkConfigurationUpdater>
NetworkConfigurationUpdater::CreateForDevicePolicy(
    scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer,
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler) {
  scoped_ptr<NetworkConfigurationUpdater> updater(
      new NetworkConfigurationUpdater(onc::ONC_SOURCE_DEVICE_POLICY,
                                      key::kDeviceOpenNetworkConfiguration,
                                      certificate_importer.Pass(),
                                      policy_service,
                                      network_config_handler));
  updater->Init();
  return updater.Pass();
}

void NetworkConfigurationUpdater::OnPolicyUpdated(const PolicyNamespace& ns,
                                                  const PolicyMap& previous,
                                                  const PolicyMap& current) {
  // Ignore this call. Policy changes are already observed by the registrar.
}

void NetworkConfigurationUpdater::OnPolicyServiceInitialized(
    PolicyDomain domain) {
  if (domain != POLICY_DOMAIN_CHROME)
    return;

  if (policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    VLOG(1) << LogHeader() << " initialized.";
    policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
    ApplyPolicy();
  }
}

NetworkConfigurationUpdater::NetworkConfigurationUpdater(
    onc::ONCSource onc_source,
    std::string policy_key,
    scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer,
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler)
    : onc_source_(onc_source),
      network_config_handler_(network_config_handler),
      certificate_importer_(certificate_importer.Pass()),
      policy_key_(policy_key),
      policy_change_registrar_(policy_service,
                               PolicyNamespace(POLICY_DOMAIN_CHROME,
                                               std::string())),
      policy_service_(policy_service) {
}

void NetworkConfigurationUpdater::Init() {
  policy_change_registrar_.Observe(
      policy_key_,
      base::Bind(&NetworkConfigurationUpdater::OnPolicyChanged,
                 base::Unretained(this)));

  if (policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    VLOG(1) << LogHeader() << " is already initialized.";
    ApplyPolicy();
  } else {
    policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
  }
}

void NetworkConfigurationUpdater::ImportCertificates(
    const base::ListValue& certificates_onc) {
  certificate_importer_->ImportCertificates(
      certificates_onc, onc_source_, NULL);
}

void NetworkConfigurationUpdater::ApplyNetworkPolicy(
    base::ListValue* network_configs_onc) {
  network_config_handler_->SetPolicy(
      onc_source_, std::string() /* no username hash */, *network_configs_onc);
}

void NetworkConfigurationUpdater::OnPolicyChanged(
    const base::Value* previous,
    const base::Value* current) {
  VLOG(1) << LogHeader() << " changed.";
  ApplyPolicy();
}

void NetworkConfigurationUpdater::ApplyPolicy() {
  const PolicyMap& policies = policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  const base::Value* policy_value = policies.GetValue(policy_key_);

  std::string onc_blob;
  if (!policy_value)
    VLOG(2) << LogHeader() << " is not set.";
  else if (!policy_value->GetAsString(&onc_blob))
    LOG(ERROR) << LogHeader() << " is not a string value.";

  base::ListValue network_configs;
  base::ListValue certificates;
  chromeos::onc::ParseAndValidateOncForImport(onc_blob,
                                              onc_source_,
                                              "" /* no passphrase */,
                                              &network_configs,
                                              &certificates);

  ImportCertificates(certificates);
  ApplyNetworkPolicy(&network_configs);
}

std::string NetworkConfigurationUpdater::LogHeader() const {
  return chromeos::onc::GetSourceAsString(onc_source_);
}

}  // namespace policy
