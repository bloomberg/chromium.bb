// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(eroman): Because VerifySignedData() is only implemented for BoringSSL
// these tests also depend on BoringSSL.
#if defined(USE_OPENSSL)

#include "net/cert/internal/verify_certificate_chain.h"

#include "net/cert/internal/parse_certificate.h"
#include "net/cert/internal/signature_policy.h"
#include "net/der/input.h"

// Disable tests that require DSA signatures (DSA signatures are intentionally
// unsupported). Custom versions of the DSA tests are defined below which expect
// verification to fail.
#define Section1ValidDSASignaturesTest4 DISABLED_Section1ValidDSASignaturesTest4
#define Section1ValidDSAParameterInheritanceTest5 \
  DISABLED_Section1ValidDSAParameterInheritanceTest5

// TODO(mattm): these require CRL support:
#define Section7InvalidkeyUsageCriticalcRLSignFalseTest4 \
  DISABLED_Section7InvalidkeyUsageCriticalcRLSignFalseTest4
#define Section7InvalidkeyUsageNotCriticalcRLSignFalseTest5 \
  DISABLED_Section7InvalidkeyUsageNotCriticalcRLSignFalseTest5

#include "net/cert/internal/nist_pkits_unittest.h"

namespace net {

namespace {

// Adds the certificate |cert_der| as a trust anchor to |trust_store|.
void AddCertificateToTrustStore(const std::string& cert_der,
                                TrustStore* trust_store) {
  ParsedCertificate cert;
  ASSERT_TRUE(ParseCertificate(der::Input(&cert_der), &cert));

  ParsedTbsCertificate tbs;
  ASSERT_TRUE(ParseTbsCertificate(cert.tbs_certificate_tlv, &tbs));
  TrustAnchor anchor = {tbs.spki_tlv.AsString(), tbs.subject_tlv.AsString()};
  trust_store->anchors.push_back(anchor);
}

class VerifyCertificateChainPkitsTestDelegate {
 public:
  static bool Verify(std::vector<std::string> cert_ders,
                     std::vector<std::string> crl_ders) {
    if (cert_ders.empty()) {
      ADD_FAILURE() << "cert_ders is empty";
      return false;
    }
    // First entry in the PKITS chain is the trust anchor.
    TrustStore trust_store;
    AddCertificateToTrustStore(cert_ders[0], &trust_store);

    // PKITS lists chains from trust anchor to target, VerifyCertificateChain
    // takes them starting with the target and not including the trust anchor.
    std::vector<der::Input> input_chain;
    for (size_t i = cert_ders.size() - 1; i > 0; --i)
      input_chain.push_back(der::Input(&cert_ders[i]));

    SimpleSignaturePolicy signature_policy(1024);

    // Run all tests at the time the PKITS was published.
    der::GeneralizedTime time = {2011, 4, 15, 0, 0, 0};

    return VerifyCertificateChain(input_chain, trust_store, &signature_policy,
                                  time);
  }
};

}  // namespace

class PkitsTest01SignatureVerificationCustom
    : public PkitsTest<VerifyCertificateChainPkitsTestDelegate> {};

// Modified version of 4.1.4 Valid DSA Signatures Test4
TEST_F(PkitsTest01SignatureVerificationCustom,
       Section1ValidDSASignaturesTest4Custom) {
  const char* const certs[] = {"TrustAnchorRootCertificate", "DSACACert",
                               "ValidDSASignaturesTest4EE"};
  const char* const crls[] = {"TrustAnchorRootCRL", "DSACACRL"};
  // DSA signatures are intentionally unsupported.
  ASSERT_FALSE(this->Verify(certs, crls));
}

// Modified version of 4.1.5 Valid DSA Parameter Inheritance Test5
TEST_F(PkitsTest01SignatureVerificationCustom,
       Section1ValidDSAParameterInheritanceTest5Custom) {
  const char* const certs[] = {"TrustAnchorRootCertificate", "DSACACert",
                               "DSAParametersInheritedCACert",
                               "ValidDSAParameterInheritanceTest5EE"};
  const char* const crls[] = {"TrustAnchorRootCRL", "DSACACRL",
                              "DSAParametersInheritedCACRL"};
  // DSA signatures are intentionally unsupported.
  ASSERT_FALSE(this->Verify(certs, crls));
}

INSTANTIATE_TYPED_TEST_CASE_P(VerifyCertificateChain,
                              PkitsTest01SignatureVerification,
                              VerifyCertificateChainPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_CASE_P(VerifyCertificateChain,
                              PkitsTest02ValidityPeriods,
                              VerifyCertificateChainPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_CASE_P(VerifyCertificateChain,
                              PkitsTest03VerifyingNameChaining,
                              VerifyCertificateChainPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_CASE_P(VerifyCertificateChain,
                              PkitsTest06VerifyingBasicConstraints,
                              VerifyCertificateChainPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_CASE_P(VerifyCertificateChain,
                              PkitsTest07KeyUsage,
                              VerifyCertificateChainPkitsTestDelegate);
INSTANTIATE_TYPED_TEST_CASE_P(VerifyCertificateChain,
                              PkitsTest16PrivateCertificateExtensions,
                              VerifyCertificateChainPkitsTestDelegate);

// TODO(mattm): CRL support: PkitsTest04BasicCertificateRevocationTests,
// PkitsTest05VerifyingPathswithSelfIssuedCertificates,
// PkitsTest14DistributionPoints, PkitsTest15DeltaCRLs

// TODO(mattm): Certificate Policies support: PkitsTest08CertificatePolicies,
// PkitsTest09RequireExplicitPolicy PkitsTest10PolicyMappings,
// PkitsTest11InhibitPolicyMapping, PkitsTest12InhibitAnyPolicy

// TODO(mattm): Name Constraints support: PkitsTest13NameConstraints

}  // namespace net

#endif  // USE_OPENSSL
