// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_policy_enforcer.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/version.h"
#include "crypto/sha2.h"
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

const char kGoogleAviatorLogID[] =
    "\x68\xf6\x98\xf8\x1f\x64\x82\xbe\x3a\x8c\xee\xb9\x28\x1d\x4c\xfc\x71\x51"
    "\x5d\x67\x93\xd4\x44\xd1\x0a\x67\xac\xbb\x4f\x4f\xfb\xc4";
static_assert(arraysize(kGoogleAviatorLogID) - 1 == crypto::kSHA256Length,
              "Incorrect log ID length.");

class CTPolicyEnforcerTest : public ::testing::Test {
 public:
  void SetUp() override {
    policy_enforcer_.reset(new CTPolicyEnforcer);

    std::string der_test_cert(ct::GetDerEncodedX509Cert());
    chain_ = X509Certificate::CreateFromBytes(der_test_cert.data(),
                                              der_test_cert.size());
    ASSERT_TRUE(chain_.get());
    google_log_id_ = std::string(kGoogleAviatorLogID, crypto::kSHA256Length);
    non_google_log_id_.assign(crypto::kSHA256Length, 'A');
  }

  void FillResultWithSCTsOfOrigin(
      ct::SignedCertificateTimestamp::Origin desired_origin,
      size_t num_scts,
      const std::vector<std::string>& desired_log_keys,
      bool timestamp_past_enforcement_date,
      ct::CTVerifyResult* result) {
    for (size_t i = 0; i < num_scts; ++i) {
      scoped_refptr<ct::SignedCertificateTimestamp> sct(
          new ct::SignedCertificateTimestamp());
      sct->origin = desired_origin;
      if (i < desired_log_keys.size())
        sct->log_id = desired_log_keys[i];
      else
        sct->log_id = non_google_log_id_;

      if (timestamp_past_enforcement_date)
        sct->timestamp =
            base::Time::FromUTCExploded({2015, 8, 0, 15, 0, 0, 0, 0});
      else
        sct->timestamp =
            base::Time::FromUTCExploded({2015, 6, 0, 15, 0, 0, 0, 0});

      result->verified_scts.push_back(sct);
    }
  }

  void FillResultWithSCTsOfOrigin(
      ct::SignedCertificateTimestamp::Origin desired_origin,
      size_t num_scts,
      ct::CTVerifyResult* result) {
    std::vector<std::string> desired_log_ids;
    desired_log_ids.push_back(google_log_id_);
    FillResultWithSCTsOfOrigin(desired_origin, num_scts, desired_log_ids, true,
                               result);
  }

  void FillResultWithRepeatedLogID(const std::string& desired_id,
                                   size_t num_scts,
                                   bool timestamp_past_enforcement_date,
                                   ct::CTVerifyResult* result) {
    std::vector<std::string> desired_log_ids(num_scts, desired_id);

    FillResultWithSCTsOfOrigin(
        ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION, num_scts,
        desired_log_ids, timestamp_past_enforcement_date, result);
  }

  void CheckCertificateCompliesWithExactNumberOfEmbeddedSCTs(
      const base::Time& start,
      const base::Time& end,
      size_t required_scts) {
    scoped_refptr<X509Certificate> cert(
        new X509Certificate("subject", "issuer", start, end));
    ct::CTVerifyResult result;

    for (size_t i = 0; i < required_scts - 1; ++i) {
      FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                                 1, std::vector<std::string>(), false, &result);
      EXPECT_FALSE(policy_enforcer_->DoesConformToCTEVPolicy(
          cert.get(), nullptr, result, BoundNetLog()))
          << " for: " << (end - start).InDays() << " and " << required_scts
          << " scts=" << result.verified_scts.size() << " i=" << i;
    }
    FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 1,
                               std::vector<std::string>(), false, &result);
    EXPECT_TRUE(policy_enforcer_->DoesConformToCTEVPolicy(
        cert.get(), nullptr, result, BoundNetLog()))
        << " for: " << (end - start).InDays() << " and " << required_scts
        << " scts=" << result.verified_scts.size();
  }

 protected:
  scoped_ptr<CTPolicyEnforcer> policy_enforcer_;
  scoped_refptr<X509Certificate> chain_;
  std::string google_log_id_;
  std::string non_google_log_id_;
};

TEST_F(CTPolicyEnforcerTest,
       DoesNotConformToCTEVPolicyNotEnoughDiverseSCTsAllGoogle) {
  ct::CTVerifyResult result;
  FillResultWithRepeatedLogID(google_log_id_, 2, true, &result);

  EXPECT_FALSE(policy_enforcer_->DoesConformToCTEVPolicy(
      chain_.get(), nullptr, result, BoundNetLog()));
}

TEST_F(CTPolicyEnforcerTest,
       DoesNotConformToCTEVPolicyNotEnoughDiverseSCTsAllNonGoogle) {
  ct::CTVerifyResult result;
  FillResultWithRepeatedLogID(non_google_log_id_, 2, true, &result);

  EXPECT_FALSE(policy_enforcer_->DoesConformToCTEVPolicy(
      chain_.get(), nullptr, result, BoundNetLog()));
}

