// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/network_configuration_updater_impl.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/net/onc_utils.h"
#include "chrome/browser/policy/policy_map.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_utils.h"
#include "policy/policy_constants.h"

namespace policy {

NetworkConfigurationUpdaterImpl::NetworkConfigurationUpdaterImpl(
    PolicyService* device_policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler,
    scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer)
    : device_policy_change_registrar_(device_policy_service,
                                      PolicyNamespace(POLICY_DOMAIN_CHROME,
                                                      std::string())),
      user_policy_service_(NULL),
      device_policy_service_(device_policy_service),
      user_(NULL),
      network_config_handler_(network_config_handler),
      certificate_importer_(certificate_importer.Pass()) {
  device_policy_change_registrar_.Observe(
      key::kDeviceOpenNetworkConfiguration,
      base::Bind(&NetworkConfigurationUpdaterImpl::OnPolicyChanged,
                 base::Unretained(this),
                 chromeos::onc::ONC_SOURCE_DEVICE_POLICY));

  if (device_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    // Apply the current device policies immediately.
    VLOG(1) << "Device policy service is already initialized.";
    ApplyNetworkConfiguration(chromeos::onc::ONC_SOURCE_DEVICE_POLICY);
  } else {
    device_policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
  }
}

NetworkConfigurationUpdaterImpl::~NetworkConfigurationUpdaterImpl() {
  DCHECK(!user_policy_service_);
  DCHECK(!user_policy_change_registrar_);
  device_policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

void NetworkConfigurationUpdaterImpl::SetUserPolicyService(
    bool allow_trusted_certs_from_policy,
    const chromeos::User* user,
    PolicyService* user_policy_service) {
  VLOG(1) << "Got user policy service.";
  user_policy_service_ = user_policy_service;
  user_ = user;
  if (allow_trusted_certs_from_policy)
    SetAllowTrustedCertsFromPolicy();

  user_policy_change_registrar_.reset(new PolicyChangeRegistrar(
      user_policy_service_,
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())));
  user_policy_change_registrar_->Observe(
      key::kOpenNetworkConfiguration,
      base::Bind(&NetworkConfigurationUpdaterImpl::OnPolicyChanged,
                 base::Unretained(this),
                 chromeos::onc::ONC_SOURCE_USER_POLICY));

  if (user_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    VLOG(1) << "User policy service is already initialized.";
    ApplyNetworkConfiguration(chromeos::onc::ONC_SOURCE_USER_POLICY);
  } else {
    user_policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
  }
}

void NetworkConfigurationUpdaterImpl::UnsetUserPolicyService() {
  if (!user_policy_service_)
    return;

  user_policy_change_registrar_.reset();
  user_policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
  user_policy_service_ = NULL;
}

void NetworkConfigurationUpdaterImpl::OnPolicyUpdated(
    const PolicyNamespace& ns,
    const PolicyMap& previous,
    const PolicyMap& current) {
  // Ignore this call. Policy changes are already observed by the registrar.
}

void NetworkConfigurationUpdaterImpl::OnPolicyServiceInitialized(
    PolicyDomain domain) {
  if (domain != POLICY_DOMAIN_CHROME)
    return;

  // We don't know which policy service called this function, thus check
  // both. Multiple removes are handled gracefully.
  if (device_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    VLOG(1) << "Device policy service initialized.";
    device_policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
    ApplyNetworkConfiguration(chromeos::onc::ONC_SOURCE_DEVICE_POLICY);
  }
  if (user_policy_service_ &&
      user_policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    VLOG(1) << "User policy service initialized.";
    user_policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
    ApplyNetworkConfiguration(chromeos::onc::ONC_SOURCE_USER_POLICY);
  }
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
  PolicyService* policy_service;
  if (onc_source == chromeos::onc::ONC_SOURCE_USER_POLICY) {
    policy_key = key::kOpenNetworkConfiguration;
    policy_service = user_policy_service_;
  } else {
    policy_key = key::kDeviceOpenNetworkConfiguration;
    policy_service = device_policy_service_;
  }

  const PolicyMap& policies = policy_service->GetPolicies(
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
  chromeos::onc::ParseAndValidateOncForImport(
      onc_blob, onc_source, "", &network_configs, &certificates);

  scoped_ptr<net::CertificateList> web_trust_certs(new net::CertificateList);
  certificate_importer_->ImportCertificates(
      certificates, onc_source, web_trust_certs.get());

  std::string userhash;
  if (onc_source == chromeos::onc::ONC_SOURCE_USER_POLICY && user_) {
    userhash = user_->username_hash();
    chromeos::onc::ExpandStringPlaceholdersInNetworksForUser(user_,
                                                             &network_configs);
  }

  network_config_handler_->SetPolicy(onc_source, userhash, network_configs);

  if (onc_source == chromeos::onc::ONC_SOURCE_USER_POLICY)
    SetTrustAnchors(web_trust_certs.Pass());
}

}  // namespace policy
