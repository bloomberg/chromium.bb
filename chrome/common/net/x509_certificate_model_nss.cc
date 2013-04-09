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
  return criticality + "\n" +
      psm::ProcessExtensionData(SECOID_FindOIDTag(&extension->id),
                                &extension->value);
}

////////////////////////////////////////////////////////////////////////////////
// NSS certificate export functions.

class FreeNSSCMSMessage {
 public:
  inline void operator()(NSSCMSMessage* x) const {
    NSS_CMSMessage_Destroy(x);
  }
};
typedef scoped_ptr_malloc<NSSCMSMessage, FreeNSSCMSMessage>
    ScopedNSSCMSMessage;

class FreeNSSCMSSignedData {
 public:
  inline void operator()(NSSCMSSignedData* x) const {
    NSS_CMSSignedData_Destroy(x);
  }
};
typedef scoped_ptr_malloc<NSSCMSSignedData, FreeNSSCMSSignedData>
    ScopedNSSCMSSignedData;

}  // namespace

namespace x509_certificate_model {

using net::X509Certificate;
using std::string;

string GetCertNameOrNickname(X509Certificate::OSCertHandle cert_handle) {
  string name = ProcessIDN(Stringize(CERT_GetCommonName(&cert_handle->subject),
                                     ""));
  if (!name.empty())
    return name;
  return GetNickname(cert_handle);
}

string GetNickname(X509Certificate::OSCertHandle cert_handle) {
  string name;
  if (cert_handle->nickname) {
    name = cert_handle->nickname;
    // Hack copied from mozilla: Cut off text before first :, which seems to
    // just be the token name.
    size_t colon_pos = name.find(':');
    if (colon_pos != string::npos)
      name = name.substr(colon_pos + 1);
  }
  return name;
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
  return "";
}

net::CertType GetType(X509Certificate::OSCertHandle cert_handle) {
    return psm::GetCertType(cert_handle);
}

string GetEmailAddress(X509Certificate::OSCertHandle cert_handle) {
  if (cert_handle->emailAddr)
    return cert_handle->emailAddr;
  return "";
}

void GetUsageStrings(X509Certificate::OSCertHandle cert_handle,
                     std::vector<string>* usages) {
  psm::GetCertUsageStrings(cert_handle, usages);
}

string GetKeyUsageString(X509Certificate::OSCertHandle cert_handle) {
  SECItem key_usage;
  key_usage.data = NULL;
  string key_usage_str;
  if (CERT_FindKeyUsageExtension(cert_handle, &key_usage) == SECSuccess) {
    key_usage_str = psm::ProcessKeyUsageBitString(&key_usage, ',');
    PORT_Free(key_usage.data);
  }
  return key_usage_str;
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

void GetEmailAddresses(X509Certificate::OSCertHandle cert_handle,
                       std::vector<string>* email_addresses) {
  for (const char* addr = CERT_GetFirstEmailAddress(cert_handle);
       addr; addr = CERT_GetNextEmailAddress(cert_handle, addr)) {
    // The first email addr (from Subject) may be duplicated in Subject
    // Alternative Name, so check subsequent addresses are not equal to the
    // first one before adding to the list.
    if (!email_addresses->size() || (*email_addresses)[0] != addr)
      email_addresses->push_back(addr);
  }
}

void GetNicknameStringsFromCertList(
    const std::vector<scoped_refptr<X509Certificate> >& certs,
    const string& cert_expired,
    const string& cert_not_yet_valid,
    std::vector<string>* nick_names) {
  CERTCertList* cert_list = CERT_NewCertList();
  for (size_t i = 0; i < certs.size(); ++i) {
    CERT_AddCertToListTail(
        cert_list,
        CERT_DupCertificate(certs[i]->os_cert_handle()));
  }
  // Would like to use CERT_GetCertNicknameWithValidity on each cert
  // individually instead of having to build a CERTCertList for this, but that
  // function is not exported.
  CERTCertNicknames* cert_nicknames = CERT_NicknameStringsFromCertList(
      cert_list,
      const_cast<char*>(cert_expired.c_str()),
      const_cast<char*>(cert_not_yet_valid.c_str()));
  DCHECK_EQ(cert_nicknames->numnicknames,
            static_cast<int>(certs.size()));

  for (int i = 0; i < cert_nicknames->numnicknames; ++i)
    nick_names->push_back(cert_nicknames->nicknames[i]);

  CERT_FreeNicknames(cert_nicknames);
  CERT_DestroyCertList(cert_list);
}

// For background see this discussion on dev-tech-crypto.lists.mozilla.org:
// http://web.archiveorange.com/archive/v/6JJW7E40sypfZGtbkzxX
//
// NOTE: This function relies on the convention that the same PKCS#11 ID
// is shared between a certificate and its associated private and public
// keys.  I tried to implement this with PK11_GetLowLevelKeyIDForCert(),
// but that always returns NULL on Chrome OS for me.
std::string GetPkcs11Id(net::X509Certificate::OSCertHandle cert_handle) {
  std::string pkcs11_id;
  SECKEYPrivateKey *priv_key = PK11_FindKeyByAnyCert(cert_handle,
                                                     NULL /* wincx */);
  if (priv_key) {
    // Get the CKA_ID attribute for a key.
    SECItem* sec_item = PK11_GetLowLevelKeyIDForPrivateKey(priv_key);
    if (sec_item) {
      pkcs11_id = base::HexEncode(sec_item->data, sec_item->len);
      SECITEM_FreeItem(sec_item, PR_TRUE);
    }
    SECKEY_DestroyPrivateKey(priv_key);
  }
  return pkcs11_id;
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

void GetCertChainFromCert(X509Certificate::OSCertHandle cert_handle,
                          X509Certificate::OSCertHandles* cert_handles) {
  CERTCertList* cert_list =
      CERT_GetCertChainFromCert(cert_handle, PR_Now(), certUsageSSLServer);
  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node)) {
    cert_handles->push_back(CERT_DupCertificate(node->cert));
  }
  CERT_DestroyCertList(cert_list);
}

void DestroyCertChain(X509Certificate::OSCertHandles* cert_handles) {
  for (X509Certificate::OSCertHandles::iterator i(cert_handles->begin());
       i != cert_handles->end(); ++i)
    CERT_DestroyCertificate(*i);
  cert_handles->clear();
}

string GetDerString(X509Certificate::OSCertHandle cert_handle) {
  return string(reinterpret_cast<const char*>(cert_handle->derCert.data),
                cert_handle->derCert.len);
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
    return "";
  }
  // Add the rest of the chain (if any).
  for (size_t i = start + 1; i < end; ++i) {
    if (NSS_CMSSignedData_AddCertificate(signed_data.get(), cert_chain[i]) !=
        SECSuccess) {
      DLOG(ERROR) << "NSS_CMSSignedData_AddCertificate failed on " << i;
      return "";
    }
  }

  NSSCMSContentInfo *cinfo = NSS_CMSMessage_GetContentInfo(message.get());
  if (NSS_CMSContentInfo_SetContent_SignedData(
      message.get(), cinfo, signed_data.get()) == SECSuccess) {
    ignore_result(signed_data.release());
  } else {
    DLOG(ERROR) << "NSS_CMSMessage_GetContentInfo failed";
    return "";
  }

  SECItem cert_p7 = { siBuffer, NULL, 0 };
  NSSCMSEncoderContext *ecx = NSS_CMSEncoder_Start(message.get(), NULL, NULL,
                                                   &cert_p7, arena.get(), NULL,
                                                   NULL, NULL, NULL, NULL,
                                                   NULL);
  if (!ecx) {
    DLOG(ERROR) << "NSS_CMSEncoder_Start failed";
    return "";
  }

  if (NSS_CMSEncoder_Finish(ecx) != SECSuccess) {
    DLOG(ERROR) << "NSS_CMSEncoder_Finish failed";
    return "";
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

void RegisterDynamicOids() {
  psm::RegisterDynamicOids();
}

}  // namespace x509_certificate_model
