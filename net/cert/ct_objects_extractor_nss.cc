// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_objects_extractor.h"

#include <cert.h>
#include <secasn1.h>
#include <secitem.h>
#include <secoid.h>

#include "base/lazy_instance.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/sha2.h"
#include "net/cert/asn1_util.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/signed_certificate_timestamp.h"

namespace net {

namespace ct {

namespace {

// Wrapper class to convert a X509Certificate::OSCertHandle directly
// into a CERTCertificate* usable with other NSS functions. This is used for
// platforms where X509Certificate::OSCertHandle refers to a different type
// than a CERTCertificate*.
struct NSSCertWrapper {
  explicit NSSCertWrapper(X509Certificate::OSCertHandle cert_handle);
  ~NSSCertWrapper() {}

  ScopedCERTCertificate cert;
};

NSSCertWrapper::NSSCertWrapper(X509Certificate::OSCertHandle cert_handle) {
#if defined(USE_NSS)
  cert.reset(CERT_DupCertificate(cert_handle));
#else
  SECItem der_cert;
  std::string der_data;
  if (!X509Certificate::GetDEREncoded(cert_handle, &der_data))
    return;
  der_cert.data =
      reinterpret_cast<unsigned char*>(const_cast<char*>(der_data.data()));
  der_cert.len = der_data.size();

  // Note: CERT_NewTempCertificate may return NULL if the certificate
  // shares a serial number with another cert issued by the same CA,
  // which is not supposed to happen.
  cert.reset(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &der_cert, NULL, PR_FALSE, PR_TRUE));
#endif
  DCHECK(cert.get() != NULL);
}

// The wire form of the OID 1.3.6.1.4.1.11129.2.4.2. See Section 3.3 of
// RFC6962.
const unsigned char kEmbeddedSCTOid[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
                                         0xD6, 0x79, 0x02, 0x04, 0x02};
const char kEmbeddedSCTDescription[] =
    "X.509v3 Certificate Transparency Embedded Signed Certificate Timestamp "
    "List";

// Initializes the necessary NSS internals for use with Certificate
// Transparency.
class CTInitSingleton {
 public:
  SECOidTag embedded_oid() const { return embedded_oid_; }

 private:
  friend struct base::DefaultLazyInstanceTraits<CTInitSingleton>;

  CTInitSingleton() : embedded_oid_(SEC_OID_UNKNOWN) {
    embedded_oid_ = RegisterOid(
        kEmbeddedSCTOid, sizeof(kEmbeddedSCTOid), kEmbeddedSCTDescription);
  }

  ~CTInitSingleton() {}

  SECOidTag RegisterOid(const unsigned char* oid,
                        unsigned int oid_len,
                        const char* description) {
    SECOidData oid_data;
    oid_data.oid.len = oid_len;
    oid_data.oid.data = const_cast<unsigned char*>(oid);
    oid_data.offset = SEC_OID_UNKNOWN;
    oid_data.desc = description;
    oid_data.mechanism = CKM_INVALID_MECHANISM;
    // Setting this to SUPPORTED_CERT_EXTENSION ensures that if a certificate
    // contains this extension with the critical bit set, NSS will not reject
    // it. However, because verification of this extension happens after NSS,
    // it is currently left as INVALID_CERT_EXTENSION.
    oid_data.supportedExtension = INVALID_CERT_EXTENSION;

    SECOidTag result = SECOID_AddEntry(&oid_data);
    CHECK_NE(SEC_OID_UNKNOWN, result);

    return result;
  }

  SECOidTag embedded_oid_;

  DISALLOW_COPY_AND_ASSIGN(CTInitSingleton);
};

base::LazyInstance<CTInitSingleton>::Leaky g_ct_singleton =
    LAZY_INSTANCE_INITIALIZER;

// Obtains the data for an X.509v3 certificate extension identified by |oid|
// and encoded as an OCTET STRING. Returns true if the extension was found,
// updating |ext_data| to be the extension data after removing the DER
// encoding of OCTET STRING.
bool GetOctetStringExtension(CERTCertificate* cert,
                             SECOidTag oid,
                             std::string* extension_data) {
  SECItem extension;
  SECStatus rv = CERT_FindCertExtension(cert, oid, &extension);
  if (rv != SECSuccess)
    return false;

  base::StringPiece raw_data(reinterpret_cast<char*>(extension.data),
                             extension.len);
  base::StringPiece parsed_data;
  if (!asn1::GetElement(&raw_data, asn1::kOCTETSTRING, &parsed_data) ||
      raw_data.size() > 0) { // Decoding failure or raw data left
    rv = SECFailure;
  } else {
    parsed_data.CopyToString(extension_data);
  }

  SECITEM_FreeItem(&extension, PR_FALSE);
  return rv == SECSuccess;
}

// Given a |cert|, extract the TBSCertificate from this certificate, also
// removing the X.509 extension with OID 1.3.6.1.4.1.11129.2.4.2 (that is,
// the embedded SCT)
bool ExtractTBSCertWithoutSCTs(CERTCertificate* cert,
                               std::string* to_be_signed) {
  SECOidData* oid = SECOID_FindOIDByTag(g_ct_singleton.Get().embedded_oid());
  if (!oid)
    return false;

  // This is a giant hack, due to the fact that NSS does not expose a good API
  // for simply removing certificate fields from existing certificates.
  CERTCertificate temp_cert;
  temp_cert = *cert;
  temp_cert.extensions = NULL;

  // Strip out the embedded SCT OID from the new certificate by directly
  // mutating the extensions in place.
  std::vector<CERTCertExtension*> new_extensions;
  if (cert->extensions) {
    for (CERTCertExtension** exts = cert->extensions; *exts; ++exts) {
      CERTCertExtension* ext = *exts;
      SECComparison result = SECITEM_CompareItem(&oid->oid, &ext->id);
      if (result != SECEqual)
        new_extensions.push_back(ext);
    }
  }
  if (!new_extensions.empty()) {
    new_extensions.push_back(NULL);
    temp_cert.extensions = &new_extensions[0];
  }

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));

