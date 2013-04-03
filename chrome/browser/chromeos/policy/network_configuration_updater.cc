// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/network_configuration_updater.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/cert_trust_anchor_provider.h"
#include "net/cert/x509_certificate.h"
#include "policy/policy_constants.h"

using content::BrowserThread;

namespace policy {

namespace {

// A simple implementation of net::CertTrustAnchorProvider that returns a list
// of certificates that can be set by the owner of this object.
class CrosTrustAnchorProvider : public net::CertTrustAnchorProvider {
 public:
  CrosTrustAnchorProvider() {}
  virtual ~CrosTrustAnchorProvider() {}

  // CertTrustAnchorProvider overrides.
  virtual const net::CertificateList& GetAdditionalTrustAnchors() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    return trust_anchors_;
  }

  void SetTrustAnchors(scoped_ptr<net::CertificateList> trust_anchors) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    trust_anchors_.swap(*trust_anchors);
  }

 private:
  net::CertificateList trust_anchors_;

  DISALLOW_COPY_AND_ASSIGN(CrosTrustAnchorProvider);
};

}  // namespace

NetworkConfigurationUpdater::NetworkConfigurationUpdater(
    PolicyService* policy_service,
    chromeos::NetworkLibrary* network_library)
    : policy_change_registrar_(
          policy_service, PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())),
      network_library_(network_library),
      user_policy_initialized_(false),
      allow_trusted_certificates_from_policy_(false),
      policy_service_(policy_service),
      cert_trust_provider_(new CrosTrustAnchorProvider()) {
  DCHECK(network_library_);
  policy_change_registrar_.Observe(
      key::kDeviceOpenNetworkConfiguration,
      base::Bind(&NetworkConfigurationUpdater::OnPolicyChanged,
                 base::Unretained(this),
                 chromeos::onc::ONC_SOURCE_DEVICE_POLICY));
  policy_change_registrar_.Observe(
      key::kOpenNetworkConfiguration,
      base::Bind(&NetworkConfigurationUpdater::OnPolicyChanged,
                 base::Unretained(this),
                 chromeos::onc::ONC_SOURCE_USER_POLICY));

  network_library_->AddNetworkProfileObserver(this);

  // Apply the current policies immediately.
  ApplyNetworkConfigurations();
}

NetworkConfigurationUpdater::~NetworkConfigurationUpdater() {
  network_library_->RemoveNetworkProfileObserver(this);
  bool posted = BrowserThread::DeleteSoon(
      BrowserThread::IO, FROM_HERE, cert_trust_provider_);
  if (!posted)
    delete cert_trust_provider_;
}

void NetworkConfigurationUpdater::OnProfileListChanged() {
  VLOG(1) << "Network profile list changed, applying policies.";
  ApplyNetworkConfigurations();
}

void NetworkConfigurationUpdater::OnUserPolicyInitialized() {
  VLOG(1) << "User policy initialized, applying policies.";
  user_policy_initialized_ = true;
  ApplyNetworkConfigurations();
}

net::CertTrustAnchorProvider*
    NetworkConfigurationUpdater::GetCertTrustAnchorProvider() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cert_trust_provider_;
}

void NetworkConfigurationUpdater::OnPolicyChanged(
    chromeos::onc::ONCSource onc_source,
    const base::Value* previous,
    const base::Value* current) {
  VLOG(1) << "Policy for ONC source "
          << chromeos::onc::GetSourceAsString(onc_source) << " changed.";
  ApplyNetworkConfigurations();
}

void NetworkConfigurationUpdater::ApplyNetworkConfigurations() {
  ApplyNetworkConfiguration(key::kDeviceOpenNetworkConfiguration,
                            chromeos::onc::ONC_SOURCE_DEVICE_POLICY);
  if (user_policy_initialized_) {
    ApplyNetworkConfiguration(key::kOpenNetworkConfiguration,
                              chromeos::onc::ONC_SOURCE_USER_POLICY);
  }
}

void NetworkConfigurationUpdater::ApplyNetworkConfiguration(
    const std::string& policy_key,
    chromeos::onc::ONCSource onc_source) {
  VLOG(1) << "Apply policy for ONC source "
          << chromeos::onc::GetSourceAsString(onc_source);
  const PolicyMap& policies = policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  const base::Value* policy_value = policies.GetValue(policy_key);

  std::string new_network_config;
  if (policy_value != NULL) {
    // If the policy is not a string, we issue a warning, but still clear the
    // network configuration.
    if (!policy_value->GetAsString(&new_network_config)) {
      LOG(WARNING) << "ONC policy for source "
                   << chromeos::onc::GetSourceAsString(onc_source)
                   << " is not a string value.";
    }
  }

  // An empty string is not a valid ONC and generates warnings and
  // errors. Replace by a valid empty configuration.
  if (new_network_config.empty())
    new_network_config = chromeos::onc::kEmptyUnencryptedConfiguration;

  scoped_ptr<net::CertificateList> web_trust_certs(new net::CertificateList());
  if (!network_library_->LoadOncNetworks(new_network_config, "", onc_source,
                                         web_trust_certs.get())) {
    LOG(ERROR) << "Errors occurred during the ONC policy application.";
  }

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (onc_source == chromeos::onc::ONC_SOURCE_USER_POLICY &&
      allow_trusted_certificates_from_policy_ &&
      command_line->HasSwitch(switches::kEnableWebTrustCerts)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&CrosTrustAnchorProvider::SetTrustAnchors,
                   base::Unretained(static_cast<CrosTrustAnchorProvider*>(
                       cert_trust_provider_)),
                   base::Passed(&web_trust_certs)));
  }
}

}  // namespace policy
