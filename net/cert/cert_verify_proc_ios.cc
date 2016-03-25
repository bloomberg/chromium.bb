// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_ios.h"

#include <CommonCrypto/CommonDigest.h>
#include <Security/Security.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/openssl_ssl_util.h"

using base::ScopedCFTypeRef;

namespace net {

namespace {

int NetErrorFromOSStatus(OSStatus status) {
  switch (status) {
    case noErr:
      return OK;
    case errSecNotAvailable:
      return ERR_NOT_IMPLEMENTED;
    case errSecAuthFailed:
      return ERR_ACCESS_DENIED;
    default:
      return ERR_FAILED;
  }
}

// Creates a series of SecPolicyRefs to be added to a SecTrustRef used to
// validate a certificate for an SSL server. |hostname| contains the name of
// the SSL server that the certificate should be verified against. If
// successful, returns noErr, and stores the resultant array of SecPolicyRefs
// in |policies|.
OSStatus CreateTrustPolicies(ScopedCFTypeRef<CFArrayRef>* policies) {
  ScopedCFTypeRef<CFMutableArrayRef> local_policies(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!local_policies)
    return errSecAllocate;

  SecPolicyRef ssl_policy = SecPolicyCreateBasicX509();
  CFArrayAppendValue(local_policies, ssl_policy);
  CFRelease(ssl_policy);
  ssl_policy = SecPolicyCreateSSL(true, nullptr);
  CFArrayAppendValue(local_policies, ssl_policy);
  CFRelease(ssl_policy);

  policies->reset(local_policies.release());
  return noErr;
}

// Builds and evaluates a SecTrustRef for the certificate chain contained
// in |cert_array|, using the verification policies in |trust_policies|. On
// success, returns OK, and updates |trust_ref| and |trust_result|. On failure,
// no output parameters are modified.
//
// Note: An OK return does not mean that |cert_array| is trusted, merely that
// verification was performed successfully.
int BuildAndEvaluateSecTrustRef(CFArrayRef cert_array,
                                CFArrayRef trust_policies,
                                ScopedCFTypeRef<SecTrustRef>* trust_ref,
                                ScopedCFTypeRef<CFArrayRef>* verified_chain,
                                SecTrustResultType* trust_result) {
  SecTrustRef tmp_trust = nullptr;
  OSStatus status =
      SecTrustCreateWithCertificates(cert_array, trust_policies, &tmp_trust);
  if (status)
    return NetErrorFromOSStatus(status);
  ScopedCFTypeRef<SecTrustRef> scoped_tmp_trust(tmp_trust);

  if (TestRootCerts::HasInstance()) {
    status = TestRootCerts::GetInstance()->FixupSecTrustRef(tmp_trust);
    if (status)
      return NetErrorFromOSStatus(status);
  }

  SecTrustResultType tmp_trust_result;
  status = SecTrustEvaluate(tmp_trust, &tmp_trust_result);
  if (status)
    return NetErrorFromOSStatus(status);

  ScopedCFTypeRef<CFMutableArrayRef> tmp_verified_chain(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  const CFIndex chain_length = SecTrustGetCertificateCount(tmp_trust);
  for (CFIndex i = 0; i < chain_length; ++i) {
    SecCertificateRef chain_cert = SecTrustGetCertificateAtIndex(tmp_trust, i);
    CFArrayAppendValue(tmp_verified_chain, chain_cert);
  }

  trust_ref->swap(scoped_tmp_trust);
  *trust_result = tmp_trust_result;
  verified_chain->reset(tmp_verified_chain.release());
  return OK;
}

void GetCertChainInfo(CFArrayRef cert_chain, CertVerifyResult* verify_result) {
  DCHECK_LT(0, CFArrayGetCount(cert_chain));

  verify_result->has_md2 = false;
  verify_result->has_md4 = false;
  verify_result->has_md5 = false;
  verify_result->has_sha1 = false;
  verify_result->has_sha1_leaf = false;

  SecCertificateRef verified_cert = nullptr;
  std::vector<SecCertificateRef> verified_chain;
  for (CFIndex i = 0, count = CFArrayGetCount(cert_chain); i < count; ++i) {
    SecCertificateRef chain_cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(cert_chain, i)));
    if (i == 0) {
      verified_cert = chain_cert;
    } else {
      verified_chain.push_back(chain_cert);
    }

    std::string der_bytes;
    if (!X509Certificate::GetDEREncoded(chain_cert, &der_bytes))
      return;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(der_bytes.data());
    ScopedX509 x509_cert(d2i_X509(NULL, &bytes, der_bytes.size()));

    base::StringPiece spki_bytes;
    if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki_bytes))
      continue;

