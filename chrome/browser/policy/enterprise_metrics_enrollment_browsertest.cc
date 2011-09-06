// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/test/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

void assert_handler(const std::string& str) {
  LOG(INFO) << "Previous failure was expected, ignoring.";
}

}  // namespace

class EnterpriseMetricsEnrollmentTest : public WizardInProcessBrowserTest {
 protected:
  EnterpriseMetricsEnrollmentTest()
      : WizardInProcessBrowserTest(
            WizardController::kEnterpriseEnrollmentScreenName) {}

  virtual void SetUpOnMainThread() OVERRIDE {
    WizardInProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(controller() != NULL);

    screen_ = controller()->GetEnterpriseEnrollmentScreen();

    ASSERT_TRUE(screen_ != NULL);
    ASSERT_EQ(controller()->current_screen(), screen_);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    WizardInProcessBrowserTest::CleanUpOnMainThread();

    // Check that no other counters were sampled.
    base::Histogram::SampleSet samples;
    GetSampleSet(&samples);
    EXPECT_EQ(1, samples.TotalCount());
  }

  // This is used to let expected NOTREACHED paths go on instead of making
  // the test fail. It will also let CHECKs and DCHECKs pass, but that should be
  // tested elsewhere.
  // The reason is that some code paths are not expected, but if they are
  // executed we want to know about it, so there's a UMA metric for that.
  // However, the NOTREACHED would make the test fail, so we disable it.
  // There should be other unit tests that check the same code,
  // but here we are just interested in the UMA samples.
  void ExpectNOTREACHED() {
    logging::SetLogAssertHandler(assert_handler);
  }

  void CheckSample(int sample) {
    ASSERT_GE(sample, 0);
    ASSERT_LT(sample, policy::kMetricEnrollmentSize);

    base::Histogram::SampleSet samples;
    GetSampleSet(&samples);
    EXPECT_EQ(1, samples.counts(sample));
  }

  EnterpriseEnrollmentScreen* screen_;

 private:
  void GetSampleSet(base::Histogram::SampleSet* set) {
    base::Histogram* histogram = NULL;
    EXPECT_TRUE(base::StatisticsRecorder::FindHistogram(
        policy::kMetricEnrollment, &histogram));
    ASSERT_TRUE(histogram != NULL);
    histogram->SnapshotSample(set);
  }

  TestURLFetcherFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseMetricsEnrollmentTest);
};

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, EnrollmentStart) {
  screen_->OnAuthSubmitted("", "", "", "");
  CheckSample(policy::kMetricEnrollmentStarted);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, EnrollmentCancelled) {
  screen_->OnAuthCancelled();
  CheckSample(policy::kMetricEnrollmentCancelled);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, AuthTokenWrongService) {
  ExpectNOTREACHED();
  screen_->OnIssueAuthTokenSuccess(std::string(), std::string());
  CheckSample(policy::kMetricEnrollmentOtherFailed);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest,
                       AuthTokenCloudPolicyNotReady) {
  ExpectNOTREACHED();
  screen_->OnIssueAuthTokenSuccess(GaiaConstants::kDeviceManagementService,
                                   std::string());
  CheckSample(policy::kMetricEnrollmentOtherFailed);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, AuthTokenFailure) {
  ExpectNOTREACHED();
  GoogleServiceAuthError error(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  screen_->OnIssueAuthTokenFailure(std::string(), error);
  CheckSample(policy::kMetricEnrollmentOtherFailed);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, OnPolicyBadToken) {
  screen_->OnPolicyStateChanged(policy::CloudPolicySubsystem::BAD_GAIA_TOKEN,
                                policy::CloudPolicySubsystem::NO_DETAILS);
  CheckSample(policy::kMetricEnrollmentPolicyFailed);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, OnPolicyNetworkError) {
  screen_->OnPolicyStateChanged(policy::CloudPolicySubsystem::NETWORK_ERROR,
                                policy::CloudPolicySubsystem::NO_DETAILS);
  CheckSample(policy::kMetricEnrollmentPolicyFailed);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, OnPolicyUnmanaged) {
  screen_->OnPolicyStateChanged(policy::CloudPolicySubsystem::UNMANAGED,
                                policy::CloudPolicySubsystem::NO_DETAILS);
  CheckSample(policy::kMetricEnrollmentNotSupported);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, OnInvalidSN) {
  screen_->OnPolicyStateChanged(
      policy::CloudPolicySubsystem::UNENROLLED,
      policy::CloudPolicySubsystem::BAD_SERIAL_NUMBER);
  CheckSample(policy::kMetricEnrollmentInvalidSerialNumber);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, EnrollmentOK) {
  screen_->OnPolicyStateChanged(policy::CloudPolicySubsystem::SUCCESS,
                                policy::CloudPolicySubsystem::NO_DETAILS);
  CheckSample(policy::kMetricEnrollmentOK);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, AuthErrorConnection) {
  GoogleServiceAuthError error(GoogleServiceAuthError::CONNECTION_FAILED);
  screen_->OnClientLoginFailure(error);
  CheckSample(policy::kMetricEnrollmentNetworkFailed);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, AuthErrorLoginFailed) {
  GoogleServiceAuthError error(GoogleServiceAuthError::TWO_FACTOR);
  screen_->OnClientLoginFailure(error);
  CheckSample(policy::kMetricEnrollmentLoginFailed);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, AccountDisabled) {
  GoogleServiceAuthError error(GoogleServiceAuthError::ACCOUNT_DISABLED);
  screen_->OnClientLoginFailure(error);
  CheckSample(policy::kMetricEnrollmentNotSupported);
}

IN_PROC_BROWSER_TEST_F(EnterpriseMetricsEnrollmentTest, AuthErrorUnexpected) {
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  screen_->OnClientLoginFailure(error);
  CheckSample(policy::kMetricEnrollmentNetworkFailed);
}

}  // namespace chromeos
