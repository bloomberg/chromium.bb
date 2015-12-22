// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc.h"

#include <stdint.h>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/sha1.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc_whitelist.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/x509_certificate.h"
#include "url/url_canon.h"

#if defined(USE_NSS_CERTS) || defined(OS_IOS)
#include "net/cert/cert_verify_proc_nss.h"
#elif defined(USE_OPENSSL_CERTS) && !defined(OS_ANDROID)
#include "net/cert/cert_verify_proc_openssl.h"
#elif defined(OS_ANDROID)
#include "net/cert/cert_verify_proc_android.h"
#elif defined(OS_MACOSX)
#include "net/cert/cert_verify_proc_mac.h"
#elif defined(OS_WIN)
#include "net/cert/cert_verify_proc_win.h"
#else
#error Implement certificate verification.
#endif

namespace net {

namespace {

// Constants used to build histogram names
const char kLeafCert[] = "Leaf";
const char kIntermediateCert[] = "Intermediate";
const char kRootCert[] = "Root";
// Matches the order of X509Certificate::PublicKeyType
const char* const kCertTypeStrings[] = {
    "Unknown",
    "RSA",
    "DSA",
    "ECDSA",
    "DH",
    "ECDH"
};
// Histogram buckets for RSA/DSA/DH key sizes.
const int kRsaDsaKeySizes[] = {512, 768, 1024, 1536, 2048, 3072, 4096, 8192,
                               16384};
// Histogram buckets for ECDSA/ECDH key sizes. The list is based upon the FIPS
// 186-4 approved curves.
const int kEccKeySizes[] = {163, 192, 224, 233, 256, 283, 384, 409, 521, 571};

const char* CertTypeToString(int cert_type) {
  if (cert_type < 0 ||
      static_cast<size_t>(cert_type) >= arraysize(kCertTypeStrings)) {
    return "Unsupported";
  }
  return kCertTypeStrings[cert_type];
}

void RecordPublicKeyHistogram(const char* chain_position,
                              bool baseline_keysize_applies,
                              size_t size_bits,
                              X509Certificate::PublicKeyType cert_type) {
  std::string histogram_name =
      base::StringPrintf("CertificateType2.%s.%s.%s",
                         baseline_keysize_applies ? "BR" : "NonBR",
                         chain_position,
                         CertTypeToString(cert_type));
  // Do not use UMA_HISTOGRAM_... macros here, as it caches the Histogram
  // instance and thus only works if |histogram_name| is constant.
  base::HistogramBase* counter = NULL;

  // Histogram buckets are contingent upon the underlying algorithm being used.
  if (cert_type == X509Certificate::kPublicKeyTypeECDH ||
      cert_type == X509Certificate::kPublicKeyTypeECDSA) {
    // Typical key sizes match SECP/FIPS 186-3 recommendations for prime and
    // binary curves - which range from 163 bits to 571 bits.
    counter = base::CustomHistogram::FactoryGet(
        histogram_name,
        base::CustomHistogram::ArrayToCustomRanges(kEccKeySizes,
                                                   arraysize(kEccKeySizes)),
        base::HistogramBase::kUmaTargetedHistogramFlag);
  } else {
    // Key sizes < 1024 bits should cause errors, while key sizes > 16K are not
    // uniformly supported by the underlying cryptographic libraries.
    counter = base::CustomHistogram::FactoryGet(
        histogram_name,
        base::CustomHistogram::ArrayToCustomRanges(kRsaDsaKeySizes,
                                                   arraysize(kRsaDsaKeySizes)),
        base::HistogramBase::kUmaTargetedHistogramFlag);
  }
  counter->Add(size_bits);
}

// Returns true if |type| is |kPublicKeyTypeRSA| or |kPublicKeyTypeDSA|, and
// if |size_bits| is < 1024. Note that this means there may be false
// negatives: keys for other algorithms and which are weak will pass this
// test.
bool IsWeakKey(X509Certificate::PublicKeyType type, size_t size_bits) {
  switch (type) {
    case X509Certificate::kPublicKeyTypeRSA:
    case X509Certificate::kPublicKeyTypeDSA:
      return size_bits < 1024;
    default:
      return false;
  }
}

// Returns true if |cert| contains a known-weak key. Additionally, histograms
// the observed keys for future tightening of the definition of what
// constitutes a weak key.
bool ExaminePublicKeys(const scoped_refptr<X509Certificate>& cert,
                       bool should_histogram) {
  // The effective date of the CA/Browser Forum's Baseline Requirements -
  // 2012-07-01 00:00:00 UTC.
  const base::Time kBaselineEffectiveDate =
      base::Time::FromInternalValue(INT64_C(12985574400000000));
  // The effective date of the key size requirements from Appendix A, v1.1.5
  // 2014-01-01 00:00:00 UTC.
  const base::Time kBaselineKeysizeEffectiveDate =
      base::Time::FromInternalValue(INT64_C(13033008000000000));

  size_t size_bits = 0;
  X509Certificate::PublicKeyType type = X509Certificate::kPublicKeyTypeUnknown;
  bool weak_key = false;
  bool baseline_keysize_applies =
      cert->valid_start() >= kBaselineEffectiveDate &&
      cert->valid_expiry() >= kBaselineKeysizeEffectiveDate;

  X509Certificate::GetPublicKeyInfo(cert->os_cert_handle(), &size_bits, &type);
  if (should_histogram) {
    RecordPublicKeyHistogram(kLeafCert, baseline_keysize_applies, size_bits,
                             type);
  }
  if (IsWeakKey(type, size_bits))
    weak_key = true;

  const X509Certificate::OSCertHandles& intermediates =
      cert->GetIntermediateCertificates();
  for (size_t i = 0; i < intermediates.size(); ++i) {
    X509Certificate::GetPublicKeyInfo(intermediates[i], &size_bits, &type);
    if (should_histogram) {
      RecordPublicKeyHistogram(
          (i < intermediates.size() - 1) ? kIntermediateCert : kRootCert,
          baseline_keysize_applies,
          size_bits,
          type);
    }
    if (!weak_key && IsWeakKey(type, size_bits))
      weak_key = true;
  }

  return weak_key;
}

// Beginning with Ballot 118, ratified in the Baseline Requirements v1.2.1,
// CAs MUST NOT issue SHA-1 certificates beginning on 1 January 2016.
bool IsPastSHA1DeprecationDate(const X509Certificate& cert) {
  const base::Time& start = cert.valid_start();
  if (start.is_max() || start.is_null())
    return true;
  // 2016-01-01 00:00:00 UTC.
  const base::Time kSHA1DeprecationDate =
      base::Time::FromInternalValue(INT64_C(13096080000000000));
  return start >= kSHA1DeprecationDate;
}

}  // namespace

// static
CertVerifyProc* CertVerifyProc::CreateDefault() {
#if defined(USE_NSS_CERTS) || defined(OS_IOS)
  return new CertVerifyProcNSS();
#elif defined(USE_OPENSSL_CERTS) && !defined(OS_ANDROID)
  return new CertVerifyProcOpenSSL();
#elif defined(OS_ANDROID)
  return new CertVerifyProcAndroid();
#elif defined(OS_MACOSX)
  return new CertVerifyProcMac();
#elif defined(OS_WIN)
  return new CertVerifyProcWin();
#else
  return NULL;
#endif
}

CertVerifyProc::CertVerifyProc() {}

CertVerifyProc::~CertVerifyProc() {}

int CertVerifyProc::Verify(X509Certificate* cert,
                           const std::string& hostname,
                           const std::string& ocsp_response,
                           int flags,
                           CRLSet* crl_set,
                           const CertificateList& additional_trust_anchors,
                           CertVerifyResult* verify_result) {
  verify_result->Reset();
  verify_result->verified_cert = cert;

  if (IsBlacklisted(cert)) {
    verify_result->cert_status |= CERT_STATUS_REVOKED;
    return ERR_CERT_REVOKED;
  }

  // We do online revocation checking for EV certificates that aren't covered
  // by a fresh CRLSet.
  // TODO(rsleevi): http://crbug.com/142974 - Allow preferences to fully
  // disable revocation checking.
  if (flags & CertVerifier::VERIFY_EV_CERT)
    flags |= CertVerifier::VERIFY_REV_CHECKING_ENABLED_EV_ONLY;

  int rv = VerifyInternal(cert, hostname, ocsp_response, flags, crl_set,
                          additional_trust_anchors, verify_result);

  UMA_HISTOGRAM_BOOLEAN("Net.CertCommonNameFallback",
                        verify_result->common_name_fallback_used);
  if (!verify_result->is_issued_by_known_root) {
    UMA_HISTOGRAM_BOOLEAN("Net.CertCommonNameFallbackPrivateCA",
                          verify_result->common_name_fallback_used);
  }

  // This check is done after VerifyInternal so that VerifyInternal can fill
  // in the list of public key hashes.
  if (IsPublicKeyBlacklisted(verify_result->public_key_hashes)) {
    verify_result->cert_status |= CERT_STATUS_REVOKED;
    rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  std::vector<std::string> dns_names, ip_addrs;
  cert->GetSubjectAltName(&dns_names, &ip_addrs);
  if (HasNameConstraintsViolation(verify_result->public_key_hashes,
                                  cert->subject().common_name,
                                  dns_names,
                                  ip_addrs)) {
    verify_result->cert_status |= CERT_STATUS_NAME_CONSTRAINT_VIOLATION;
    rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  if (IsNonWhitelistedCertificate(*verify_result->verified_cert,
                                  verify_result->public_key_hashes)) {
    verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
    rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  // Check for weak keys in the entire verified chain.
  bool weak_key = ExaminePublicKeys(verify_result->verified_cert,
                                    verify_result->is_issued_by_known_root);

  if (weak_key) {
    verify_result->cert_status |= CERT_STATUS_WEAK_KEY;
    // Avoid replacing a more serious error, such as an OS/library failure,
    // by ensuring that if verification failed, it failed with a certificate
    // error.
    if (rv == OK || IsCertificateError(rv))
      rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  // Treat certificates signed using broken signature algorithms as invalid.
  if (verify_result->has_md2 || verify_result->has_md4) {
    verify_result->cert_status |= CERT_STATUS_INVALID;
    rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  if (verify_result->has_sha1)
    verify_result->cert_status |= CERT_STATUS_SHA1_SIGNATURE_PRESENT;

  // Flag certificates using weak signature algorithms.
  // The CA/Browser Forum Baseline Requirements (beginning with v1.2.1)
  // prohibits SHA-1 certificates from being issued beginning on
  // 1 January 2016. Ideally, all of SHA-1 in new certificates would be
  // disabled on this date, but enterprises need more time to transition.
  // As the risk is greatest for publicly trusted certificates, prevent
  // those certificates from being trusted from that date forward.
  if (verify_result->has_md5 ||
      (verify_result->has_sha1_leaf && verify_result->is_issued_by_known_root &&
       IsPastSHA1DeprecationDate(*cert))) {
    verify_result->cert_status |= CERT_STATUS_WEAK_SIGNATURE_ALGORITHM;
    // Avoid replacing a more serious error, such as an OS/library failure,
    // by ensuring that if verification failed, it failed with a certificate
    // error.
    if (rv == OK || IsCertificateError(rv))
      rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  // Flag certificates from publicly-trusted CAs that are issued to intranet
  // hosts. While the CA/Browser Forum Baseline Requirements (v1.1) permit
  // these to be issued until 1 November 2015, they represent a real risk for
  // the deployment of gTLDs and are being phased out ahead of the hard
  // deadline.
  if (verify_result->is_issued_by_known_root && IsHostnameNonUnique(hostname)) {
    verify_result->cert_status |= CERT_STATUS_NON_UNIQUE_NAME;
    // CERT_STATUS_NON_UNIQUE_NAME will eventually become a hard error. For
    // now treat it as a warning and do not map it to an error return value.
  }

  // Flag certificates using too long validity periods.
  if (verify_result->is_issued_by_known_root && HasTooLongValidity(*cert)) {
    verify_result->cert_status |= CERT_STATUS_VALIDITY_TOO_LONG;
    if (rv == OK)
      rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  return rv;
}

// static
bool CertVerifyProc::IsBlacklisted(X509Certificate* cert) {
  static const unsigned kComodoSerialBytes = 16;
  static const uint8_t kComodoSerials[][kComodoSerialBytes] = {
    // Not a real certificate. For testing only.
    {0x07,0x7a,0x59,0xbc,0xd5,0x34,0x59,0x60,0x1c,0xa6,0x90,0x72,0x67,0xa6,0xdd,0x1c},

    // The next nine certificates all expire on Fri Mar 14 23:59:59 2014.
    // Some serial numbers actually have a leading 0x00 byte required to
    // encode a positive integer in DER if the most significant bit is 0.
    // We omit the leading 0x00 bytes to make all serial numbers 16 bytes.

    // Subject: CN=mail.google.com
    // subjectAltName dNSName: mail.google.com, www.mail.google.com
    {0x04,0x7e,0xcb,0xe9,0xfc,0xa5,0x5f,0x7b,0xd0,0x9e,0xae,0x36,0xe1,0x0c,0xae,0x1e},
    // Subject: CN=global trustee
    // subjectAltName dNSName: global trustee
    // Note: not a CA certificate.
    {0xd8,0xf3,0x5f,0x4e,0xb7,0x87,0x2b,0x2d,0xab,0x06,0x92,0xe3,0x15,0x38,0x2f,0xb0},
    // Subject: CN=login.live.com
    // subjectAltName dNSName: login.live.com, www.login.live.com
    {0xb0,0xb7,0x13,0x3e,0xd0,0x96,0xf9,0xb5,0x6f,0xae,0x91,0xc8,0x74,0xbd,0x3a,0xc0},
    // Subject: CN=addons.mozilla.org
    // subjectAltName dNSName: addons.mozilla.org, www.addons.mozilla.org
    {0x92,0x39,0xd5,0x34,0x8f,0x40,0xd1,0x69,0x5a,0x74,0x54,0x70,0xe1,0xf2,0x3f,0x43},
    // Subject: CN=login.skype.com
    // subjectAltName dNSName: login.skype.com, www.login.skype.com
    {0xe9,0x02,0x8b,0x95,0x78,0xe4,0x15,0xdc,0x1a,0x71,0x0a,0x2b,0x88,0x15,0x44,0x47},
    // Subject: CN=login.yahoo.com
    // subjectAltName dNSName: login.yahoo.com, www.login.yahoo.com
    {0xd7,0x55,0x8f,0xda,0xf5,0xf1,0x10,0x5b,0xb2,0x13,0x28,0x2b,0x70,0x77,0x29,0xa3},
    // Subject: CN=www.google.com
    // subjectAltName dNSName: www.google.com, google.com
    {0xf5,0xc8,0x6a,0xf3,0x61,0x62,0xf1,0x3a,0x64,0xf5,0x4f,0x6d,0xc9,0x58,0x7c,0x06},
    // Subject: CN=login.yahoo.com
    // subjectAltName dNSName: login.yahoo.com
    {0x39,0x2a,0x43,0x4f,0x0e,0x07,0xdf,0x1f,0x8a,0xa3,0x05,0xde,0x34,0xe0,0xc2,0x29},
    // Subject: CN=login.yahoo.com
    // subjectAltName dNSName: login.yahoo.com
    {0x3e,0x75,0xce,0xd4,0x6b,0x69,0x30,0x21,0x21,0x88,0x30,0xae,0x86,0xa8,0x2a,0x71},
  };

  const std::string& serial_number = cert->serial_number();
  if (!serial_number.empty() && (serial_number[0] & 0x80) != 0) {
    // This is a negative serial number, which isn't technically allowed but
    // which probably happens. In order to avoid confusing a negative serial
    // number with a positive one once the leading zeros have been removed, we
    // disregard it.
    return false;
  }

  base::StringPiece serial(serial_number);
  // Remove leading zeros.
  while (serial.size() > 1 && serial[0] == 0)
    serial.remove_prefix(1);

  if (serial.size() == kComodoSerialBytes) {
    for (unsigned i = 0; i < arraysize(kComodoSerials); i++) {
      if (memcmp(kComodoSerials[i], serial.data(), kComodoSerialBytes) == 0) {
        UMA_HISTOGRAM_ENUMERATION("Net.SSLCertBlacklisted", i,
                                  arraysize(kComodoSerials) + 1);
        return true;
      }
    }
  }

  // CloudFlare revoked all certificates issued prior to April 2nd, 2014. Thus
  // all certificates where the CN ends with ".cloudflare.com" with a prior
  // issuance date are rejected.
  //
  // The old certs had a lifetime of five years, so this can be removed April
  // 2nd, 2019.
  const std::string& cn = cert->subject().common_name;
  static const char kCloudFlareCNSuffix[] = ".cloudflare.com";
  // kCloudFlareEpoch is the base::Time internal value for midnight at the
  // beginning of April 2nd, 2014, UTC.
  static const int64_t kCloudFlareEpoch = INT64_C(13040870400000000);
  if (cn.size() > arraysize(kCloudFlareCNSuffix) - 1 &&
      cn.compare(cn.size() - (arraysize(kCloudFlareCNSuffix) - 1),
                 arraysize(kCloudFlareCNSuffix) - 1,
                 kCloudFlareCNSuffix) == 0 &&
      cert->valid_start() < base::Time::FromInternalValue(kCloudFlareEpoch)) {
    return true;
  }

  return false;
}

// static
// NOTE: This implementation assumes and enforces that the hashes are SHA1.
bool CertVerifyProc::IsPublicKeyBlacklisted(
    const HashValueVector& public_key_hashes) {
  static const unsigned kNumSHA1Hashes = 14;
  static const uint8_t kSHA1Hashes[kNumSHA1Hashes][base::kSHA1Length] = {
    // Subject: CN=DigiNotar Root CA
    // Issuer: CN=Entrust.net x2 and self-signed
    {0x41, 0x0f, 0x36, 0x36, 0x32, 0x58, 0xf3, 0x0b, 0x34, 0x7d,
     0x12, 0xce, 0x48, 0x63, 0xe4, 0x33, 0x43, 0x78, 0x06, 0xa8},
    // Subject: CN=DigiNotar Cyber CA
    // Issuer: CN=GTE CyberTrust Global Root
    {0xc4, 0xf9, 0x66, 0x37, 0x16, 0xcd, 0x5e, 0x71, 0xd6, 0x95,
     0x0b, 0x5f, 0x33, 0xce, 0x04, 0x1c, 0x95, 0xb4, 0x35, 0xd1},
    // Subject: CN=DigiNotar Services 1024 CA
    // Issuer: CN=Entrust.net
    {0xe2, 0x3b, 0x8d, 0x10, 0x5f, 0x87, 0x71, 0x0a, 0x68, 0xd9,
     0x24, 0x80, 0x50, 0xeb, 0xef, 0xc6, 0x27, 0xbe, 0x4c, 0xa6},
    // Subject: CN=DigiNotar PKIoverheid CA Organisatie - G2
    // Issuer: CN=Staat der Nederlanden Organisatie CA - G2
    {0x7b, 0x2e, 0x16, 0xbc, 0x39, 0xbc, 0xd7, 0x2b, 0x45, 0x6e,
     0x9f, 0x05, 0x5d, 0x1d, 0xe6, 0x15, 0xb7, 0x49, 0x45, 0xdb},
    // Subject: CN=DigiNotar PKIoverheid CA Overheid en Bedrijven
    // Issuer: CN=Staat der Nederlanden Overheid CA
    {0xe8, 0xf9, 0x12, 0x00, 0xc6, 0x5c, 0xee, 0x16, 0xe0, 0x39,
     0xb9, 0xf8, 0x83, 0x84, 0x16, 0x61, 0x63, 0x5f, 0x81, 0xc5},
    // Issuer: CN=Trustwave Organization Issuing CA, Level 2
    // Covers two certificates, the latter of which expires Apr 15 21:09:30
    // 2021 GMT.
    {0xe1, 0x2d, 0x89, 0xf5, 0x6d, 0x22, 0x76, 0xf8, 0x30, 0xe6,
     0xce, 0xaf, 0xa6, 0x6c, 0x72, 0x5c, 0x0b, 0x41, 0xa9, 0x32},
    // Cyberoam CA certificate. Private key leaked, but this certificate would
    // only have been installed by Cyberoam customers. The certificate expires
    // in 2036, but we can probably remove in a couple of years (2014).
    {0xd9, 0xf5, 0xc6, 0xce, 0x57, 0xff, 0xaa, 0x39, 0xcc, 0x7e,
     0xd1, 0x72, 0xbd, 0x53, 0xe0, 0xd3, 0x07, 0x83, 0x4b, 0xd1},
    // Win32/Sirefef.gen!C generates fake certificates with this public key.
    {0xa4, 0xf5, 0x6e, 0x9e, 0x1d, 0x9a, 0x3b, 0x7b, 0x1a, 0xc3,
     0x31, 0xcf, 0x64, 0xfc, 0x76, 0x2c, 0xd0, 0x51, 0xfb, 0xa4},
    // Three retired intermediate certificates from Symantec. No compromise;
    // just for robustness. All expire May 17 23:59:59 2018.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=966060
    {0x68, 0x5e, 0xec, 0x0a, 0x39, 0xf6, 0x68, 0xae, 0x8f, 0xd8,
     0x96, 0x4f, 0x98, 0x74, 0x76, 0xb4, 0x50, 0x4f, 0xd2, 0xbe},
    {0x0e, 0x50, 0x2d, 0x4d, 0xd1, 0xe1, 0x60, 0x36, 0x8a, 0x31,
     0xf0, 0x6a, 0x81, 0x04, 0x31, 0xba, 0x6f, 0x72, 0xc0, 0x41},
    {0x93, 0xd1, 0x53, 0x22, 0x29, 0xcc, 0x2a, 0xbd, 0x21, 0xdf,
     0xf5, 0x97, 0xee, 0x32, 0x0f, 0xe4, 0x24, 0x6f, 0x3d, 0x0c},
    // C=IN, O=National Informatics Centre, CN=NIC CA 2011. Issued by
    // C=IN, O=India PKI, CN=CCA India 2011.
    // Expires March 11th 2016.
    {0x07, 0x7a, 0xc7, 0xde, 0x8d, 0xa5, 0x58, 0x64, 0x3a, 0x06,
     0xc5, 0x36, 0x9e, 0x55, 0x4f, 0xae, 0xb3, 0xdf, 0xa1, 0x66},
    // C=IN, O=National Informatics Centre, CN=NIC CA 2014. Issued by
    // C=IN, O=India PKI, CN=CCA India 2014.
    // Expires: March 5th, 2024.
    {0xe5, 0x8e, 0x31, 0x5b, 0xaa, 0xee, 0xaa, 0xc6, 0xe7, 0x2e,
     0xc9, 0x57, 0x36, 0x70, 0xca, 0x2f, 0x25, 0x4e, 0xc3, 0x47},
    // C=DE, O=Fraunhofer, OU=Fraunhofer Corporate PKI,
    // CN=Fraunhofer Service CA 2007.
    // Expires: Jun 30 2019.
    // No compromise, just for robustness. See
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1076940
    {0x38, 0x4d, 0x0c, 0x1d, 0xc4, 0x77, 0xa7, 0xb3, 0xf8, 0x67,
     0x86, 0xd0, 0x18, 0x51, 0x9f, 0x58, 0x9f, 0x1e, 0x9e, 0x25},
  };

  static const unsigned kNumSHA256Hashes = 4;
  static const uint8_t kSHA256Hashes[kNumSHA256Hashes][crypto::kSHA256Length] =
  {
    // Two intermediates issued by TurkTrust to *.ego.gov.tr and
    // e-islem.kktcmerkezbankasi.org. Expires July 6 2021 and August 5 2021,
    // respectively.
    // See http://googleonlinesecurity.blogspot.com/2013/01/enhancing-digital-certificate-security.html
    {0x3e, 0xdb, 0xd9, 0xac, 0xe6, 0x39, 0xba, 0x1a,
     0x2d, 0x4a, 0xd0, 0x47, 0x18, 0x71, 0x1f, 0xda,
     0x23, 0xe8, 0x59, 0xb2, 0xfb, 0xf5, 0xd1, 0x37,
     0xd4, 0x24, 0x04, 0x5e, 0x79, 0x19, 0xdf, 0xb9},
    {0xc1, 0x73, 0xf0, 0x62, 0x64, 0x56, 0xca, 0x85,
     0x4f, 0xf2, 0xa7, 0xf0, 0xb1, 0x33, 0xa7, 0xcf,
     0x4d, 0x02, 0x11, 0xe5, 0x52, 0xf2, 0x4b, 0x3e,
     0x33, 0xad, 0xe8, 0xc5, 0x9f, 0x0a, 0x42, 0x4c},

    // xs4all certificate. Expires March 19 2016.
    // https://raymii.org/s/blog/How_I_got_a_valid_SSL_certificate_for_my_ISPs_main_website.html
    {0xf2, 0xbb, 0xe0, 0x4c, 0x5d, 0xc7, 0x0d, 0x76,
     0x3e, 0x89, 0xc5, 0xa0, 0x52, 0x70, 0x48, 0xcd,
     0x9e, 0xcd, 0x39, 0xeb, 0x62, 0x1e, 0x20, 0x72,
     0xff, 0x9a, 0x5f, 0x84, 0x32, 0x57, 0x1a, 0xa0},

    // Japanese National Institute of Informatics intermediate. No suggestion
    // of compromise, it's just being discontinued.
    // Expires March 27th 2019
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1188582
    {0x5c, 0x72, 0x2c, 0xb7, 0x0f, 0xb3, 0x11, 0xf2,
     0x1e, 0x0d, 0xa0, 0xe7, 0xd1, 0x2e, 0xbc, 0x8e,
     0x05, 0xf6, 0x07, 0x96, 0xbc, 0x49, 0xcf, 0x51,
     0x18, 0x49, 0xd5, 0xbc, 0x62, 0x03, 0x03, 0x82},
  };

  for (unsigned i = 0; i < kNumSHA1Hashes; i++) {
    for (HashValueVector::const_iterator j = public_key_hashes.begin();
         j != public_key_hashes.end(); ++j) {
      if (j->tag == HASH_VALUE_SHA1 &&
          memcmp(j->data(), kSHA1Hashes[i], base::kSHA1Length) == 0) {
        return true;
      }
    }
  }

  for (unsigned i = 0; i < kNumSHA256Hashes; i++) {
    for (HashValueVector::const_iterator j = public_key_hashes.begin();
         j != public_key_hashes.end(); ++j) {
      if (j->tag == HASH_VALUE_SHA256 &&
          memcmp(j->data(), kSHA256Hashes[i], crypto::kSHA256Length) == 0) {
        return true;
      }
    }
  }

  return false;
}

static const size_t kMaxDomainLength = 18;

// CheckNameConstraints verifies that every name in |dns_names| is in one of
// the domains specified by |domains|. The |domains| array is terminated by an
// empty string.
static bool CheckNameConstraints(const std::vector<std::string>& dns_names,
                                 const char domains[][kMaxDomainLength]) {
  for (std::vector<std::string>::const_iterator i = dns_names.begin();
       i != dns_names.end(); ++i) {
    bool ok = false;
    url::CanonHostInfo host_info;
    const std::string dns_name = CanonicalizeHost(*i, &host_info);
    if (host_info.IsIPAddress())
      continue;

    const size_t registry_len = registry_controlled_domains::GetRegistryLength(
        dns_name,
        registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
        registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    // If the name is not in a known TLD, ignore it. This permits internal
    // names.
    if (registry_len == 0)
      continue;

    for (size_t j = 0; domains[j][0]; ++j) {
      const size_t domain_length = strlen(domains[j]);
      // The DNS name must have "." + domains[j] as a suffix.
      if (i->size() <= (1 /* period before domain */ + domain_length))
        continue;

      std::string suffix =
          base::ToLowerASCII(&(*i)[i->size() - domain_length - 1]);
      if (suffix[0] != '.')
        continue;
      if (memcmp(&suffix[1], domains[j], domain_length) != 0)
        continue;
      ok = true;
      break;
    }

    if (!ok)
      return false;
  }

  return true;
}

// PublicKeyDomainLimitation contains a SHA1, SPKI hash and a pointer to an
// array of fixed-length strings that contain the domains that the SPKI is
// allowed to issue for.
struct PublicKeyDomainLimitation {
  uint8_t public_key[base::kSHA1Length];
  const char (*domains)[kMaxDomainLength];
};

// static
bool CertVerifyProc::HasNameConstraintsViolation(
    const HashValueVector& public_key_hashes,
    const std::string& common_name,
    const std::vector<std::string>& dns_names,
    const std::vector<std::string>& ip_addrs) {
  static const char kDomainsANSSI[][kMaxDomainLength] = {
    "fr",  // France
    "gp",  // Guadeloupe
    "gf",  // Guyane
    "mq",  // Martinique
    "re",  // Réunion
    "yt",  // Mayotte
    "pm",  // Saint-Pierre et Miquelon
    "bl",  // Saint Barthélemy
    "mf",  // Saint Martin
    "wf",  // Wallis et Futuna
    "pf",  // Polynésie française
    "nc",  // Nouvelle Calédonie
    "tf",  // Terres australes et antarctiques françaises
    "",
  };

  static const char kDomainsIndiaCCA[][kMaxDomainLength] = {
    "gov.in",
    "nic.in",
    "ac.in",
    "rbi.org.in",
    "bankofindia.co.in",
    "ncode.in",
    "tcs.co.in",
    "",
  };

  static const char kDomainsTest[][kMaxDomainLength] = {
    "example.com",
    "",
  };

  static const PublicKeyDomainLimitation kLimits[] = {
      // C=FR, ST=France, L=Paris, O=PM/SGDN, OU=DCSSI,
      // CN=IGC/A/emailAddress=igca@sgdn.pm.gouv.fr
      {
          {0x79, 0x23, 0xd5, 0x8d, 0x0f, 0xe0, 0x3c, 0xe6, 0xab, 0xad, 0xae,
           0x27, 0x1a, 0x6d, 0x94, 0xf4, 0x14, 0xd1, 0xa8, 0x73},
          kDomainsANSSI,
      },
      // C=IN, O=India PKI, CN=CCA India 2007
      // Expires: July 4th 2015.
      {
          {0xfe, 0xe3, 0x95, 0x21, 0x2d, 0x5f, 0xea, 0xfc, 0x7e, 0xdc, 0xcf,
           0x88, 0x3f, 0x1e, 0xc0, 0x58, 0x27, 0xd8, 0xb8, 0xe4},
          kDomainsIndiaCCA,
      },
      // C=IN, O=India PKI, CN=CCA India 2011
      // Expires: March 11 2016.
      {
          {0xf1, 0x42, 0xf6, 0xa2, 0x7d, 0x29, 0x3e, 0xa8, 0xf9, 0x64, 0x52,
           0x56, 0xed, 0x07, 0xa8, 0x63, 0xf2, 0xdb, 0x1c, 0xdf},
          kDomainsIndiaCCA,
      },
      // C=IN, O=India PKI, CN=CCA India 2014
      // Expires: March 5 2024.
      {
          {0x36, 0x8c, 0x4a, 0x1e, 0x2d, 0xb7, 0x81, 0xe8, 0x6b, 0xed, 0x5a,
           0x0a, 0x42, 0xb8, 0xc5, 0xcf, 0x6d, 0xb3, 0x57, 0xe1},
          kDomainsIndiaCCA,
      },
      // Not a real certificate - just for testing. This is the SPKI hash of
      // the keys used in net/data/ssl/certificates/name_constraint_*.crt.
      {
          {0x48, 0x49, 0x4a, 0xc5, 0x5a, 0x3e, 0xcd, 0xc5, 0x62, 0x9f, 0xef,
           0x23, 0x14, 0xad, 0x05, 0xa9, 0x2a, 0x5c, 0x39, 0xc0},
          kDomainsTest,
      },
  };

  for (unsigned i = 0; i < arraysize(kLimits); ++i) {
    for (HashValueVector::const_iterator j = public_key_hashes.begin();
         j != public_key_hashes.end(); ++j) {
      if (j->tag == HASH_VALUE_SHA1 &&
          memcmp(j->data(), kLimits[i].public_key, base::kSHA1Length) == 0) {
        if (dns_names.empty() && ip_addrs.empty()) {
          std::vector<std::string> dns_names;
          dns_names.push_back(common_name);
          if (!CheckNameConstraints(dns_names, kLimits[i].domains))
            return true;
        } else {
          if (!CheckNameConstraints(dns_names, kLimits[i].domains))
            return true;
        }
      }
    }
  }

  return false;
}

// static
bool CertVerifyProc::HasTooLongValidity(const X509Certificate& cert) {
  const base::Time& start = cert.valid_start();
  const base::Time& expiry = cert.valid_expiry();
  if (start.is_max() || start.is_null() || expiry.is_max() ||
      expiry.is_null() || start > expiry) {
    return true;
  }

  base::Time::Exploded exploded_start;
  base::Time::Exploded exploded_expiry;
  cert.valid_start().UTCExplode(&exploded_start);
  cert.valid_expiry().UTCExplode(&exploded_expiry);

  if (exploded_expiry.year - exploded_start.year > 10)
    return true;

  int month_diff = (exploded_expiry.year - exploded_start.year) * 12 +
                   (exploded_expiry.month - exploded_start.month);

  // Add any remainder as a full month.
  if (exploded_expiry.day_of_month > exploded_start.day_of_month)
    ++month_diff;

  static const base::Time time_2012_07_01 =
      base::Time::FromUTCExploded({2012, 7, 0, 1, 0, 0, 0, 0});
  static const base::Time time_2015_04_01 =
      base::Time::FromUTCExploded({2015, 4, 0, 1, 0, 0, 0, 0});
  static const base::Time time_2019_07_01 =
      base::Time::FromUTCExploded({2019, 7, 0, 1, 0, 0, 0, 0});

  // For certificates issued before the BRs took effect.
  if (start < time_2012_07_01 && (month_diff > 120 || expiry > time_2019_07_01))
    return true;

  // For certificates issued after 1 July 2012: 60 months.
  if (start >= time_2012_07_01 && month_diff > 60)
    return true;

  // For certificates issued after 1 April 2015: 39 months.
  if (start >= time_2015_04_01 && month_diff > 39)
    return true;

  return false;
}

}  // namespace net
