// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/certificate_provider/thread_safe_certificate_map.h"

#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {
namespace certificate_provider {
namespace {

void BuildFingerprintsMap(
    const std::map<std::string, certificate_provider::CertificateInfoList>&
        extension_to_certificates,
    ThreadSafeCertificateMap::ExtensionToFingerprintsMap* ext_to_certs_map) {
  for (const auto& entry : extension_to_certificates) {
    const std::string& extension_id = entry.first;
    auto& fingerprint_to_cert = (*ext_to_certs_map)[extension_id];
    for (const CertificateInfo& cert_info : entry.second) {
      const net::SHA256HashValue fingerprint =
          net::X509Certificate::CalculateFingerprint256(
              cert_info.certificate->os_cert_handle());
      fingerprint_to_cert[fingerprint] = cert_info;
    }
  }
}

}  // namespace

ThreadSafeCertificateMap::ThreadSafeCertificateMap() {}

ThreadSafeCertificateMap::~ThreadSafeCertificateMap() {}

void ThreadSafeCertificateMap::Update(
    const std::map<std::string, certificate_provider::CertificateInfoList>&
        extension_to_certificates) {
  ExtensionToFingerprintsMap new_ext_to_certs_map;
  BuildFingerprintsMap(extension_to_certificates, &new_ext_to_certs_map);

  base::AutoLock auto_lock(lock_);
  extension_to_certificates_.swap(new_ext_to_certs_map);
}

bool ThreadSafeCertificateMap::LookUpCertificate(
    const net::X509Certificate& cert,
    CertificateInfo* info,
    std::string* extension_id) {
  const net::SHA256HashValue fingerprint =
      net::X509Certificate::CalculateFingerprint256(cert.os_cert_handle());

  base::AutoLock auto_lock(lock_);
  for (const auto& entry : extension_to_certificates_) {
    const FingerprintToCertMap& certs = entry.second;
    const auto it = certs.find(fingerprint);
    if (it != certs.end()) {
      *info = it->second;
      *extension_id = entry.first;
      return true;
    }
  }
  return false;
}

void ThreadSafeCertificateMap::RemoveExtension(
    const std::string& extension_id) {
  base::AutoLock auto_lock(lock_);
  extension_to_certificates_.erase(extension_id);
}

}  // namespace certificate_provider
}  // namespace chromeos
