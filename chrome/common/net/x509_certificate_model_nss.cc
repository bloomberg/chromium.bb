// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include <cert.h>
#include <cms.h>
#include <hasht.h>
#include <keyhi.h>  // SECKEY_DestroyPrivateKey
#include <keythi.h>  // SECKEYPrivateKey
#include <pk11pub.h>  // PK11_FindKeyByAnyCert
#include <seccomon.h>  // SECItem
#include <sechash.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertHelper.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertificate.h"
#include "chrome/third_party/mozilla_security_manager/nsUsageArrayHelper.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"

namespace psm = mozilla_security_manager;

namespace {

// Convert a char* return value from NSS into a std::string and free the NSS
// memory.  If the arg is NULL, an empty string will be returned instead.
std::string Stringize(char* nss_text, const std::string& alternative_text) {
  if (!nss_text)
    return alternative_text;

  std::string s = nss_text;
  PORT_Free(nss_text);
  return s;
}

// Hash a certificate using the given algorithm, return the result as a
// colon-seperated hex string.  The len specified is the number of bytes
// required for storing the raw fingerprint.
// (It's a bit redundant that the caller needs to specify len in addition to the
// algorithm, but given the limited uses, not worth fixing.)
std::string HashCert(CERTCertificate* cert, HASH_HashType algorithm, int len) {
  unsigned char fingerprint[HASH_LENGTH_MAX];

  DCHECK(NULL != cert->derCert.data);
  DCHECK_NE(0U, cert->derCert.len);
  DCHECK_LE(len, HASH_LENGTH_MAX);
  memset(fingerprint, 0, len);
  SECStatus rv = HASH_HashBuf(algorithm, fingerprint, cert->derCert.data,
                              cert->derCert.len);
  DCHECK_EQ(rv, SECSuccess);
  return x509_certificate_model::ProcessRawBytes(fingerprint, len);
}

std::string ProcessSecAlgorithmInternal(SECAlgorithmID* algorithm_id) {
  return psm::GetOIDText(&algorithm_id->algorithm);
}

std::string ProcessExtension(
    const std::string& critical_label,
    const std::string& non_critical_label,
    CERTCertExtension* extension) {
  std::string criticality =
      extension->critical.data && extension->critical.data[0] ?
          critical_label : non_critical_label;
  return criticality + "\n" + psm::ProcessExtensionData(extension);
}

std::string GetNickname(net::X509Certificate::OSCertHandle cert_handle) {
  std::string name;
  if (cert_handle->nickname) {
    name = cert_handle->nickname;
    // Hack copied from mozilla: Cut off text before first :, which seems to
    // just be the token name.
    size_t colon_pos = name.find(':');
    if (colon_pos != std::string::npos)
      name = name.substr(colon_pos + 1);
  }
  return name;
}

////////////////////////////////////////////////////////////////////////////////
// NSS certificate export functions.

struct NSSCMSMessageDeleter {
  inline void operator()(NSSCMSMessage* x) const {
    NSS_CMSMessage_Destroy(x);
  }
};
typedef scoped_ptr<NSSCMSMessage, NSSCMSMessageDeleter> ScopedNSSCMSMessage;

struct FreeNSSCMSSignedData {
  inline void operator()(NSSCMSSignedData* x) const {
    NSS_CMSSignedData_Destroy(x);
  }
};
typedef scoped_ptr<NSSCMSSignedData, FreeNSSCMSSignedData>
    ScopedNSSCMSSignedData;

}  // namespace

