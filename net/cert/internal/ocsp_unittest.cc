// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/ocsp.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "net/cert/internal/test_helpers.h"
#include "net/der/encode_values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const base::TimeDelta kOCSPAgeOneWeek = base::TimeDelta::FromDays(7);

std::string GetFilePath(const std::string& file_name) {
  return std::string("net/data/ocsp_unittest/") + file_name;
}

struct TestParams {
  const char* file_name;
  OCSPRevocationStatus expected_revocation_status;
  OCSPVerifyResult::ResponseStatus expected_response_status;
};

class CheckOCSPTest : public ::testing::TestWithParam<TestParams> {};

const TestParams kTestParams[] = {
    {"good_response.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},

    {"no_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::NO_MATCHING_RESPONSE},

    {"malformed_status.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::ERROR_RESPONSE},

    {"bad_status.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PARSE_RESPONSE_ERROR},

    {"bad_ocsp_type.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PARSE_RESPONSE_ERROR},

    {"bad_signature.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},

    {"ocsp_sign_direct.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},

    {"ocsp_sign_indirect.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},

    {"ocsp_sign_indirect_missing.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},

    {"ocsp_sign_bad_indirect.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},

    {"ocsp_extra_certs.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},

    {"has_version.pem", OCSPRevocationStatus::GOOD, OCSPVerifyResult::PROVIDED},

    {"responder_name.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},

    {"responder_id.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},

    {"has_extension.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},

    {"good_response_next_update.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},

    {"revoke_response.pem", OCSPRevocationStatus::REVOKED,
     OCSPVerifyResult::PROVIDED},

    {"revoke_response_reason.pem", OCSPRevocationStatus::REVOKED,
     OCSPVerifyResult::PROVIDED},

    {"unknown_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},

    {"multiple_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},

    {"other_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::NO_MATCHING_RESPONSE},

    {"has_single_extension.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},

    {"missing_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::NO_MATCHING_RESPONSE},
};

// Parameterised test name generator for tests depending on RenderTextBackend.
struct PrintTestName {
  std::string operator()(const testing::TestParamInfo<TestParams>& info) const {
    base::StringPiece name(info.param.file_name);
    // Strip ".pem" from the end as GTest names cannot contain period.
    name.remove_suffix(4);
    return name.as_string();
  }
};

INSTANTIATE_TEST_CASE_P(,
                        CheckOCSPTest,
                        ::testing::ValuesIn(kTestParams),
                        PrintTestName());

TEST_P(CheckOCSPTest, FromFile) {
  const TestParams& params = GetParam();

  std::string ocsp_data;
  std::string ca_data;
  std::string cert_data;
  const PemBlockMapping mappings[] = {
      {"OCSP RESPONSE", &ocsp_data},
      {"CA CERTIFICATE", &ca_data},
      {"CERTIFICATE", &cert_data},
  };

  ASSERT_TRUE(ReadTestDataFromPemFile(GetFilePath(params.file_name), mappings));

  // TODO(eroman): Not all of the test data passes the time checks. Need to
  // either change policy or regenerate the test data.
  bool skip_time_checks = true;
  // Mar 6 21:40:02 2016 GMT
  base::Time kVerifyTime =
      base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(1457300402);

  OCSPVerifyResult::ResponseStatus response_status;
  OCSPRevocationStatus revocation_status =
      CheckOCSP(ocsp_data, cert_data, ca_data, kVerifyTime, skip_time_checks,
                &response_status);

  EXPECT_EQ(params.expected_revocation_status, revocation_status);
  EXPECT_EQ(params.expected_response_status, response_status);
}

TEST(OCSPDateTest, Valid) {
  OCSPSingleResponse response;

  base::Time now = base::Time::Now();
  base::Time this_update = now - base::TimeDelta::FromHours(1);
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &response.this_update));
  response.has_next_update = false;
  EXPECT_TRUE(CheckOCSPDateValid(response, now, kOCSPAgeOneWeek));

  base::Time next_update = this_update + base::TimeDelta::FromDays(7);
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(next_update, &response.next_update));
  response.has_next_update = true;
  EXPECT_TRUE(CheckOCSPDateValid(response, now, kOCSPAgeOneWeek));
}

TEST(OCSPDateTest, ThisUpdateInTheFuture) {
  OCSPSingleResponse response;

  base::Time now = base::Time::Now();
  base::Time this_update = now + base::TimeDelta::FromHours(1);
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &response.this_update));
  response.has_next_update = false;
  EXPECT_FALSE(CheckOCSPDateValid(response, now, kOCSPAgeOneWeek));

  base::Time next_update = this_update + base::TimeDelta::FromDays(7);
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(next_update, &response.next_update));
  response.has_next_update = true;
  EXPECT_FALSE(CheckOCSPDateValid(response, now, kOCSPAgeOneWeek));
}

TEST(OCSPDateTest, NextUpdatePassed) {
  OCSPSingleResponse response;

  base::Time now = base::Time::Now();
  base::Time this_update = now - base::TimeDelta::FromDays(6);
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &response.this_update));
  response.has_next_update = false;
  EXPECT_TRUE(CheckOCSPDateValid(response, now, kOCSPAgeOneWeek));

  base::Time next_update = now - base::TimeDelta::FromHours(1);
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(next_update, &response.next_update));
  response.has_next_update = true;
  EXPECT_FALSE(CheckOCSPDateValid(response, now, kOCSPAgeOneWeek));
}

TEST(OCSPDateTest, NextUpdateBeforeThisUpdate) {
  OCSPSingleResponse response;

  base::Time now = base::Time::Now();
  base::Time this_update = now - base::TimeDelta::FromDays(1);
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &response.this_update));
  response.has_next_update = false;
  EXPECT_TRUE(CheckOCSPDateValid(response, now, kOCSPAgeOneWeek));

  base::Time next_update = this_update - base::TimeDelta::FromDays(1);
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(next_update, &response.next_update));
  response.has_next_update = true;
  EXPECT_FALSE(CheckOCSPDateValid(response, now, kOCSPAgeOneWeek));
}

TEST(OCSPDateTest, ThisUpdateOlderThanMaxAge) {
  OCSPSingleResponse response;

  base::Time now = base::Time::Now();
  base::Time this_update = now - kOCSPAgeOneWeek;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &response.this_update));
  response.has_next_update = false;
  EXPECT_TRUE(CheckOCSPDateValid(response, now, kOCSPAgeOneWeek));

  base::Time next_update = now + base::TimeDelta::FromHours(1);
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(next_update, &response.next_update));
  response.has_next_update = true;
  EXPECT_TRUE(CheckOCSPDateValid(response, now, kOCSPAgeOneWeek));

  ASSERT_TRUE(der::EncodeTimeAsGeneralizedTime(
      this_update - base::TimeDelta::FromSeconds(1), &response.this_update));
  response.has_next_update = false;
  EXPECT_FALSE(CheckOCSPDateValid(response, now, kOCSPAgeOneWeek));
  response.has_next_update = true;
  EXPECT_FALSE(CheckOCSPDateValid(response, now, kOCSPAgeOneWeek));
}

TEST(OCSPDateTest, VerifyTimeFromBeforeWindowsEpoch) {
  OCSPSingleResponse response;
  base::Time windows_epoch;
  base::Time verify_time = windows_epoch - base::TimeDelta::FromDays(1);

  base::Time now = base::Time::Now();
  base::Time this_update = now - base::TimeDelta::FromHours(1);
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &response.this_update));
  response.has_next_update = false;
  EXPECT_FALSE(CheckOCSPDateValid(response, verify_time, kOCSPAgeOneWeek));

  base::Time next_update = this_update + kOCSPAgeOneWeek;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(next_update, &response.next_update));
  response.has_next_update = true;
  EXPECT_FALSE(CheckOCSPDateValid(response, verify_time, kOCSPAgeOneWeek));
}

TEST(OCSPDateTest, VerifyTimeMinusAgeFromBeforeWindowsEpoch) {
  OCSPSingleResponse response;
  base::Time windows_epoch;
  base::Time verify_time = windows_epoch + base::TimeDelta::FromDays(1);

  base::Time this_update = windows_epoch;
  ASSERT_TRUE(
      der::EncodeTimeAsGeneralizedTime(this_update, &response.this_update));
  response.has_next_update = false;
#if defined(OS_WIN)
  EXPECT_FALSE(CheckOCSPDateValid(response, verify_time, kOCSPAgeOneWeek));
#else
  EXPECT_TRUE(CheckOCSPDateValid(response, verify_time, kOCSPAgeOneWeek));
#endif
}

}  // namespace

}  // namespace net
