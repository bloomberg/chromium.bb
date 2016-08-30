// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_state_model_client.h"

#include "components/security_state/security_state_model.h"
#include "content/public/browser/security_style_explanations.h"
#include "net/cert/cert_status_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Tests that SecurityInfo flags for subresources with certificate
// errors are reflected in the SecurityStyleExplanations produced by
// ChromeSecurityStateModelClient.
TEST(ChromeSecurityStateModelClientTest,
     GetSecurityStyleForContentWithCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityStateModel::SecurityInfo security_info;
  security_info.cert_status = 0;
  security_info.scheme_is_cryptographic = true;

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_RAN;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_NONE;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);
}

// Tests that SecurityStyleExplanations for subresources with cert
// errors are *not* set when the main resource has major certificate
// errors. If the main resource has certificate errors, it would be
// duplicative/confusing to also report subresources with cert errors.
TEST(ChromeSecurityStateModelClientTest,
     SubresourcesAndMainResourceWithMajorCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityStateModel::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  security_info.scheme_is_cryptographic = true;

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_RAN;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_NONE;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);
}

// Tests that SecurityStyleExplanations for subresources with cert
// errors are set when the main resource has only minor certificate
// errors. Minor errors on the main resource should not hide major
// errors on subresources.
TEST(ChromeSecurityStateModelClientTest,
     SubresourcesAndMainResourceWithMinorCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityStateModel::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info.scheme_is_cryptographic = true;

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_RAN;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_NONE;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);
}

}  // namespace
