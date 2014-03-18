// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_certificate_importer_impl.h"

#include <cert.h>
#include <keyhi.h>
#include <pk11pub.h>

#include "base/base64.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/onc/onc_constants.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/crypto_module.h"
#include "net/base/net_errors.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"

#define ONC_LOG_WARNING(message)                                \
  NET_LOG_DEBUG("ONC Certificate Import Warning", message)
#define ONC_LOG_ERROR(message)                                  \
  NET_LOG_ERROR("ONC Certificate Import Error", message)

namespace chromeos {
namespace onc {

CertificateImporterImpl::CertificateImporterImpl(
    net::NSSCertDatabase* target_nssdb)
    : target_nssdb_(target_nssdb) {
  CHECK(target_nssdb);
}

bool CertificateImporterImpl::ImportCertificates(
    const base::ListValue& certificates,
    ::onc::ONCSource source,
    net::CertificateList* onc_trusted_certificates) {
  VLOG(2) << "ONC file has " << certificates.GetSize() << " certificates";

  // Web trust is only granted to certificates imported by the user.
  bool allow_trust_imports = source == ::onc::ONC_SOURCE_USER_IMPORT;
  if (!ParseAndStoreCertificates(allow_trust_imports,
                                 certificates,
                                 onc_trusted_certificates,
                                 NULL)) {
    LOG(ERROR) << "Cannot parse some of the certificates in the ONC from "
               << onc::GetSourceAsString(source);
    return false;
  }
  return true;
}

bool CertificateImporterImpl::ParseAndStoreCertificates(
    bool allow_trust_imports,
    const base::ListValue& certificates,
    net::CertificateList* onc_trusted_certificates,
    CertsByGUID* imported_server_and_ca_certs) {
  bool success = true;
  for (size_t i = 0; i < certificates.GetSize(); ++i) {
    const base::DictionaryValue* certificate = NULL;
    certificates.GetDictionary(i, &certificate);
    DCHECK(certificate != NULL);

    VLOG(2) << "Parsing certificate at index " << i << ": " << *certificate;

    if (!ParseAndStoreCertificate(allow_trust_imports,
                                  *certificate,
                                  onc_trusted_certificates,
                                  imported_server_and_ca_certs)) {
      success = false;
      ONC_LOG_ERROR(
          base::StringPrintf("Cannot parse certificate at index %zu", i));
    } else {
      VLOG(2) << "Successfully imported certificate at index " << i;
    }
  }
  return success;
}

// static
void CertificateImporterImpl::ListCertsWithNickname(
    const std::string& label,
    net::CertificateList* result,
    net::NSSCertDatabase* target_nssdb) {
  net::CertificateList all_certs;
  // TODO(tbarzic): Use async |ListCerts|.
  target_nssdb->ListCertsSync(&all_certs);
  result->clear();
  for (net::CertificateList::iterator iter = all_certs.begin();
       iter != all_certs.end(); ++iter) {
    if (iter->get()->os_cert_handle()->nickname) {
      // Separate the nickname stored in the certificate at the colon, since
      // NSS likes to store it as token:nickname.
      const char* delimiter =
          ::strchr(iter->get()->os_cert_handle()->nickname, ':');
      if (delimiter) {
        ++delimiter;  // move past the colon.
        if (strcmp(delimiter, label.c_str()) == 0) {
          result->push_back(*iter);
          continue;
        }
      }
    }
    // Now we find the private key for this certificate and see if it has a
    // nickname that matches.  If there is a private key, and it matches,
    // then this is a client cert that we are looking for.
    SECKEYPrivateKey* private_key = PK11_FindPrivateKeyFromCert(
        iter->get()->os_cert_handle()->slot,
        iter->get()->os_cert_handle(),
        NULL);  // wincx
    if (private_key) {
      char* private_key_nickname = PK11_GetPrivateKeyNickname(private_key);
      if (private_key_nickname && std::string(label) == private_key_nickname)
        result->push_back(*iter);
      PORT_Free(private_key_nickname);
      SECKEY_DestroyPrivateKey(private_key);
    }
  }
}

// static
bool CertificateImporterImpl::DeleteCertAndKeyByNickname(
    const std::string& label,
    net::NSSCertDatabase* target_nssdb) {
  net::CertificateList cert_list;
  ListCertsWithNickname(label, &cert_list, target_nssdb);
  bool result = true;
  for (net::CertificateList::iterator iter = cert_list.begin();
       iter != cert_list.end(); ++iter) {
    // If we fail, we try and delete the rest still.
    // TODO(gspencer): this isn't very "transactional".  If we fail on some, but
    // not all, then it's possible to leave things in a weird state.
    // Luckily there should only be one cert with a particular
    // label, and the cert not being found is one of the few reasons the
    // delete could fail, but still...  The other choice is to return
    // failure immediately, but that doesn't seem to do what is intended.
    if (!target_nssdb->DeleteCertAndKey(iter->get()))
      result = false;
  }
  return result;
}

bool CertificateImporterImpl::ParseAndStoreCertificate(
    bool allow_trust_imports,
    const base::DictionaryValue& certificate,
    net::CertificateList* onc_trusted_certificates,
    CertsByGUID* imported_server_and_ca_certs) {
  // Get out the attributes of the given certificate.
  std::string guid;
  certificate.GetStringWithoutPathExpansion(::onc::certificate::kGUID, &guid);
  DCHECK(!guid.empty());

  bool remove = false;
  if (certificate.GetBooleanWithoutPathExpansion(::onc::kRemove, &remove) &&
      remove) {
    if (!DeleteCertAndKeyByNickname(guid, target_nssdb_)) {
      ONC_LOG_ERROR("Unable to delete certificate");
      return false;
    } else {
      return true;
    }
  }

  // Not removing, so let's get the data we need to add this certificate.
  std::string cert_type;
  certificate.GetStringWithoutPathExpansion(::onc::certificate::kType,
                                            &cert_type);
  if (cert_type == ::onc::certificate::kServer ||
      cert_type == ::onc::certificate::kAuthority) {
    return ParseServerOrCaCertificate(allow_trust_imports,
                                      cert_type,
                                      guid,
                                      certificate,
                                      onc_trusted_certificates,
                                      imported_server_and_ca_certs);
  } else if (cert_type == ::onc::certificate::kClient) {
    return ParseClientCertificate(guid, certificate);
  }

  NOTREACHED();
  return false;
}

bool CertificateImporterImpl::ParseServerOrCaCertificate(
    bool allow_trust_imports,
    const std::string& cert_type,
    const std::string& guid,
    const base::DictionaryValue& certificate,
    net::CertificateList* onc_trusted_certificates,
    CertsByGUID* imported_server_and_ca_certs) {
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
        ONC_LOG_WARNING("Certificate contains unknown trust type " +
                        trust_type);
      }
    }
  }

  bool import_with_ssl_trust = false;
  if (web_trust_flag) {
    if (!allow_trust_imports)
      ONC_LOG_WARNING("Web trust not granted for certificate: " + guid);
    else
      import_with_ssl_trust = true;
  }

  std::string x509_data;
  if (!certificate.GetStringWithoutPathExpansion(::onc::certificate::kX509,
                                                 &x509_data) ||
      x509_data.empty()) {
    ONC_LOG_ERROR(
        "Certificate missing appropriate certificate data for type: " +
        cert_type);
    return false;
  }

  scoped_refptr<net::X509Certificate> x509_cert =
      DecodePEMCertificate(x509_data);
  if (!x509_cert.get()) {
    ONC_LOG_ERROR("Unable to create certificate from PEM encoding, type: " +
                  cert_type);
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
        trust & ~target_nssdb_->GetCertTrust(x509_cert.get(), net_cert_type);
    if (missing_trust_bits) {
      std::string error_reason;
      bool success = false;
      if (target_nssdb_->IsReadOnly(x509_cert.get())) {
        error_reason = " Certificate is stored read-only.";
      } else {
        success = target_nssdb_->SetCertTrust(x509_cert.get(),
                                              net_cert_type,
                                              trust);
      }
      if (!success) {
        ONC_LOG_ERROR("Certificate of type " + cert_type +
                      " was already present, but trust couldn't be set." +
                      error_reason);
      }
    }
  } else {
    net::CertificateList cert_list;
    cert_list.push_back(x509_cert);
    net::NSSCertDatabase::ImportCertFailureList failures;
    bool success = false;
    if (cert_type == ::onc::certificate::kServer)
      success = target_nssdb_->ImportServerCert(cert_list, trust, &failures);
    else  // Authority cert
      success = target_nssdb_->ImportCACerts(cert_list, trust, &failures);

    if (!failures.empty()) {
      ONC_LOG_ERROR(
          base::StringPrintf("Error ( %s ) importing %s certificate",
                             net::ErrorToString(failures[0].net_error),
                             cert_type.c_str()));
      return false;
    }

    if (!success) {
      ONC_LOG_ERROR("Unknown error importing " + cert_type + " certificate.");
      return false;
    }
  }

  if (web_trust_flag && onc_trusted_certificates)
    onc_trusted_certificates->push_back(x509_cert);

  if (imported_server_and_ca_certs)
    (*imported_server_and_ca_certs)[guid] = x509_cert;

  return true;
}

