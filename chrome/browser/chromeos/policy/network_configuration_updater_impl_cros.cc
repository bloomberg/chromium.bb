// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/network_configuration_updater_impl_cros.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/policy/policy_map.h"
#include "chromeos/network/certificate_handler.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_utils.h"
#include "policy/policy_constants.h"

namespace policy {

NetworkConfigurationUpdaterImplCros::NetworkConfigurationUpdaterImplCros(
    PolicyService* policy_service,
    chromeos::NetworkLibrary* network_library,
    scoped_ptr<chromeos::CertificateHandler> certificate_handler)
    : policy_change_registrar_(
          policy_service, PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())),
      network_library_(network_library),
      certificate_handler_(certificate_handler.Pass()),
      user_policy_initialized_(false),
      policy_service_(policy_service) {
  DCHECK(network_library_);
  policy_change_registrar_.Observe(
      key::kDeviceOpenNetworkConfiguration,
      base::Bind(&NetworkConfigurationUpdaterImplCros::OnPolicyChanged,
                 base::Unretained(this),
                 chromeos::onc::ONC_SOURCE_DEVICE_POLICY));
  policy_change_registrar_.Observe(
      key::kOpenNetworkConfiguration,
      base::Bind(&NetworkConfigurationUpdaterImplCros::OnPolicyChanged,
                 base::Unretained(this),
                 chromeos::onc::ONC_SOURCE_USER_POLICY));

  network_library_->AddNetworkProfileObserver(this);

  // Apply the current policies immediately.
  ApplyNetworkConfigurations();
}

NetworkConfigurationUpdaterImplCros::~NetworkConfigurationUpdaterImplCros() {
  network_library_->RemoveNetworkProfileObserver(this);
}

void NetworkConfigurationUpdaterImplCros::OnProfileListChanged() {
  VLOG(1) << "Network profile list changed, applying policies.";
  ApplyNetworkConfigurations();
}

void NetworkConfigurationUpdaterImplCros::OnUserPolicyInitialized(
    bool allow_trust_certs_from_policy,
    const std::string& hashed_username) {
  VLOG(1) << "User policy initialized, applying policies.";
  user_policy_initialized_ = true;
  if (allow_trust_certs_from_policy)
    SetAllowTrustedCertsFromPolicy();
  ApplyNetworkConfigurations();
}

void NetworkConfigurationUpdaterImplCros::OnPolicyChanged(
    chromeos::onc::ONCSource onc_source,
    const base::Value* previous,
    const base::Value* current) {
  VLOG(1) << "Policy for ONC source "
          << chromeos::onc::GetSourceAsString(onc_source) << " changed.";
  ApplyNetworkConfigurations();
}

void NetworkConfigurationUpdaterImplCros::ApplyNetworkConfigurations() {
  ApplyNetworkConfiguration(key::kDeviceOpenNetworkConfiguration,
                            chromeos::onc::ONC_SOURCE_DEVICE_POLICY);
  if (user_policy_initialized_) {
    ApplyNetworkConfiguration(key::kOpenNetworkConfiguration,
                              chromeos::onc::ONC_SOURCE_USER_POLICY);
  }
}

void NetworkConfigurationUpdaterImplCros::ApplyNetworkConfiguration(
    const std::string& policy_key,
    chromeos::onc::ONCSource onc_source) {
  VLOG(1) << "Apply policy for ONC source "
          << chromeos::onc::GetSourceAsString(onc_source);
  const PolicyMap& policies = policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  const base::Value* policy_value = policies.GetValue(policy_key);

  std::string onc_blob;
  if (policy_value != NULL) {
    // If the policy is not a string, we issue a warning, but still clear the
    // network configuration.
    if (!policy_value->GetAsString(&onc_blob)) {
      LOG(WARNING) << "ONC policy for source "
                   << chromeos::onc::GetSourceAsString(onc_source)
                   << " is not a string value.";
    }
  }

  base::ListValue network_configs;
  base::ListValue certificates;
  ParseAndValidateOncForImport(
      onc_blob, onc_source, "", &network_configs, &certificates);

  network_library_->LoadOncNetworks(network_configs, onc_source);

  scoped_ptr<net::CertificateList> web_trust_certs(new net::CertificateList);
  certificate_handler_->ImportCertificates(
      certificates, onc_source, web_trust_certs.get());

  if (onc_source == chromeos::onc::ONC_SOURCE_USER_POLICY)
    SetTrustAnchors(web_trust_certs.Pass());
}

}  // namespace policy
