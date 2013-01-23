// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_certificate_importer.h"

#include <cert.h>
#include <keyhi.h>
#include <pk11pub.h>

#include "base/base64.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/onc/onc_constants.h"
#include "net/base/crypto_module.h"
#include "net/base/net_errors.h"
#include "net/base/nss_cert_database.h"
#include "net/base/pem_tokenizer.h"
#include "net/base/x509_certificate.h"

#define ONC_LOG_WARNING(message) NET_LOG_WARNING("ONC", message)
#define ONC_LOG_ERROR(message) NET_LOG_ERROR("ONC", message)

namespace {

// The PEM block header used for DER certificates
const char kCertificateHeader[] = "CERTIFICATE";
// This is an older PEM marker for DER certificates.
const char kX509CertificateHeader[] = "X509 CERTIFICATE";

}  // namespace

namespace chromeos {
namespace onc {

CertificateImporter::CertificateImporter(
    ONCSource onc_source,
    bool allow_web_trust_from_policy)
    : onc_source_(onc_source),
      allow_web_trust_from_policy_(allow_web_trust_from_policy) {
}

CertificateImporter::ParseResult CertificateImporter::ParseAndStoreCertificates(
    const base::ListValue& certificates) {
  size_t successful_imports = 0;
  for (size_t i = 0; i < certificates.GetSize(); ++i) {
    const base::DictionaryValue* certificate = NULL;
    if (!certificates.GetDictionary(i, &certificate)) {
      ONC_LOG_ERROR("Certificate data malformed");
      continue;
    }

    VLOG(2) << "Parsing certificate at index " << i << ": " << *certificate;

    if (!ParseAndStoreCertificate(*certificate)) {
      ONC_LOG_ERROR(
          base::StringPrintf("Cannot parse certificate at index %zu", i));
    } else {
      VLOG(2) << "Successfully imported certificate at index " << i;
      ++successful_imports;
    }
  }

  if (successful_imports == certificates.GetSize())
    return IMPORT_OK;
  else if (successful_imports == 0)
    return IMPORT_FAILED;
  else
    return IMPORT_INCOMPLETE;
}

bool CertificateImporter::ParseAndStoreCertificate(
    const base::DictionaryValue& certificate) {
  // Get out the attributes of the given certificate.
  std::string guid;
  if (!certificate.GetString(certificate::kGUID, &guid) || guid.empty()) {
    ONC_LOG_ERROR("Certificate missing GUID identifier");
    return false;
  }

  bool remove = false;
  if (certificate.GetBoolean(kRemove, &remove) && remove) {
    if (!DeleteCertAndKeyByNickname(guid)) {
      ONC_LOG_ERROR("Unable to delete certificate");
      return false;
    } else {
      return true;
    }
  }

  // Not removing, so let's get the data we need to add this certificate.
  std::string cert_type;
  certificate.GetString(certificate::kType, &cert_type);
  if (cert_type == certificate::kServer || cert_type == certificate::kAuthority)
    return ParseServerOrCaCertificate(cert_type, guid, certificate);

  if (cert_type == certificate::kClient)
    return ParseClientCertificate(guid, certificate);

  ONC_LOG_ERROR("Certificate of unknown type: " + cert_type);
  return false;
}

// static
void CertificateImporter::ListCertsWithNickname(const std::string& label,
                                                net::CertificateList* result) {
  net::CertificateList all_certs;
  net::NSSCertDatabase::GetInstance()->ListCerts(&all_certs);
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
bool CertificateImporter::DeleteCertAndKeyByNickname(const std::string& label) {
  net::CertificateList cert_list;
  ListCertsWithNickname(label, &cert_list);
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
    if (!net::NSSCertDatabase::GetInstance()->DeleteCertAndKey(iter->get()))
      result = false;
  }
  return result;
}

bool CertificateImporter::ParseServerOrCaCertificate(
    const std::string& cert_type,
    const std::string& guid,
    const base::DictionaryValue& certificate) {
  // Device policy can't import certificates.
  if (onc_source_ == ONC_SOURCE_DEVICE_POLICY) {
    // This isn't a parsing error.
    ONC_LOG_WARNING("Refusing to import certificate from device policy.");
    return true;
  }

  bool web_trust = false;
  const base::ListValue* trust_list = NULL;
  if (certificate.GetList(certificate::kTrust, &trust_list)) {
    for (size_t i = 0; i < trust_list->GetSize(); ++i) {
      std::string trust_type;
      if (!trust_list->GetString(i, &trust_type)) {
        ONC_LOG_ERROR("Certificate trust is invalid");
        return false;
      }
      if (trust_type == certificate::kWeb) {
        // "Web" implies that the certificate is to be trusted for SSL
        // identification.
        web_trust = true;
      } else {
        ONC_LOG_ERROR("Certificate contains unknown trust type " + trust_type);
        return false;
      }
    }
  }

  // Web trust is only granted to certificates imported for a managed user
  // on a managed device.
  if (onc_source_ == ONC_SOURCE_USER_POLICY &&
      web_trust && !allow_web_trust_from_policy_) {
    LOG(WARNING) << "Web trust not granted for certificate: " << guid;
    web_trust = false;
  }

  std::string x509_data;
  if (!certificate.GetString(certificate::kX509, &x509_data) ||
      x509_data.empty()) {
    ONC_LOG_ERROR(
        "Certificate missing appropriate certificate data for type: " +
        cert_type);
    return false;
  }

  // Parse PEM certificate, and get the decoded data for use in creating
  // certificate below.
  std::vector<std::string> pem_headers;
  pem_headers.push_back(kCertificateHeader);
  pem_headers.push_back(kX509CertificateHeader);

  net::PEMTokenizer pem_tokenizer(x509_data, pem_headers);
  std::string decoded_x509;
  if (!pem_tokenizer.GetNext()) {
    // If we failed to read the data as a PEM file, then let's just try plain
    // base64 decode: some versions of Spigots didn't apply the PEM marker
    // strings. For this to work, there has to be no white space, and it has to
    // only contain the base64-encoded data.
    if (!base::Base64Decode(x509_data, &decoded_x509)) {
      ONC_LOG_ERROR("Unable to base64 decode X509 data: " + x509_data);
      return false;
    }
  } else {
    decoded_x509 = pem_tokenizer.data();
  }

  scoped_refptr<net::X509Certificate> x509_cert =
      net::X509Certificate::CreateFromBytesWithNickname(
          decoded_x509.data(),
          decoded_x509.size(),
          guid.c_str());
  if (!x509_cert.get()) {
    ONC_LOG_ERROR("Unable to create X509 certificate from bytes.");
    return false;
  }

  // Due to a mismatch regarding cert identity between NSS (cert identity is
  // determined by the raw bytes) and ONC (cert identity is determined by
  // GUIDs), we have to special-case a number of situations here:
  //
  // a) The cert bits we're trying to insert are already present in the NSS cert
  //    store. This is indicated by the isperm bit in CERTCertificateStr. Since
  //    we might have to update the nick name, we just delete the existing cert
  //    and reimport the cert bits.
  // b) NSS gives us an actual temporary certificate. In this case, there is no
  //    identical certificate known to NSS, so we can safely import the
  //    certificate. The GUID being imported may still be on a different
  //    certificate, and we could jump through hoops to reimport the existing
  //    certificate with a different nickname. However, that would mean lots of
  //    effort for a case that's pretty much illegal (reusing GUIDs contradicts
  //    the intention of GUIDs), so we just report an error.
  //
  // TODO(mnissler, gspencer): We should probably switch to a mode where we
  // keep our own database for mapping GUIDs to certs in order to enable several
  // GUIDs to map to the same cert. See http://crosbug.com/26073.
  net::NSSCertDatabase* cert_database = net::NSSCertDatabase::GetInstance();
  if (x509_cert->os_cert_handle()->isperm) {
    if (!cert_database->DeleteCertAndKey(x509_cert.get())) {
      ONC_LOG_ERROR("Unable to delete X509 certificate.");
      return false;
    }

    // Reload the cert here to get an actual temporary cert instance.
    x509_cert =
        net::X509Certificate::CreateFromBytesWithNickname(
            decoded_x509.data(),
            decoded_x509.size(),
            guid.c_str());
    if (!x509_cert.get()) {
      ONC_LOG_ERROR("Unable to create X509 certificate from bytes.");
      return false;
    }
    DCHECK(!x509_cert->os_cert_handle()->isperm);
    DCHECK(x509_cert->os_cert_handle()->istemp);
  }

  // Make sure the GUID is not already taken. Note that for the reimport case we
  // have removed the existing cert above.
  net::CertificateList certs;
  ListCertsWithNickname(guid, &certs);
  if (!certs.empty()) {
    ONC_LOG_ERROR("Certificate GUID is already in use: " + guid);
    return false;
  }

  net::CertificateList cert_list;
  cert_list.push_back(x509_cert);
  net::NSSCertDatabase::ImportCertFailureList failures;
  bool success = false;
  net::NSSCertDatabase::TrustBits trust = web_trust ?
                                          net::NSSCertDatabase::TRUSTED_SSL :
                                          net::NSSCertDatabase::TRUST_DEFAULT;
  if (cert_type == certificate::kServer)
    success = cert_database->ImportServerCert(cert_list, trust, &failures);
  else  // Authority cert
    success = cert_database->ImportCACerts(cert_list, trust, &failures);

  if (!failures.empty()) {
    ONC_LOG_ERROR("Error (" + net::ErrorToString(failures[0].net_error) +
            ") importing " + cert_type + " certificate");
    return false;
  }
  if (!success) {
    ONC_LOG_ERROR("Unknown error importing " + cert_type + " certificate.");
    return false;
  }

  return true;
}

bool CertificateImporter::ParseClientCertificate(
    const std::string& guid,
    const base::DictionaryValue& certificate) {
  std::string pkcs12_data;
  if (!certificate.GetString(certificate::kPKCS12, &pkcs12_data) ||
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
  net::NSSCertDatabase* cert_database = net::NSSCertDatabase::GetInstance();
  scoped_refptr<net::CryptoModule> module(cert_database->GetPrivateModule());
  net::CertificateList imported_certs;

  int import_result = cert_database->ImportFromPKCS12(
      module.get(), decoded_pkcs12, string16(), false, &imported_certs);
  if (import_result != net::OK) {
    ONC_LOG_ERROR("Unable to import client certificate (error " +
                  net::ErrorToString(import_result) + ").");
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
