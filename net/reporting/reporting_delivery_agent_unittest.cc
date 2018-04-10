// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_delivery_agent.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "net/base/backoff_entry.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_test_util.h"
#include "net/reporting/reporting_uploader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

class ReportingDeliveryAgentTest : public ReportingTestBase {
 protected:
  ReportingDeliveryAgentTest() {
    ReportingPolicy policy;
    policy.endpoint_backoff_policy.num_errors_to_ignore = 0;
    policy.endpoint_backoff_policy.initial_delay_ms = 60000;
    policy.endpoint_backoff_policy.multiply_factor = 2.0;
    policy.endpoint_backoff_policy.jitter_factor = 0.0;
    policy.endpoint_backoff_policy.maximum_backoff_ms = -1;
    policy.endpoint_backoff_policy.entry_lifetime_ms = 0;
    policy.endpoint_backoff_policy.always_use_initial_delay = false;
    UsePolicy(policy);
  }

  base::TimeTicks tomorrow() {
    return tick_clock()->NowTicks() + base::TimeDelta::FromDays(1);
  }

  void SetClient(const url::Origin& origin,
                 const GURL& endpoint,
                 const std::string& group) {
    cache()->SetClient(origin, endpoint, ReportingClient::Subdomains::EXCLUDE,
                       group, tomorrow(), ReportingClient::kDefaultPriority,
                       ReportingClient::kDefaultWeight);
  }

  const GURL kUrl_ = GURL("https://origin/path");
  const url::Origin kOrigin_ = url::Origin::Create(GURL("https://origin/"));
  const GURL kEndpoint_ = GURL("https://endpoint/");
  const std::string kGroup_ = "group";
  const std::string kType_ = "type";
};

TEST_F(ReportingDeliveryAgentTest, SuccessfulImmediateUpload) {
  base::DictionaryValue body;
  body.SetString("key", "value");

  SetClient(kOrigin_, kEndpoint_, kGroup_);
  cache()->AddReport(kUrl_, kGroup_, kType_, body.CreateDeepCopy(), 0,
                     tick_clock()->NowTicks(), 0);

  // Upload is automatically started when cache is modified.

  ASSERT_EQ(1u, pending_uploads().size());
  EXPECT_EQ(kEndpoint_, pending_uploads()[0]->url());
  {
    auto value = pending_uploads()[0]->GetValue();

    base::ListValue* list;
    ASSERT_TRUE(value->GetAsList(&list));
    EXPECT_EQ(1u, list->GetSize());

    base::DictionaryValue* report;
    ASSERT_TRUE(list->GetDictionary(0, &report));
    EXPECT_EQ(4u, report->size());

    ExpectDictIntegerValue(0, *report, "age");
    ExpectDictStringValue(kType_, *report, "type");
    ExpectDictStringValue(kUrl_.spec(), *report, "url");
    ExpectDictDictionaryValue(body, *report, "report");
  }
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  // Successful upload should remove delivered reports.
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());

  // TODO(juliatuttle): Check that BackoffEntry was informed of success.
}

