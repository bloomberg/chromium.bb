// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_certificate_importer_impl.h"

#include <cert.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <stddef.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/onc/onc_utils.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/crypto_module.h"
#include "net/base/net_errors.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {
namespace onc {

namespace {

void CallBackOnOriginLoop(
    const scoped_refptr<base::SingleThreadTaskRunner>& origin_loop,
    const CertificateImporter::DoneCallback& callback,
    bool success,
    const net::CertificateList& onc_trusted_certificates) {
  origin_loop->PostTask(
      FROM_HERE, base::Bind(callback, success, onc_trusted_certificates));
}

}  // namespace

CertificateImporterImpl::CertificateImporterImpl(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    net::NSSCertDatabase* target_nssdb)
    : io_task_runner_(io_task_runner),
      target_nssdb_(target_nssdb),
      weak_factory_(this) {
  CHECK(target_nssdb);
}

CertificateImporterImpl::~CertificateImporterImpl() {
}

void CertificateImporterImpl::ImportCertificates(
    const base::ListValue& certificates,
    ::onc::ONCSource source,
    const DoneCallback& done_callback) {
  VLOG(2) << "ONC file has " << certificates.GetSize() << " certificates";
  // |done_callback| must only be called as long as |this| still exists.
  // Thereforce, call back to |this|. This check of |this| must happen last and
  // on the origin thread.
  DoneCallback callback_to_this =
      base::Bind(&CertificateImporterImpl::RunDoneCallback,
                 weak_factory_.GetWeakPtr(),
                 done_callback);

  // |done_callback| must be called on the origin thread.
  DoneCallback callback_on_origin_loop =
      base::Bind(&CallBackOnOriginLoop,
                 base::ThreadTaskRunnerHandle::Get(),
                 callback_to_this);

  // This is the actual function that imports the certificates.
  base::Closure import_certs_callback =
      base::Bind(&ParseAndStoreCertificates,
                 source,
                 callback_on_origin_loop,
                 base::Owned(certificates.DeepCopy()),
                 target_nssdb_);

  // The NSSCertDatabase must be accessed on |io_task_runner_|
  io_task_runner_->PostTask(FROM_HERE, import_certs_callback);
}

// static
void CertificateImporterImpl::ParseAndStoreCertificates(
    ::onc::ONCSource source,
    const DoneCallback& done_callback,
    base::ListValue* certificates,
    net::NSSCertDatabase* nssdb) {
  // Web trust is only granted to certificates imported by the user.
  bool allow_trust_imports = source == ::onc::ONC_SOURCE_USER_IMPORT;
  net::CertificateList onc_trusted_certificates;
  bool success = true;
  for (size_t i = 0; i < certificates->GetSize(); ++i) {
    const base::DictionaryValue* certificate = NULL;
    certificates->GetDictionary(i, &certificate);
    DCHECK(certificate != NULL);

    VLOG(2) << "Parsing certificate at index " << i << ": " << *certificate;

    if (!ParseAndStoreCertificate(allow_trust_imports,
                                  *certificate,
                                  nssdb,
                                  &onc_trusted_certificates)) {
      success = false;
      LOG(ERROR) << "Cannot parse certificate at index " << i;
    } else {
      VLOG(2) << "Successfully imported certificate at index " << i;
    }
  }

  done_callback.Run(success, onc_trusted_certificates);
}

void CertificateImporterImpl::RunDoneCallback(
    const CertificateImporter::DoneCallback& callback,
    bool success,
    const net::CertificateList& onc_trusted_certificates) {
  if (!success)
    NET_LOG_ERROR("ONC Certificate Import Error", "");
  callback.Run(success, onc_trusted_certificates);
}

bool CertificateImporterImpl::ParseAndStoreCertificate(
    bool allow_trust_imports,
    const base::DictionaryValue& certificate,
    net::NSSCertDatabase* nssdb,
    net::CertificateList* onc_trusted_certificates) {
  std::string guid;
  certificate.GetStringWithoutPathExpansion(::onc::certificate::kGUID, &guid);
  DCHECK(!guid.empty());

  std::string cert_type;
  certificate.GetStringWithoutPathExpansion(::onc::certificate::kType,
                                            &cert_type);
  if (cert_type == ::onc::certificate::kServer ||
      cert_type == ::onc::certificate::kAuthority) {
    return ParseServerOrCaCertificate(allow_trust_imports,
                                      cert_type,
                                      guid,
                                      certificate,
                                      nssdb,
                                      onc_trusted_certificates);
  } else if (cert_type == ::onc::certificate::kClient) {
    return ParseClientCertificate(guid, certificate, nssdb);
  }

  NOTREACHED();
  return false;
}

bool CertificateImporterImpl::ParseServerOrCaCertificate(
    bool allow_trust_imports,
    const std::string& cert_type,
    const std::string& guid,
    const base::DictionaryValue& certificate,
    net::NSSCertDatabase* nssdb,
    net::CertificateList* onc_trusted_certificates) {
  bool web_trust_flag = false;
  const base::ListValue* trust_list = NULL;
  if (certificate.GetListWithoutPathExpansion(::onc::certificate::kTrustBits,
                                              &trust_list)) {
    for (base::ListValue::const_iterator it = trust_list->begin();
         it != trust_list->end(); ++it) {
      std::string trust_type;
      if (!(*it)->GetAsString(&trust_type))
        NOTREACHED();

      if (trust_type == ::onc::certificate::kWeb) {
        // "Web" implies that the certificate is to be trusted for SSL
        // identification.
        web_trust_flag = true;
      } else {
        // Trust bits should only increase trust and never restrict. Thus,
        // ignoring unknown bits should be safe.
        LOG(WARNING) << "Certificate contains unknown trust type "
                     << trust_type;
      }
    }
  }

  bool import_with_ssl_trust = false;
  if (web_trust_flag) {
    if (!allow_trust_imports)
      LOG(WARNING) << "Web trust not granted for certificate: " << guid;
    else
      import_with_ssl_trust = true;
  }

  std::string x509_data;
  if (!certificate.GetStringWithoutPathExpansion(::onc::certificate::kX509,
                                                 &x509_data) ||
      x509_data.empty()) {
    LOG(ERROR) << "Certificate missing appropriate certificate data for type: "
               << cert_type;
    return false;
  }

  scoped_refptr<net::X509Certificate> x509_cert =
      DecodePEMCertificate(x509_data);
  if (!x509_cert.get()) {
    LOG(ERROR) << "Unable to create certificate from PEM encoding, type: "
               << cert_type;
    return false;
  }

  net::NSSCertDatabase::TrustBits trust = (import_with_ssl_trust ?
                                           net::NSSCertDatabase::TRUSTED_SSL :
                                           net::NSSCertDatabase::TRUST_DEFAULT);

  if (x509_cert->os_cert_handle()->isperm) {
    net::CertType net_cert_type =
        cert_type == ::onc::certificate::kServer ? net::SERVER_CERT
                                                 : net::CA_CERT;
    VLOG(1) << "Certificate is already installed.";
    net::NSSCertDatabase::TrustBits missing_trust_bits =
        trust & ~nssdb->GetCertTrust(x509_cert.get(), net_cert_type);
    if (missing_trust_bits) {
      std::string error_reason;
      bool success = false;
      if (nssdb->IsReadOnly(x509_cert.get())) {
        error_reason = " Certificate is stored read-only.";
      } else {
        success = nssdb->SetCertTrust(x509_cert.get(), net_cert_type, trust);
      }
      if (!success) {
        LOG(ERROR) << "Certificate of type " << cert_type
                   << " was already present, but trust couldn't be set."
                   << error_reason;
      }
    }
  } else {
    net::CertificateList cert_list;
    cert_list.push_back(x509_cert);
    net::NSSCertDatabase::ImportCertFailureList failures;
    bool success = false;
    if (cert_type == ::onc::certificate::kServer)
      success = nssdb->ImportServerCert(cert_list, trust, &failures);
    else  // Authority cert
      success = nssdb->ImportCACerts(cert_list, trust, &failures);

    if (!failures.empty()) {
      std::string error_string = net::ErrorToString(failures[0].net_error);
      LOG(ERROR) << "Error ( " << error_string
                 << " ) importing certificate of type " << cert_type;
      return false;
    }

    if (!success) {
      LOG(ERROR) << "Unknown error importing " << cert_type << " certificate.";
      return false;
    }
  }

  if (web_trust_flag && onc_trusted_certificates)
    onc_trusted_certificates->push_back(x509_cert);

  return true;
}

bool CertificateImporterImpl::ParseClientCertificate(
    const std::string& guid,
    const base::DictionaryValue& certificate,
    net::NSSCertDatabase* nssdb) {
  std::string pkcs12_data;
  if (!certificate.GetStringWithoutPathExpansion(::onc::certificate::kPKCS12,
                                                 &pkcs12_data) ||
      pkcs12_data.empty()) {
    LOG(ERROR) << "PKCS12 data is missing for client certificate.";
    return false;
  }

  std::string decoded_pkcs12;
  if (!base::Base64Decode(pkcs12_data, &decoded_pkcs12)) {
    LOG(ERROR) << "Unable to base64 decode PKCS#12 data: \"" << pkcs12_data
               << "\".";
    return false;
  }

  // Since this has a private key, always use the private module.
  crypto::ScopedPK11Slot private_slot(nssdb->GetPrivateSlot());
  if (!private_slot)
    return false;

  net::CertificateList imported_certs;

  int import_result =
      nssdb->ImportFromPKCS12(private_slot.get(), decoded_pkcs12,
                              base::string16(), false, &imported_certs);
  if (import_result != net::OK) {
    std::string error_string = net::ErrorToString(import_result);
    LOG(ERROR) << "Unable to import client certificate, error: "
               << error_string;
    return false;
  }

  if (imported_certs.size() == 0) {
    LOG(WARNING) << "PKCS12 data contains no importable certificates.";
    return true;
  }

  if (imported_certs.size() != 1) {
    LOG(WARNING) << "ONC File: PKCS12 data contains more than one certificate. "
                    "Only the first one will be imported.";
  }

  scoped_refptr<net::X509Certificate> cert_result = imported_certs[0];

  // Find the private key associated with this certificate, and set the
  // nickname on it.
  SECKEYPrivateKey* private_key = PK11_FindPrivateKeyFromCert(
      cert_result->os_cert_handle()->slot,
      cert_result->os_cert_handle(),
      NULL);  // wincx
  if (private_key) {
    PK11_SetPrivateKeyNickname(private_key, const_cast<char*>(guid.c_str()));
    SECKEY_DestroyPrivateKey(private_key);
  } else {
    LOG(WARNING) << "Unable to find private key for certificate.";
  }
  return true;
}

}  // namespace onc
}  // namespace chromeos