    HashValue sha1(HASH_VALUE_SHA1);
    CC_SHA1(spki_bytes.data(), spki_bytes.size(), sha1.data());
    verify_result->public_key_hashes.push_back(sha1);

    HashValue sha256(HASH_VALUE_SHA256);
    CC_SHA256(spki_bytes.data(), spki_bytes.size(), sha256.data());
    verify_result->public_key_hashes.push_back(sha256);

    int sig_alg = OBJ_obj2nid(x509_cert->sig_alg->algorithm);
    if (sig_alg == NID_md2WithRSAEncryption) {
      verify_result->has_md2 = true;
    } else if (sig_alg == NID_md4WithRSAEncryption) {
      verify_result->has_md4 = true;
    } else if (sig_alg == NID_md5WithRSAEncryption ||
               sig_alg == NID_md5WithRSA) {
      verify_result->has_md5 = true;
    } else if (sig_alg == NID_sha1WithRSAEncryption ||
               sig_alg == NID_dsaWithSHA || sig_alg == NID_dsaWithSHA1 ||
               sig_alg == NID_dsaWithSHA1_2 || sig_alg == NID_sha1WithRSA ||
               sig_alg == NID_ecdsa_with_SHA1) {
      verify_result->has_sha1 = true;
      if (i == 0)
        verify_result->has_sha1_leaf = true;
    }
  }
  if (!verified_cert) {
    NOTREACHED();
    return;
  }

  verify_result->verified_cert =
      X509Certificate::CreateFromHandle(verified_cert, verified_chain);
}

}  // namespace

CertVerifyProcIOS::CertVerifyProcIOS() {}

CertVerifyProcIOS::~CertVerifyProcIOS() {}

bool CertVerifyProcIOS::SupportsAdditionalTrustAnchors() const {
  return false;
}

bool CertVerifyProcIOS::SupportsOCSPStapling() const {
  return false;
}

int CertVerifyProcIOS::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    int flags,
    CRLSet* crl_set,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result) {
  ScopedCFTypeRef<CFArrayRef> trust_policies;
  OSStatus status = CreateTrustPolicies(&trust_policies);
  if (status)
    return NetErrorFromOSStatus(status);

  ScopedCFTypeRef<CFMutableArrayRef> cert_array(
      cert->CreateOSCertChainForCert());
  ScopedCFTypeRef<SecTrustRef> trust_ref;
  SecTrustResultType trust_result = kSecTrustResultDeny;
  ScopedCFTypeRef<CFArrayRef> final_chain;

  status = BuildAndEvaluateSecTrustRef(cert_array, trust_policies, &trust_ref,
                                       &final_chain, &trust_result);
  if (status)
    return NetErrorFromOSStatus(status);

  if (CFArrayGetCount(final_chain) == 0)
    return ERR_FAILED;

  GetCertChainInfo(final_chain, verify_result);

  // TODO(sleevi): Support CRLSet revocation.
  // TODO(svaldez): Add specific error codes for trust errors resulting from
  // expired/not-yet-valid certs.
  switch (trust_result) {
    case kSecTrustResultUnspecified:
    case kSecTrustResultProceed:
      break;
    case kSecTrustResultDeny:
      verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
    default:
      verify_result->cert_status |= CERT_STATUS_INVALID;
  }

  // Perform hostname verification independent of SecTrustEvaluate.
  if (!verify_result->verified_cert->VerifyNameMatch(
          hostname, &verify_result->common_name_fallback_used)) {
    verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;
  }

  verify_result->is_issued_by_known_root = false;

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  return OK;
}

}  // namespace net
