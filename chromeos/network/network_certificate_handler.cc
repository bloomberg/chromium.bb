// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_certificate_handler.h"

#include "base/observer_list_threadsafe.h"
#include "base/strings/stringprintf.h"
#include "chromeos/network/certificate_helper.h"
#include "net/base/hash_value.h"

namespace chromeos {

namespace {

// Root CA certificates that are built into Chrome use this token name.
const char kRootCertificateTokenName[] = "Builtin Object Token";

NetworkCertificateHandler::Certificate GetCertificate(
    const net::X509Certificate& cert,
    net::CertType type) {
  NetworkCertificateHandler::Certificate result;

  result.hash = net::HashValue(net::X509Certificate::CalculateFingerprint256(
                                   cert.os_cert_handle()))
                    .ToString();

  std::string alt_text;
  if (!cert.subject().organization_names.empty())
    alt_text = cert.subject().organization_names[0];
  if (alt_text.empty())
    alt_text = cert.subject().GetDisplayName();
  result.issued_by =
      certificate::GetIssuerCommonName(cert.os_cert_handle(), alt_text);

  result.issued_to = certificate::GetCertNameOrNickname(cert.os_cert_handle());
  result.issued_to_ascii =
      certificate::GetCertAsciiNameOrNickname(cert.os_cert_handle());

  if (type == net::USER_CERT) {
    int slot_id;
    std::string pkcs11_id =
        CertLoader::GetPkcs11IdAndSlotForCert(cert, &slot_id);
    result.pkcs11_id = base::StringPrintf("%i:%s", slot_id, pkcs11_id.c_str());
  } else if (type == net::CA_CERT) {
    if (!net::X509Certificate::GetPEMEncoded(cert.os_cert_handle(),
                                             &result.pem)) {
      LOG(ERROR) << "Unable to PEM-encode CA";
    }
  } else {
    NOTREACHED();
  }

  result.hardware_backed = CertLoader::IsCertificateHardwareBacked(&cert);

  return result;
}

}  // namespace

NetworkCertificateHandler::Certificate::Certificate() {}

NetworkCertificateHandler::Certificate::~Certificate() {}

NetworkCertificateHandler::Certificate::Certificate(const Certificate& other) =
    default;

NetworkCertificateHandler::NetworkCertificateHandler() {
  CertLoader::Get()->AddObserver(this);
  if (CertLoader::Get()->initial_load_finished())
    OnCertificatesLoaded(CertLoader::Get()->all_certs(), true);
}

NetworkCertificateHandler::~NetworkCertificateHandler() {
  CertLoader::Get()->RemoveObserver(this);
}

void NetworkCertificateHandler::AddObserver(
    NetworkCertificateHandler::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void NetworkCertificateHandler::RemoveObserver(
    NetworkCertificateHandler::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void NetworkCertificateHandler::OnCertificatesLoaded(
    const net::CertificateList& cert_list,
    bool /* initial_load */) {
  ProcessCertificates(cert_list);
}

void NetworkCertificateHandler::ProcessCertificates(
    const net::CertificateList& cert_list) {
  user_certificates_.clear();
  server_ca_certificates_.clear();

  // Add certificates to the appropriate list.
  for (const auto& cert_ref : cert_list) {
    const net::X509Certificate& cert = *cert_ref.get();
    net::X509Certificate::OSCertHandle cert_handle = cert.os_cert_handle();
    net::CertType type = certificate::GetCertType(cert_handle);
    switch (type) {
      case net::USER_CERT:
        user_certificates_.push_back(GetCertificate(cert, type));
        break;
      case net::CA_CERT: {
        // Exclude root CA certificates that are built into Chrome.
        std::string token_name = certificate::GetCertTokenName(cert_handle);
        if (token_name != kRootCertificateTokenName)
          server_ca_certificates_.push_back(GetCertificate(cert, type));
        else
          VLOG(2) << "Ignoring root cert";
        break;
      }
      default:
        // Ignore other certificates.
        VLOG(2) << "Ignoring cert type: " << type;
        break;
    }
  }

  for (auto& observer : observer_list_)
    observer.OnCertificatesChanged();
}

void NetworkCertificateHandler::SetCertificatesForTest(
    const net::CertificateList& cert_list) {
  ProcessCertificates(cert_list);
}

void NetworkCertificateHandler::NotifyCertificatsChangedForTest() {
  for (auto& observer : observer_list_)
    observer.OnCertificatesChanged();
}

}  // namespace chromeos