namespace x509_certificate_model {

using net::X509Certificate;
using std::string;

string GetCertNameOrNickname(X509Certificate::OSCertHandle cert_handle) {
  string name = ProcessIDN(
      Stringize(CERT_GetCommonName(&cert_handle->subject), std::string()));
  if (!name.empty())
    return name;
  return GetNickname(cert_handle);
}

string GetTokenName(X509Certificate::OSCertHandle cert_handle) {
  return psm::GetCertTokenName(cert_handle);
}

string GetVersion(X509Certificate::OSCertHandle cert_handle) {
  // If the version field is omitted from the certificate, the default
  // value is v1(0).
  unsigned long version = 0;
  if (cert_handle->version.len == 0 ||
      SEC_ASN1DecodeInteger(&cert_handle->version, &version) == SECSuccess) {
    return base::UintToString(version + 1);
  }
  return std::string();
}

net::CertType GetType(X509Certificate::OSCertHandle cert_handle) {
    return psm::GetCertType(cert_handle);
}

void GetUsageStrings(X509Certificate::OSCertHandle cert_handle,
                     std::vector<string>* usages) {
  psm::GetCertUsageStrings(cert_handle, usages);
}

string GetSerialNumberHexified(X509Certificate::OSCertHandle cert_handle,
                               const string& alternative_text) {
  return Stringize(CERT_Hexify(&cert_handle->serialNumber, true),
                   alternative_text);
}

string GetIssuerCommonName(X509Certificate::OSCertHandle cert_handle,
                           const string& alternative_text) {
  return Stringize(CERT_GetCommonName(&cert_handle->issuer), alternative_text);
}

string GetIssuerOrgName(X509Certificate::OSCertHandle cert_handle,
                        const string& alternative_text) {
  return Stringize(CERT_GetOrgName(&cert_handle->issuer), alternative_text);
}

string GetIssuerOrgUnitName(X509Certificate::OSCertHandle cert_handle,
                            const string& alternative_text) {
  return Stringize(CERT_GetOrgUnitName(&cert_handle->issuer), alternative_text);
}

string GetSubjectOrgName(X509Certificate::OSCertHandle cert_handle,
                         const string& alternative_text) {
  return Stringize(CERT_GetOrgName(&cert_handle->subject), alternative_text);
}

string GetSubjectOrgUnitName(X509Certificate::OSCertHandle cert_handle,
                             const string& alternative_text) {
  return Stringize(CERT_GetOrgUnitName(&cert_handle->subject),
                   alternative_text);
}

string GetSubjectCommonName(X509Certificate::OSCertHandle cert_handle,
                            const string& alternative_text) {
  return Stringize(CERT_GetCommonName(&cert_handle->subject), alternative_text);
}

bool GetTimes(X509Certificate::OSCertHandle cert_handle,
              base::Time* issued, base::Time* expires) {
  PRTime pr_issued, pr_expires;
  if (CERT_GetCertTimes(cert_handle, &pr_issued, &pr_expires) == SECSuccess) {
    *issued = crypto::PRTimeToBaseTime(pr_issued);
    *expires = crypto::PRTimeToBaseTime(pr_expires);
    return true;
  }
  return false;
}

string GetTitle(X509Certificate::OSCertHandle cert_handle) {
  return psm::GetCertTitle(cert_handle);
}

string GetIssuerName(X509Certificate::OSCertHandle cert_handle) {
  return psm::ProcessName(&cert_handle->issuer);
}

string GetSubjectName(X509Certificate::OSCertHandle cert_handle) {
  return psm::ProcessName(&cert_handle->subject);
}

void GetExtensions(
    const string& critical_label,
    const string& non_critical_label,
    X509Certificate::OSCertHandle cert_handle,
    Extensions* extensions) {
  if (cert_handle->extensions) {
    for (size_t i = 0; cert_handle->extensions[i] != NULL; ++i) {
      Extension extension;
      extension.name = psm::GetOIDText(&cert_handle->extensions[i]->id);
      extension.value = ProcessExtension(
          critical_label, non_critical_label, cert_handle->extensions[i]);
      extensions->push_back(extension);
    }
  }
}

string HashCertSHA256(X509Certificate::OSCertHandle cert_handle) {
  return HashCert(cert_handle, HASH_AlgSHA256, SHA256_LENGTH);
}

string HashCertSHA1(X509Certificate::OSCertHandle cert_handle) {
  return HashCert(cert_handle, HASH_AlgSHA1, SHA1_LENGTH);
}

string GetCMSString(const X509Certificate::OSCertHandles& cert_chain,
                    size_t start, size_t end) {
  crypto::ScopedPLArenaPool arena(PORT_NewArena(1024));
  DCHECK(arena.get());

  ScopedNSSCMSMessage message(NSS_CMSMessage_Create(arena.get()));
  DCHECK(message.get());

  // First, create SignedData with the certificate only (no chain).
  ScopedNSSCMSSignedData signed_data(NSS_CMSSignedData_CreateCertsOnly(
      message.get(), cert_chain[start], PR_FALSE));
  if (!signed_data.get()) {
    DLOG(ERROR) << "NSS_CMSSignedData_Create failed";
    return std::string();
  }
  // Add the rest of the chain (if any).
  for (size_t i = start + 1; i < end; ++i) {
    if (NSS_CMSSignedData_AddCertificate(signed_data.get(), cert_chain[i]) !=
        SECSuccess) {
      DLOG(ERROR) << "NSS_CMSSignedData_AddCertificate failed on " << i;
      return std::string();
    }
  }

  NSSCMSContentInfo *cinfo = NSS_CMSMessage_GetContentInfo(message.get());
  if (NSS_CMSContentInfo_SetContent_SignedData(
      message.get(), cinfo, signed_data.get()) == SECSuccess) {
    ignore_result(signed_data.release());
  } else {
    DLOG(ERROR) << "NSS_CMSMessage_GetContentInfo failed";
    return std::string();
  }

  SECItem cert_p7 = { siBuffer, NULL, 0 };
  NSSCMSEncoderContext *ecx = NSS_CMSEncoder_Start(message.get(), NULL, NULL,
                                                   &cert_p7, arena.get(), NULL,
                                                   NULL, NULL, NULL, NULL,
                                                   NULL);
  if (!ecx) {
    DLOG(ERROR) << "NSS_CMSEncoder_Start failed";
    return std::string();
  }

  if (NSS_CMSEncoder_Finish(ecx) != SECSuccess) {
    DLOG(ERROR) << "NSS_CMSEncoder_Finish failed";
    return std::string();
  }

  return string(reinterpret_cast<const char*>(cert_p7.data), cert_p7.len);
}

string ProcessSecAlgorithmSignature(X509Certificate::OSCertHandle cert_handle) {
  return ProcessSecAlgorithmInternal(&cert_handle->signature);
}

string ProcessSecAlgorithmSubjectPublicKey(
    X509Certificate::OSCertHandle cert_handle) {
  return ProcessSecAlgorithmInternal(
      &cert_handle->subjectPublicKeyInfo.algorithm);
}

string ProcessSecAlgorithmSignatureWrap(
    X509Certificate::OSCertHandle cert_handle) {
  return ProcessSecAlgorithmInternal(
      &cert_handle->signatureWrap.signatureAlgorithm);
}

string ProcessSubjectPublicKeyInfo(X509Certificate::OSCertHandle cert_handle) {
  return psm::ProcessSubjectPublicKeyInfo(&cert_handle->subjectPublicKeyInfo);
}

string ProcessRawBitsSignatureWrap(X509Certificate::OSCertHandle cert_handle) {
  return ProcessRawBits(cert_handle->signatureWrap.signature.data,
                        cert_handle->signatureWrap.signature.len);
}

}  // namespace x509_certificate_model