TEST_F(ReportingDeliveryAgentTest, SuccessfulDelayedUpload) {
  base::DictionaryValue body;
  body.SetString("key", "value");

  // Trigger and complete an upload to start the delivery timer.
  SetClient(kOrigin_, kEndpoint_, kGroup_);
  cache()->AddReport(kUrl_, kGroup_, kType_, body.CreateDeepCopy(), 0,
                     tick_clock()->NowTicks(), 0);
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  SetClient(kOrigin_, kEndpoint_, kGroup_);
  cache()->AddReport(kUrl_, kGroup_, kType_, body.CreateDeepCopy(), 0,
                     tick_clock()->NowTicks(), 0);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();

  ASSERT_EQ(1u, pending_uploads().size());
  EXPECT_EQ(kEndpoint_, pending_uploads()[0]->url());
  {
    auto value = pending_uploads()[0]->GetValue();

    base::ListValue* list;
    ASSERT_TRUE(value->GetAsList(&list));
    EXPECT_EQ(1u, list->GetSize());

    base::DictionaryValue* report;
    ASSERT_TRUE(list->GetDictionary(0, &report));
    EXPECT_EQ(4u, report->size());

    ExpectDictIntegerValue(0, *report, "age");
    ExpectDictStringValue(kType_, *report, "type");
    ExpectDictStringValue(kUrl_.spec(), *report, "url");
    ExpectDictDictionaryValue(body, *report, "report");
  }
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  {
    ReportingCache::ClientStatistics stats =
        cache()->GetStatisticsForOriginAndEndpoint(kOrigin_, kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(1, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(1, stats.successful_reports);
  }

  // Successful upload should remove delivered reports.
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());

  // TODO(juliatuttle): Check that BackoffEntry was informed of success.
}

TEST_F(ReportingDeliveryAgentTest, FailedUpload) {
  SetClient(kOrigin_, kEndpoint_, kGroup_);
  cache()->AddReport(kUrl_, kGroup_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();

  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::FAILURE);

  {
    ReportingCache::ClientStatistics stats =
        cache()->GetStatisticsForOriginAndEndpoint(kOrigin_, kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(0, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(0, stats.successful_reports);
  }

  // Failed upload should increment reports' attempts.
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(1, reports[0]->attempts);

  // Since endpoint is now failing, an upload won't be started despite a pending
  // report.
  ASSERT_TRUE(pending_uploads().empty());
  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  EXPECT_TRUE(pending_uploads().empty());

  {
    ReportingCache::ClientStatistics stats =
        cache()->GetStatisticsForOriginAndEndpoint(kOrigin_, kEndpoint_);
    EXPECT_EQ(1, stats.attempted_uploads);
    EXPECT_EQ(0, stats.successful_uploads);
    EXPECT_EQ(1, stats.attempted_reports);
    EXPECT_EQ(0, stats.successful_reports);
  }
}

TEST_F(ReportingDeliveryAgentTest, DisallowedUpload) {
  // This mimics the check that is controlled by the BACKGROUND_SYNC permission
  // in a real browser profile.
  context()->test_delegate()->set_disallow_report_uploads(true);

  static const int kAgeMillis = 12345;

  base::DictionaryValue body;
  body.SetString("key", "value");

  SetClient(kOrigin_, kEndpoint_, kGroup_);
  cache()->AddReport(kUrl_, kGroup_, kType_, body.CreateDeepCopy(), 0,
                     tick_clock()->NowTicks(), 0);

  tick_clock()->Advance(base::TimeDelta::FromMilliseconds(kAgeMillis));

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();

  // We should not try to upload the report, since we weren't given permission
  // for this origin.
  EXPECT_TRUE(pending_uploads().empty());

  {
    ReportingCache::ClientStatistics stats =
        cache()->GetStatisticsForOriginAndEndpoint(kOrigin_, kEndpoint_);
    EXPECT_EQ(0, stats.attempted_uploads);
    EXPECT_EQ(0, stats.successful_uploads);
    EXPECT_EQ(0, stats.attempted_reports);
    EXPECT_EQ(0, stats.successful_reports);
  }

  // Disallowed reports should NOT have been removed from the cache.
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());
}

TEST_F(ReportingDeliveryAgentTest, RemoveEndpointUpload) {
  static const url::Origin kDifferentOrigin =
      url::Origin::Create(GURL("https://origin2/"));

  SetClient(kOrigin_, kEndpoint_, kGroup_);
  SetClient(kDifferentOrigin, kEndpoint_, kGroup_);
  ASSERT_TRUE(FindClientInCache(cache(), kOrigin_, kEndpoint_));
  ASSERT_TRUE(FindClientInCache(cache(), kDifferentOrigin, kEndpoint_));

  cache()->AddReport(kUrl_, kGroup_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();

  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::REMOVE_ENDPOINT);

  // "Remove endpoint" upload should remove endpoint from *all* origins and
  // increment reports' attempts.
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(1, reports[0]->attempts);

  EXPECT_FALSE(FindClientInCache(cache(), kOrigin_, kEndpoint_));
  EXPECT_FALSE(FindClientInCache(cache(), kDifferentOrigin, kEndpoint_));

  // Since endpoint is now failing, an upload won't be started despite a pending
  // report.
  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  EXPECT_TRUE(pending_uploads().empty());
}

TEST_F(ReportingDeliveryAgentTest, ConcurrentRemove) {
  SetClient(kOrigin_, kEndpoint_, kGroup_);
  cache()->AddReport(kUrl_, kGroup_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(1u, pending_uploads().size());

  // Remove the report while the upload is running.
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());

  const ReportingReport* report = reports[0];
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(report));

  // Report should appear removed, even though the cache has doomed it.
  cache()->RemoveReports(reports, ReportingReport::Outcome::UNKNOWN);
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
  EXPECT_TRUE(cache()->IsReportDoomedForTesting(report));

  // Completing upload shouldn't crash, and report should still be gone.
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
  // This is slightly sketchy since |report| has been freed, but it nonetheless
  // should not be in the set of doomed reports.
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(report));
}

