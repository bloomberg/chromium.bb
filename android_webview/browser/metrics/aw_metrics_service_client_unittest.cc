// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {
namespace {

// For client ID format, see:
// https://en.wikipedia.org/wiki/Universally_unique_identifier#Version_4_(random)
const char kTestClientId[] = "01234567-89ab-40cd-80ef-0123456789ab";

class TestClient : public AwMetricsServiceClient {
 public:
  TestClient() : in_sample_(true) {}
  ~TestClient() override {}

  bool IsRecordingActive() {
    auto* service = GetMetricsService();
    if (service)
      return service->recording_active();
    return false;
  }
  void SetInSample(bool value) { in_sample_ = value; }

 protected:
  bool IsInSample() override { return true; }

 private:
  bool in_sample_;
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

std::unique_ptr<TestingPrefServiceSimple> CreateTestPrefs() {
  auto prefs = std::make_unique<TestingPrefServiceSimple>();
  metrics::MetricsService::RegisterPrefs(prefs->registry());
  return prefs;
}

std::unique_ptr<TestClient> CreateAndInitTestClient(PrefService* prefs) {
  auto client = std::make_unique<TestClient>();
  client->Initialize(prefs);
  return client;
}

}  // namespace

class AwMetricsServiceClientTest : public testing::Test {
 public:
  AwMetricsServiceClientTest() : task_runner_(new base::TestSimpleTaskRunner) {
    // Required by MetricsService.
    base::SetRecordActionTaskRunner(task_runner_);
  }

 protected:
  ~AwMetricsServiceClientTest() override {}

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AwMetricsServiceClientTest);
};

TEST_F(AwMetricsServiceClientTest, TestSetConsentTrueBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(true, true);
  client->Initialize(prefs.get());
  ASSERT_TRUE(client->IsRecordingActive());
  ASSERT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(false, false);
  client->Initialize(prefs.get());
  ASSERT_FALSE(client->IsRecordingActive());
  ASSERT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentTrueAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  ASSERT_TRUE(client->IsRecordingActive());
  ASSERT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false, false);
  ASSERT_FALSE(client->IsRecordingActive());
  ASSERT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

// If there is already a valid client ID, it should be reused.
TEST_F(AwMetricsServiceClientTest, TestKeepExistingClientId) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  ASSERT_TRUE(client->IsRecordingActive());
  ASSERT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  ASSERT_EQ(kTestClientId, prefs->GetString(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseClearsClientId) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false, false);
  ASSERT_FALSE(client->IsRecordingActive());
  ASSERT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestCanForceEnableMetrics) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      metrics::switches::kForceEnableMetricsReporting);

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // Flag should have higher precedence than sampling or user consent (but not
  // app consent, so we set that to 'true' for this case).
  client->SetHaveMetricsConsent(false, /* app_consent */ true);
  client->SetInSample(false);
  client->Initialize(prefs.get());

  ASSERT_TRUE(client->IsReportingEnabled());
  ASSERT_TRUE(client->IsRecordingActive());
}

TEST_F(AwMetricsServiceClientTest, TestCanForceEnableMetricsIfAlreadyEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      metrics::switches::kForceEnableMetricsReporting);

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // This is a sanity check: flip consent and sampling to true, just to make
  // sure the flag continues to work.
  client->SetHaveMetricsConsent(true, true);
  client->SetInSample(true);
  client->Initialize(prefs.get());

  ASSERT_TRUE(client->IsReportingEnabled());
  ASSERT_TRUE(client->IsRecordingActive());
}

TEST_F(AwMetricsServiceClientTest, TestCannotForceEnableMetricsIfAppOptsOut) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      metrics::switches::kForceEnableMetricsReporting);

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // Even with the flag, app consent should be respected.
  client->SetHaveMetricsConsent(true, /* app_consent */ false);
  client->SetInSample(true);
  client->Initialize(prefs.get());

  ASSERT_FALSE(client->IsReportingEnabled());
  ASSERT_FALSE(client->IsRecordingActive());
}

// TODO(https://crbug.com/1012025): remove this when the kInstallDate pref has
// been persisted for one or two milestones.
TEST_F(AwMetricsServiceClientTest, TestPreferPersistedInstallDate) {
  base::HistogramTester histogram_tester;
  auto prefs = CreateTestPrefs();
  int64_t install_date = 12345;
  prefs->SetInt64(metrics::prefs::kInstallDate, install_date);
  auto client = CreateAndInitTestClient(prefs.get());
  ASSERT_EQ(install_date, prefs->GetInt64(metrics::prefs::kInstallDate));

  // Verify the histogram.
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.BackfillInstallDate",
      BackfillInstallDate::kValidInstallDatePref, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.BackfillInstallDate", 1);
}

// TODO(https://crbug.com/1012025): remove this when the kInstallDate pref has
// been persisted for one or two milestones.
TEST_F(AwMetricsServiceClientTest, TestGetInstallDateFromJavaIfMissing) {
  base::HistogramTester histogram_tester;
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  // All we can safely assert is the install time is set, since checking the
  // actual time is racy (ex. in the unlikely scenario if this test executes in
  // the same millisecond as when the package was installed).
  ASSERT_TRUE(prefs->HasPrefPath(metrics::prefs::kInstallDate));

  // Verify the histogram.
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.BackfillInstallDate",
      BackfillInstallDate::kPersistedPackageManagerInstallDate, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.BackfillInstallDate", 1);
}

}  // namespace android_webview
