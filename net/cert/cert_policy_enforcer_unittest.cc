// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_policy_enforcer.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/version.h"
#include "net/base/test_data_directory.h"
#include "net/cert/ct_ev_whitelist.h"
#include "net/cert/ct_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class DummyEVCertsWhitelist : public ct::EVCertsWhitelist {
 public:
  DummyEVCertsWhitelist(bool is_valid_response, bool contains_hash_response)
      : canned_is_valid_(is_valid_response),
        canned_contains_response_(contains_hash_response) {}

  bool IsValid() const override { return canned_is_valid_; }

  bool ContainsCertificateHash(
      const std::string& certificate_hash) const override {
    return canned_contains_response_;
  }

  base::Version Version() const override { return base::Version(); }

 protected:
  ~DummyEVCertsWhitelist() override {}

 private:
  bool canned_is_valid_;
  bool canned_contains_response_;
};

class CertPolicyEnforcerTest : public ::testing::Test {
 public:
  void SetUp() override {
    policy_enforcer_.reset(new CertPolicyEnforcer(true));

    std::string der_test_cert(ct::GetDerEncodedX509Cert());
    chain_ = X509Certificate::CreateFromBytes(der_test_cert.data(),
                                              der_test_cert.size());
    ASSERT_TRUE(chain_.get());
  }

  void FillResultWithSCTsOfOrigin(
      ct::SignedCertificateTimestamp::Origin desired_origin,
      int num_scts,
      ct::CTVerifyResult* result) {
    for (int i = 0; i < num_scts; ++i) {
      scoped_refptr<ct::SignedCertificateTimestamp> sct(
          new ct::SignedCertificateTimestamp());
      sct->origin = desired_origin;
      result->verified_scts.push_back(sct);
    }
  }

 protected:
  scoped_ptr<CertPolicyEnforcer> policy_enforcer_;
  scoped_refptr<X509Certificate> chain_;
};

TEST_F(CertPolicyEnforcerTest, ConformsToCTEVPolicyWithNonEmbeddedSCTs) {
  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(
      ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION, 2, &result);

  EXPECT_TRUE(policy_enforcer_->DoesConformToCTEVPolicy(chain_.get(), nullptr,
                                                        result, BoundNetLog()));
}

TEST_F(CertPolicyEnforcerTest, ConformsToCTEVPolicyWithEmbeddedSCTs) {
  // This chain_ is valid for 10 years - over 121 months - so requires 5 SCTs.
  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 5,
                             &result);

  EXPECT_TRUE(policy_enforcer_->DoesConformToCTEVPolicy(chain_.get(), nullptr,
                                                        result, BoundNetLog()));
}

TEST_F(CertPolicyEnforcerTest, DoesNotConformToCTEVPolicyNotEnoughSCTs) {
  scoped_refptr<ct::EVCertsWhitelist> non_including_whitelist(
      new DummyEVCertsWhitelist(true, false));
  // This chain_ is valid for 10 years - over 121 months - so requires 5 SCTs.
  // However, as there are only two logs, two SCTs will be required - supply one
  // to guarantee the test fails.
  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 1,
                             &result);

  EXPECT_FALSE(policy_enforcer_->DoesConformToCTEVPolicy(
      chain_.get(), non_including_whitelist.get(), result, BoundNetLog()));

  // ... but should be OK if whitelisted.
  scoped_refptr<ct::EVCertsWhitelist> whitelist(
      new DummyEVCertsWhitelist(true, true));
  EXPECT_TRUE(policy_enforcer_->DoesConformToCTEVPolicy(
      chain_.get(), whitelist.get(), result, BoundNetLog()));
}

TEST_F(CertPolicyEnforcerTest, DoesNotEnforceCTPolicyIfNotRequired) {
  scoped_ptr<CertPolicyEnforcer> enforcer(new CertPolicyEnforcer(false));

  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 1,
                             &result);
  // Expect true despite the chain not having enough SCTs as the policy
  // is not enforced.
  EXPECT_TRUE(enforcer->DoesConformToCTEVPolicy(chain_.get(), nullptr, result,
                                                BoundNetLog()));
}

TEST_F(CertPolicyEnforcerTest, DoesNotConformToPolicyInvalidDates) {
  scoped_refptr<X509Certificate> no_valid_dates_cert(new X509Certificate(
      "subject", "issuer", base::Time(), base::Time::Now()));
  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 5,
                             &result);
  EXPECT_FALSE(policy_enforcer_->DoesConformToCTEVPolicy(
      no_valid_dates_cert.get(), nullptr, result, BoundNetLog()));
  // ... but should be OK if whitelisted.
  scoped_refptr<ct::EVCertsWhitelist> whitelist(
      new DummyEVCertsWhitelist(true, true));
  EXPECT_TRUE(policy_enforcer_->DoesConformToCTEVPolicy(
      chain_.get(), whitelist.get(), result, BoundNetLog()));
}

TEST_F(CertPolicyEnforcerTest,
       ConformsToPolicyExactNumberOfSCTsForValidityPeriod) {
  // Test multiple validity periods: Over 27 months, Over 15 months (but less
  // than 27 months),
  // Less than 15 months.
  const size_t validity_period[] = {12, 19, 30, 50};
  const size_t needed_scts[] = {2, 3, 4, 5};

  for (int i = 0; i < 3; ++i) {
    size_t curr_validity = validity_period[i];
    scoped_refptr<X509Certificate> cert(new X509Certificate(
        "subject", "issuer", base::Time::Now(),
        base::Time::Now() + base::TimeDelta::FromDays(31 * curr_validity)));
    size_t curr_required_scts = needed_scts[i];
    ct::CTVerifyResult result;
    for (size_t j = 0; j < curr_required_scts - 1; ++j) {
      FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                                 1, &result);
      EXPECT_FALSE(policy_enforcer_->DoesConformToCTEVPolicy(
          cert.get(), nullptr, result, BoundNetLog()))
          << " for: " << curr_validity << " and " << curr_required_scts
          << " scts=" << result.verified_scts.size() << " j=" << j;
    }
    FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 1,
                               &result);
    EXPECT_TRUE(policy_enforcer_->DoesConformToCTEVPolicy(
        cert.get(), nullptr, result, BoundNetLog()));
  }
}

TEST_F(CertPolicyEnforcerTest, ConformsToPolicyByEVWhitelistPresence) {
  scoped_refptr<ct::EVCertsWhitelist> whitelist(
      new DummyEVCertsWhitelist(true, true));

  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 1,
                             &result);
  EXPECT_TRUE(policy_enforcer_->DoesConformToCTEVPolicy(
      chain_.get(), whitelist.get(), result, BoundNetLog()));
}

TEST_F(CertPolicyEnforcerTest, IgnoresInvalidEVWhitelist) {
  scoped_refptr<ct::EVCertsWhitelist> whitelist(
      new DummyEVCertsWhitelist(false, true));

  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 1,
                             &result);
  EXPECT_FALSE(policy_enforcer_->DoesConformToCTEVPolicy(
      chain_.get(), whitelist.get(), result, BoundNetLog()));
}

TEST_F(CertPolicyEnforcerTest, IgnoresNullEVWhitelist) {
  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 1,
                             &result);
  EXPECT_FALSE(policy_enforcer_->DoesConformToCTEVPolicy(
      chain_.get(), nullptr, result, BoundNetLog()));
}

}  // namespace

}  // namespace net
