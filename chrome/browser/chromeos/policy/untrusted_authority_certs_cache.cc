// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/untrusted_authority_certs_cache.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"
#include "chromeos/network/onc/onc_utils.h"

namespace policy {

UntrustedAuthorityCertsCache::UntrustedAuthorityCertsCache(
    const std::vector<std::string>& x509_authority_certs) {
  for (const auto& x509_authority_cert : x509_authority_certs) {
    net::ScopedCERTCertificate x509_cert =
        chromeos::onc::DecodePEMCertificate(x509_authority_cert);
    if (!x509_cert) {
      LOG(ERROR) << "Unable to create untrusted authority certificate from PEM "
                    "encoding";
      continue;
    }

    untrusted_authority_certs_.push_back(std::move(x509_cert));
  }
}

UntrustedAuthorityCertsCache::~UntrustedAuthorityCertsCache() {}

// static
std::vector<std::string>
UntrustedAuthorityCertsCache::GetUntrustedAuthoritiesFromDeviceOncPolicy() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->GetDeviceNetworkConfigurationUpdater()
      ->GetAuthorityCertificates();
}

}  // namespace policy