TEST_F(CTPolicyEnforcerTest, ConformsToCTEVPolicyIfSCTBeforeEnforcementDate) {
  ct::CTVerifyResult result;
  FillResultWithRepeatedLogID(non_google_log_id_, 2, false, &result);

  EXPECT_TRUE(policy_enforcer_->DoesConformToCTEVPolicy(chain_.get(), nullptr,
                                                        result, BoundNetLog()));
}

TEST_F(CTPolicyEnforcerTest, ConformsToCTEVPolicyWithNonEmbeddedSCTs) {
  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(
      ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION, 2, &result);

  EXPECT_TRUE(policy_enforcer_->DoesConformToCTEVPolicy(chain_.get(), nullptr,
                                                        result, BoundNetLog()));
}

TEST_F(CTPolicyEnforcerTest, ConformsToCTEVPolicyWithEmbeddedSCTs) {
  // This chain_ is valid for 10 years - over 121 months - so requires 5 SCTs.
  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 5,
                             &result);

  EXPECT_TRUE(policy_enforcer_->DoesConformToCTEVPolicy(chain_.get(), nullptr,
                                                        result, BoundNetLog()));
}

TEST_F(CTPolicyEnforcerTest, DoesNotConformToCTEVPolicyNotEnoughSCTs) {
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

TEST_F(CTPolicyEnforcerTest, DoesNotConformToPolicyInvalidDates) {
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

TEST_F(CTPolicyEnforcerTest,
       ConformsToPolicyExactNumberOfSCTsForValidityPeriod) {
  // Test multiple validity periods
  const struct TestData {
    base::Time validity_start;
    base::Time validity_end;
    size_t scts_required;
  } kTestData[] = {{// Cert valid for 14 months, needs 2 SCTs.
                    base::Time::FromUTCExploded({2015, 3, 0, 25, 11, 25, 0, 0}),
                    base::Time::FromUTCExploded({2016, 6, 0, 6, 11, 25, 0, 0}),
                    2},
                   {// Cert valid for exactly 15 months, needs 3 SCTs.
                    base::Time::FromUTCExploded({2015, 3, 0, 25, 11, 25, 0, 0}),
                    base::Time::FromUTCExploded({2016, 6, 0, 25, 11, 25, 0, 0}),
                    3},
                   {// Cert valid for over 15 months, needs 3 SCTs.
                    base::Time::FromUTCExploded({2015, 3, 0, 25, 11, 25, 0, 0}),
                    base::Time::FromUTCExploded({2016, 6, 0, 27, 11, 25, 0, 0}),
                    3},
                   {// Cert valid for exactly 27 months, needs 3 SCTs.
                    base::Time::FromUTCExploded({2015, 3, 0, 25, 11, 25, 0, 0}),
                    base::Time::FromUTCExploded({2017, 6, 0, 25, 11, 25, 0, 0}),
                    3},
                   {// Cert valid for over 27 months, needs 4 SCTs.
                    base::Time::FromUTCExploded({2015, 3, 0, 25, 11, 25, 0, 0}),
                    base::Time::FromUTCExploded({2017, 6, 0, 28, 11, 25, 0, 0}),
                    4},
                   {// Cert valid for exactly 39 months, needs 4 SCTs.
                    base::Time::FromUTCExploded({2015, 3, 0, 25, 11, 25, 0, 0}),
                    base::Time::FromUTCExploded({2018, 6, 0, 25, 11, 25, 0, 0}),
                    4},
                   {// Cert valid for over 39 months, needs 5 SCTs.
                    base::Time::FromUTCExploded({2015, 3, 0, 25, 11, 25, 0, 0}),
                    base::Time::FromUTCExploded({2018, 6, 0, 27, 11, 25, 0, 0}),
                    5}};

  for (size_t i = 0; i < arraysize(kTestData); ++i) {
    SCOPED_TRACE(i);
    CheckCertificateCompliesWithExactNumberOfEmbeddedSCTs(
        kTestData[i].validity_start, kTestData[i].validity_end,
        kTestData[i].scts_required);
  }
}

TEST_F(CTPolicyEnforcerTest, ConformsToPolicyByEVWhitelistPresence) {
  scoped_refptr<ct::EVCertsWhitelist> whitelist(
      new DummyEVCertsWhitelist(true, true));

  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 1,
                             &result);
  EXPECT_TRUE(policy_enforcer_->DoesConformToCTEVPolicy(
      chain_.get(), whitelist.get(), result, BoundNetLog()));
}

TEST_F(CTPolicyEnforcerTest, IgnoresInvalidEVWhitelist) {
  scoped_refptr<ct::EVCertsWhitelist> whitelist(
      new DummyEVCertsWhitelist(false, true));

  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 1,
                             &result);
  EXPECT_FALSE(policy_enforcer_->DoesConformToCTEVPolicy(
      chain_.get(), whitelist.get(), result, BoundNetLog()));
}

TEST_F(CTPolicyEnforcerTest, IgnoresNullEVWhitelist) {
  ct::CTVerifyResult result;
  FillResultWithSCTsOfOrigin(ct::SignedCertificateTimestamp::SCT_EMBEDDED, 1,
                             &result);
  EXPECT_FALSE(policy_enforcer_->DoesConformToCTEVPolicy(
      chain_.get(), nullptr, result, BoundNetLog()));
}

}  // namespace

}  // namespace net
