// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/network_configuration_updater_impl.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_map.h"
#include "chromeos/network/certificate_handler.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_utils.h"
#include "policy/policy_constants.h"

namespace policy {

NetworkConfigurationUpdaterImpl::NetworkConfigurationUpdaterImpl(
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler,
    scoped_ptr<chromeos::CertificateHandler> certificate_handler)
    : policy_change_registrar_(
          policy_service, PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())),
      policy_service_(policy_service),
      network_config_handler_(network_config_handler),
      certificate_handler_(certificate_handler.Pass()) {
  policy_change_registrar_.Observe(
      key::kDeviceOpenNetworkConfiguration,
      base::Bind(&NetworkConfigurationUpdaterImpl::OnPolicyChanged,
                 base::Unretained(this),
                 chromeos::onc::ONC_SOURCE_DEVICE_POLICY));
  policy_change_registrar_.Observe(
      key::kOpenNetworkConfiguration,
      base::Bind(&NetworkConfigurationUpdaterImpl::OnPolicyChanged,
                 base::Unretained(this),
                 chromeos::onc::ONC_SOURCE_USER_POLICY));

  // Apply the current device policies immediately.
  ApplyNetworkConfiguration(chromeos::onc::ONC_SOURCE_DEVICE_POLICY);
}

NetworkConfigurationUpdaterImpl::~NetworkConfigurationUpdaterImpl() {
}

void NetworkConfigurationUpdaterImpl::OnUserPolicyInitialized(
    bool allow_trusted_certs_from_policy,
    const std::string& hashed_username) {
  VLOG(1) << "User policy initialized.";
  if (allow_trusted_certs_from_policy)
    SetAllowTrustedCertsFromPolicy();
  ApplyNetworkConfiguration(chromeos::onc::ONC_SOURCE_USER_POLICY);
}

void NetworkConfigurationUpdaterImpl::OnPolicyChanged(
    chromeos::onc::ONCSource onc_source,
    const base::Value* previous,
    const base::Value* current) {
  VLOG(1) << "Policy for ONC source "
          << chromeos::onc::GetSourceAsString(onc_source) << " changed.";
  ApplyNetworkConfiguration(onc_source);
}

void NetworkConfigurationUpdaterImpl::ApplyNetworkConfiguration(
    chromeos::onc::ONCSource onc_source) {
  VLOG(1) << "Apply policy for ONC source "
          << chromeos::onc::GetSourceAsString(onc_source);

  std::string policy_key;
  if (onc_source == chromeos::onc::ONC_SOURCE_USER_POLICY)
    policy_key = key::kOpenNetworkConfiguration;
  else
    policy_key = key::kDeviceOpenNetworkConfiguration;

  const PolicyMap& policies = policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  const base::Value* policy_value = policies.GetValue(policy_key);

  std::string onc_blob;
  if (policy_value) {
    if (!policy_value->GetAsString(&onc_blob))
      LOG(ERROR) << "ONC policy " << policy_key << " is not a string value.";
  } else {
    VLOG(2) << "The policy is not set.";
  }
  VLOG(2) << "The policy contains this ONC: " << onc_blob;

  base::ListValue network_configs;
  base::ListValue certificates;
  ParseAndValidateOncForImport(
      onc_blob, onc_source, "", &network_configs, &certificates);

  network_config_handler_->SetPolicy(onc_source, network_configs);

  scoped_ptr<net::CertificateList> web_trust_certs(new net::CertificateList);
  certificate_handler_->ImportCertificates(
      certificates, onc_source, web_trust_certs.get());

  if (onc_source == chromeos::onc::ONC_SOURCE_USER_POLICY)
    SetTrustAnchors(web_trust_certs.Pass());
}

}  // namespace policy