  SECItem tbs_data;
  tbs_data.len = 0;
  tbs_data.data = NULL;
  void* result = SEC_ASN1EncodeItem(arena.get(),
                                    &tbs_data,
                                    &temp_cert,
                                    SEC_ASN1_GET(CERT_CertificateTemplate));
  if (!result)
    return false;

  to_be_signed->assign(reinterpret_cast<char*>(tbs_data.data), tbs_data.len);
  return true;
}

}  // namespace

bool ExtractEmbeddedSCTList(X509Certificate::OSCertHandle cert,
                            std::string* sct_list) {
  DCHECK(cert);

  NSSCertWrapper leaf_cert(cert);
  if (!leaf_cert.cert)
    return false;

  return GetOctetStringExtension(
      leaf_cert.cert.get(), g_ct_singleton.Get().embedded_oid(), sct_list);
}

bool GetPrecertLogEntry(X509Certificate::OSCertHandle leaf,
                        X509Certificate::OSCertHandle issuer,
                        LogEntry* result) {
  DCHECK(leaf);
  DCHECK(issuer);

  NSSCertWrapper leaf_cert(leaf);
  NSSCertWrapper issuer_cert(issuer);

  result->Reset();
  // XXX(rsleevi): This check may be overkill, since we should be able to
  // generate precerts for certs without the extension. For now, just a sanity
  // check to match the reference implementation.
  SECItem extension;
  SECStatus rv = CERT_FindCertExtension(
      leaf_cert.cert.get(), g_ct_singleton.Get().embedded_oid(), &extension);
  if (rv != SECSuccess)
    return false;
  SECITEM_FreeItem(&extension, PR_FALSE);

  std::string to_be_signed;
  if (!ExtractTBSCertWithoutSCTs(leaf_cert.cert.get(), &to_be_signed))
    return false;

  if (!issuer_cert.cert) {
    // This can happen when the issuer and leaf certs share the same serial
    // number and are from the same CA, which should never be the case
    // (but happened with bad test certs).
    VLOG(1) << "Issuer cert is null, cannot generate Precert entry.";
    return false;
  }

  SECKEYPublicKey* issuer_pub_key =
      SECKEY_ExtractPublicKey(&(issuer_cert.cert->subjectPublicKeyInfo));
  if (!issuer_pub_key) {
    VLOG(1) << "Could not extract issuer public key, "
            << "cannot generate Precert entry.";
    return false;
  }

  SECItem* encoded_issuer_pubKey =
      SECKEY_EncodeDERSubjectPublicKeyInfo(issuer_pub_key);
  if (!encoded_issuer_pubKey) {
    SECKEY_DestroyPublicKey(issuer_pub_key);
    return false;
  }

  result->type = ct::LogEntry::LOG_ENTRY_TYPE_PRECERT;
  result->tbs_certificate.swap(to_be_signed);

  crypto::SHA256HashString(
      base::StringPiece(reinterpret_cast<char*>(encoded_issuer_pubKey->data),
                        encoded_issuer_pubKey->len),
      result->issuer_key_hash.data,
      sizeof(result->issuer_key_hash.data));

  SECITEM_FreeItem(encoded_issuer_pubKey, PR_TRUE);
  SECKEY_DestroyPublicKey(issuer_pub_key);

  return true;
}

bool GetX509LogEntry(X509Certificate::OSCertHandle leaf, LogEntry* result) {
  DCHECK(leaf);

  std::string encoded;
  if (!X509Certificate::GetDEREncoded(leaf, &encoded))
    return false;

  result->Reset();
  result->type = ct::LogEntry::LOG_ENTRY_TYPE_X509;
  result->leaf_certificate.swap(encoded);
  return true;
}

}  // namespace ct

}  // namespace net