bool CertificateImporterImpl::ParseClientCertificate(
    const std::string& guid,
    const base::DictionaryValue& certificate) {
  std::string pkcs12_data;
  if (!certificate.GetStringWithoutPathExpansion(::onc::certificate::kPKCS12,
                                                 &pkcs12_data) ||
      pkcs12_data.empty()) {
    ONC_LOG_ERROR("PKCS12 data is missing for client certificate.");
    return false;
  }

  std::string decoded_pkcs12;
  if (!base::Base64Decode(pkcs12_data, &decoded_pkcs12)) {
    ONC_LOG_ERROR(
        "Unable to base64 decode PKCS#12 data: \"" + pkcs12_data + "\".");
    return false;
  }

  // Since this has a private key, always use the private module.
  crypto::ScopedPK11Slot private_slot(target_nssdb_->GetPrivateSlot());
  if (!private_slot)
    return false;
  scoped_refptr<net::CryptoModule> module(
      net::CryptoModule::CreateFromHandle(private_slot.get()));
  net::CertificateList imported_certs;

  int import_result = target_nssdb_->ImportFromPKCS12(
      module.get(), decoded_pkcs12, base::string16(), false, &imported_certs);
  if (import_result != net::OK) {
    ONC_LOG_ERROR(
        base::StringPrintf("Unable to import client certificate (error %s)",
                           net::ErrorToString(import_result)));
    return false;
  }

  if (imported_certs.size() == 0) {
    ONC_LOG_WARNING("PKCS12 data contains no importable certificates.");
    return true;
  }

  if (imported_certs.size() != 1) {
    ONC_LOG_WARNING("ONC File: PKCS12 data contains more than one certificate. "
                    "Only the first one will be imported.");
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
    ONC_LOG_WARNING("Unable to find private key for certificate.");
  }
  return true;
}

}  // namespace onc
}  // namespace chromeos