TEST_F(ReportingDeliveryAgentTest, ConcurrentRemoveDuringPermissionsCheck) {
  // Pause the permissions check, so that we can try to remove some reports
  // while we're in the middle of verifying that we can upload them.  (This is
  // similar to the previous test, but removes the reports during a different
  // part of the upload process.)
  context()->test_delegate()->set_pause_permissions_check(true);

  SetClient(kOrigin_, kEndpoint_, kGroup_);
  cache()->AddReport(kUrl_, kGroup_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  ASSERT_TRUE(context()->test_delegate()->PermissionsCheckPaused());

  // Remove the report while the upload is running.
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(1u, reports.size());

  const ReportingReport* report = reports[0];
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(report));

  // Report should appear removed, even though the cache has doomed it.
  cache()->RemoveReports(reports, ReportingReport::Outcome::UNKNOWN);
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
  EXPECT_TRUE(cache()->IsReportDoomedForTesting(report));

  // Completing upload shouldn't crash, and report should still be gone.
  context()->test_delegate()->ResumePermissionsCheck();
  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
  // This is slightly sketchy since |report| has been freed, but it nonetheless
  // should not be in the set of doomed reports.
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(report));
}

// Test that the agent will combine reports destined for the same endpoint, even
// if the reports are from different origins.
TEST_F(ReportingDeliveryAgentTest,
       BatchReportsFromDifferentOriginsToSameEndpoint) {
  static const GURL kDifferentUrl("https://origin2/path");
  static const url::Origin kDifferentOrigin =
      url::Origin::Create(kDifferentUrl);

  SetClient(kOrigin_, kEndpoint_, kGroup_);
  SetClient(kDifferentOrigin, kEndpoint_, kGroup_);

  cache()->AddReport(kUrl_, kGroup_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);
  cache()->AddReport(kDifferentUrl, kGroup_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(1u, pending_uploads().size());

  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());
}

// Test that the agent won't start a second upload to the same endpoint (even
// for a different origin) while one is pending, but will once it is no longer
// pending.
TEST_F(ReportingDeliveryAgentTest, SerializeUploadsToEndpoint) {
  static const GURL kDifferentUrl("https://origin2/path");
  static const url::Origin kDifferentOrigin =
      url::Origin::Create(kDifferentUrl);

  SetClient(kOrigin_, kEndpoint_, kGroup_);
  SetClient(kDifferentOrigin, kEndpoint_, kGroup_);

  cache()->AddReport(kUrl_, kGroup_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  EXPECT_EQ(1u, pending_uploads().size());

  cache()->AddReport(kDifferentUrl, kGroup_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(1u, pending_uploads().size());

  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(1u, pending_uploads().size());

  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());
}

// Test that the agent won't start a second upload for an (origin, group) while
// one is pending, even if a different endpoint is available, but will once the
// original delivery is complete and the (origin, group) is no longer pending.
TEST_F(ReportingDeliveryAgentTest, SerializeUploadsToGroup) {
  static const GURL kDifferentEndpoint("https://endpoint2/");

  SetClient(kOrigin_, kEndpoint_, kGroup_);
  SetClient(kOrigin_, kDifferentEndpoint, kGroup_);

  cache()->AddReport(kUrl_, kGroup_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  EXPECT_EQ(1u, pending_uploads().size());

  cache()->AddReport(kUrl_, kGroup_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(1u, pending_uploads().size());

  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(1u, pending_uploads().size());

  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());
}

// Tests that the agent will start parallel uploads to different groups within
// the same origin.
TEST_F(ReportingDeliveryAgentTest, ParallelizeUploadsAcrossGroups) {
  static const GURL kDifferentEndpoint("https://endpoint2/");
  static const std::string kDifferentGroup("group2");

  SetClient(kOrigin_, kEndpoint_, kGroup_);
  SetClient(kOrigin_, kDifferentEndpoint, kDifferentGroup);

  cache()->AddReport(kUrl_, kGroup_, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);
  cache()->AddReport(kUrl_, kDifferentGroup, kType_,
                     std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  EXPECT_TRUE(delivery_timer()->IsRunning());
  delivery_timer()->Fire();
  ASSERT_EQ(2u, pending_uploads().size());

  pending_uploads()[1]->Complete(ReportingUploader::Outcome::SUCCESS);
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());
}

}  // namespace
}  // namespace net
