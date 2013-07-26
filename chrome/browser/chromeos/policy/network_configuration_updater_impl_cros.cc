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
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_utils.h"
#include "policy/policy_constants.h"

namespace policy {

NetworkConfigurationUpdaterImplCros::NetworkConfigurationUpdaterImplCros(
    PolicyService* device_policy_service,
    chromeos::NetworkLibrary* network_library,
    scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer)
    : policy_change_registrar_(
          device_policy_service,
          PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())),
      network_library_(network_library),
      certificate_importer_(certificate_importer.Pass()),
      user_policy_service_(NULL),
      device_policy_service_(device_policy_service) {
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
  if (device_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    // Apply the current policies immediately.
    VLOG(1) << "Device policy service is already initialized.";
    ApplyNetworkConfigurations();
  } else {
    device_policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
  }
}

NetworkConfigurationUpdaterImplCros::~NetworkConfigurationUpdaterImplCros() {
  network_library_->RemoveNetworkProfileObserver(this);
  DCHECK(!user_policy_service_);
  device_policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

void NetworkConfigurationUpdaterImplCros::OnProfileListChanged() {
  VLOG(1) << "Network profile list changed, applying policies.";
  ApplyNetworkConfigurations();
}

void NetworkConfigurationUpdaterImplCros::SetUserPolicyService(
    bool allow_trust_certs_from_policy,
    const std::string& hashed_username,
    PolicyService* user_policy_service) {
  VLOG(1) << "Got user policy service.";
  user_policy_service_ = user_policy_service;
  if (allow_trust_certs_from_policy)
    SetAllowTrustedCertsFromPolicy();

  if (user_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    VLOG(1) << "User policy service is already initialized.";
    ApplyNetworkConfigurations();
  } else {
    user_policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
  }
}

void NetworkConfigurationUpdaterImplCros::UnsetUserPolicyService() {
  if (!user_policy_service_)
    return;

  user_policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
  user_policy_service_ = NULL;
}

void NetworkConfigurationUpdaterImplCros::OnPolicyUpdated(
    const PolicyNamespace& ns,
    const PolicyMap& previous,
    const PolicyMap& current) {
  // Ignore this call. Policy changes are already observed by the registrar.
}

void NetworkConfigurationUpdaterImplCros::OnPolicyServiceInitialized(
    PolicyDomain domain) {
  if (domain != POLICY_DOMAIN_CHROME)
    return;

  // We don't know which policy service called this function, thus check
  // both. Multiple removes are handled gracefully.
  if (device_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    VLOG(1) << "Device policy service initialized.";
    device_policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
  }
  if (user_policy_service_ &&
      user_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    VLOG(1) << "User policy service initialized.";
    user_policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
  }

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
  if (!device_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME))
    return;

  ApplyNetworkConfiguration(key::kDeviceOpenNetworkConfiguration,
                            chromeos::onc::ONC_SOURCE_DEVICE_POLICY,
                            device_policy_service_);
  if (user_policy_service_ &&
      user_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    ApplyNetworkConfiguration(key::kOpenNetworkConfiguration,
                              chromeos::onc::ONC_SOURCE_USER_POLICY,
                              user_policy_service_);
  }
}

void NetworkConfigurationUpdaterImplCros::ApplyNetworkConfiguration(
    const std::string& policy_key,
    chromeos::onc::ONCSource onc_source,
    PolicyService* policy_service) {
  VLOG(1) << "Apply policy for ONC source "
          << chromeos::onc::GetSourceAsString(onc_source);
  const PolicyMap& policies = policy_service->GetPolicies(
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
  chromeos::onc::ParseAndValidateOncForImport(
      onc_blob, onc_source, "", &network_configs, &certificates);

  scoped_ptr<net::CertificateList> web_trust_certs(new net::CertificateList);
  certificate_importer_->ImportCertificates(
      certificates, onc_source, web_trust_certs.get());

  network_library_->LoadOncNetworks(network_configs, onc_source);

  if (onc_source == chromeos::onc::ONC_SOURCE_USER_POLICY)
    SetTrustAnchors(web_trust_certs.Pass());
}

}  // namespace policy
