// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_delivery_agent.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
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

class MockUploader : public ReportingUploader {
 public:
  class PendingUpload {
   public:
    PendingUpload(MockUploader* uploader,
                  const GURL& url,
                  const std::string& json,
                  const Callback& callback)
        : uploader_(uploader), url_(url), json_(json), callback_(callback) {
      DCHECK(uploader_);
    }

    ~PendingUpload() {}

    void Complete(Outcome outcome) {
      callback_.Run(outcome);
      // Deletes |this|.
      uploader_->OnUploadComplete(this);
    }

    const GURL& url() const { return url_; }
    const std::string& json() const { return json_; }

    std::unique_ptr<base::Value> GetValue() const {
      return base::JSONReader::Read(json_);
    }

   private:
    MockUploader* uploader_;
    GURL url_;
    std::string json_;
    Callback callback_;
  };

  MockUploader() {}
  ~MockUploader() override {}

  void StartUpload(const GURL& url,
                   const std::string& json,
                   const Callback& callback) override {
    uploads_.push_back(
        base::MakeUnique<PendingUpload>(this, url, json, callback));
  }

  const std::vector<std::unique_ptr<PendingUpload>>& pending_uploads() const {
    return uploads_;
  }

  void OnUploadComplete(PendingUpload* upload) {
    for (auto it = uploads_.begin(); it != uploads_.end(); ++it) {
      if (it->get() == upload) {
        uploads_.erase(it);
        return;
      }
    }
    NOTREACHED();
  }

 private:
  std::vector<std::unique_ptr<PendingUpload>> uploads_;
};

class ReportingDeliveryAgentTest : public ::testing::Test {
 protected:
  ReportingDeliveryAgentTest()
      : agent_(&clock_, &cache_, &uploader_, &backoff_policy_) {
    backoff_policy_.num_errors_to_ignore = 0;
    backoff_policy_.initial_delay_ms = 60000;
    backoff_policy_.multiply_factor = 2.0;
    backoff_policy_.jitter_factor = 0.0;
    backoff_policy_.maximum_backoff_ms = -1;
    backoff_policy_.entry_lifetime_ms = 0;
    backoff_policy_.always_use_initial_delay = false;
  }

  base::TimeTicks tomorrow() {
    return clock_.NowTicks() + base::TimeDelta::FromDays(1);
  }

  const std::vector<std::unique_ptr<MockUploader::PendingUpload>>&
  pending_uploads() const {
    return uploader_.pending_uploads();
  }

  const GURL kUrl_ = GURL("https://origin/path");
  const url::Origin kOrigin_ = url::Origin(GURL("https://origin/"));
  const GURL kEndpoint_ = GURL("https://endpoint/");
  const std::string kGroup_ = "group";
  const std::string kType_ = "type";

  base::SimpleTestTickClock clock_;
  ReportingCache cache_;
  MockUploader uploader_;
  BackoffEntry::Policy backoff_policy_;
  ReportingDeliveryAgent agent_;
};

TEST_F(ReportingDeliveryAgentTest, SuccessfulUpload) {
  static const int kAgeMillis = 12345;

  base::DictionaryValue body;
  body.SetString("key", "value");

  cache_.SetClient(kOrigin_, kEndpoint_, ReportingClient::Subdomains::EXCLUDE,
                   kGroup_, tomorrow());
  cache_.AddReport(kUrl_, kGroup_, kType_, body.CreateDeepCopy(),
                   clock_.NowTicks(), 0);

  clock_.Advance(base::TimeDelta::FromMilliseconds(kAgeMillis));

  agent_.SendReports();

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

    ExpectDictIntegerValue(kAgeMillis, *report, "age");
    ExpectDictStringValue(kType_, *report, "type");
    ExpectDictStringValue(kUrl_.spec(), *report, "url");
    ExpectDictDictionaryValue(body, *report, "report");
  }
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);

  // Successful upload should remove delivered reports.
  std::vector<const ReportingReport*> reports;
  cache_.GetReports(&reports);
  EXPECT_TRUE(reports.empty());

  // TODO(juliatuttle): Check that BackoffEntry was informed of success.
}

TEST_F(ReportingDeliveryAgentTest, FailedUpload) {
  cache_.SetClient(kOrigin_, kEndpoint_, ReportingClient::Subdomains::EXCLUDE,
                   kGroup_, tomorrow());
  cache_.AddReport(kUrl_, kGroup_, kType_,
                   base::MakeUnique<base::DictionaryValue>(), clock_.NowTicks(),
                   0);

  agent_.SendReports();

  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::FAILURE);

  // Failed upload should increment reports' attempts.
  std::vector<const ReportingReport*> reports;
  cache_.GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(1, reports[0]->attempts);

  // Since endpoint is now failing, an upload won't be started despite a pending
  // report.
  ASSERT_TRUE(pending_uploads().empty());
  agent_.SendReports();
  EXPECT_TRUE(pending_uploads().empty());
}

TEST_F(ReportingDeliveryAgentTest, RemoveEndpointUpload) {
  static const url::Origin kDifferentOrigin(GURL("https://origin2/"));

  cache_.SetClient(kOrigin_, kEndpoint_, ReportingClient::Subdomains::EXCLUDE,
                   kGroup_, tomorrow());
  cache_.SetClient(kDifferentOrigin, kEndpoint_,
                   ReportingClient::Subdomains::EXCLUDE, kGroup_, tomorrow());
  ASSERT_TRUE(FindClientInCache(&cache_, kOrigin_, kEndpoint_));
  ASSERT_TRUE(FindClientInCache(&cache_, kDifferentOrigin, kEndpoint_));

  cache_.AddReport(kUrl_, kGroup_, kType_,
                   base::MakeUnique<base::DictionaryValue>(), clock_.NowTicks(),
                   0);

  agent_.SendReports();

  ASSERT_EQ(1u, pending_uploads().size());
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::REMOVE_ENDPOINT);

  // "Remove endpoint" upload should remove endpoint from *all* origins and
  // increment reports' attempts.
  std::vector<const ReportingReport*> reports;
  cache_.GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(1, reports[0]->attempts);

  EXPECT_FALSE(FindClientInCache(&cache_, kOrigin_, kEndpoint_));
  EXPECT_FALSE(FindClientInCache(&cache_, kDifferentOrigin, kEndpoint_));

  // Since endpoint is now failing, an upload won't be started despite a pending
  // report.
  agent_.SendReports();
  EXPECT_TRUE(pending_uploads().empty());
}

TEST_F(ReportingDeliveryAgentTest, ConcurrentRemove) {
  cache_.SetClient(kOrigin_, kEndpoint_, ReportingClient::Subdomains::EXCLUDE,
                   kGroup_, tomorrow());
  cache_.AddReport(kUrl_, kGroup_, kType_,
                   base::MakeUnique<base::DictionaryValue>(), clock_.NowTicks(),
                   0);

  agent_.SendReports();
  ASSERT_EQ(1u, pending_uploads().size());

  // Remove the report while the upload is running.
  std::vector<const ReportingReport*> reports;
  cache_.GetReports(&reports);
  EXPECT_EQ(1u, reports.size());

  const ReportingReport* report = reports[0];
  EXPECT_FALSE(cache_.IsReportDoomedForTesting(report));

  // Report should appear removed, even though the cache has doomed it.
  cache_.RemoveReports(reports);
  cache_.GetReports(&reports);
  EXPECT_TRUE(reports.empty());
  EXPECT_TRUE(cache_.IsReportDoomedForTesting(report));

  // Completing upload shouldn't crash, and report should still be gone.
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  cache_.GetReports(&reports);
  EXPECT_TRUE(reports.empty());
  // This is slightly sketchy since |report| has been freed, but it nonetheless
  // should not be in the set of doomed reports.
  EXPECT_FALSE(cache_.IsReportDoomedForTesting(report));
}

// Test that the agent will combine reports destined for the same endpoint, even
// if the reports are from different origins.
TEST_F(ReportingDeliveryAgentTest,
       BatchReportsFromDifferentOriginsToSameEndpoint) {
  static const GURL kDifferentUrl("https://origin2/path");
  static const url::Origin kDifferentOrigin(kDifferentUrl);

  cache_.SetClient(kOrigin_, kEndpoint_, ReportingClient::Subdomains::EXCLUDE,
                   kGroup_, tomorrow());
  cache_.SetClient(kDifferentOrigin, kEndpoint_,
                   ReportingClient::Subdomains::EXCLUDE, kGroup_, tomorrow());

  cache_.AddReport(kUrl_, kGroup_, kType_,
                   base::MakeUnique<base::DictionaryValue>(), clock_.NowTicks(),
                   0);
  cache_.AddReport(kDifferentUrl, kGroup_, kType_,
                   base::MakeUnique<base::DictionaryValue>(), clock_.NowTicks(),
                   0);

  agent_.SendReports();
  ASSERT_EQ(1u, pending_uploads().size());

  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());
}

// Test that the agent won't start a second upload to the same endpoint (even
// for a different origin) while one is pending, but will once it is no longer
// pending.
TEST_F(ReportingDeliveryAgentTest, SerializeUploadsToEndpoint) {
  static const GURL kDifferentUrl("https://origin2/path");
  static const url::Origin kDifferentOrigin(kDifferentUrl);

  cache_.SetClient(kOrigin_, kEndpoint_, ReportingClient::Subdomains::EXCLUDE,
                   kGroup_, tomorrow());
  cache_.SetClient(kDifferentOrigin, kEndpoint_,
                   ReportingClient::Subdomains::EXCLUDE, kGroup_, tomorrow());

  cache_.AddReport(kUrl_, kGroup_, kType_,
                   base::MakeUnique<base::DictionaryValue>(), clock_.NowTicks(),
                   0);

  agent_.SendReports();
  EXPECT_EQ(1u, pending_uploads().size());

  cache_.AddReport(kDifferentUrl, kGroup_, kType_,
                   base::MakeUnique<base::DictionaryValue>(), clock_.NowTicks(),
                   0);

  agent_.SendReports();
  ASSERT_EQ(1u, pending_uploads().size());

  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());

  agent_.SendReports();
  ASSERT_EQ(1u, pending_uploads().size());

  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());
}

// Test that the agent won't start a second upload for an (origin, group) while
// one is pending, even if a different endpoint is available, but will once the
// original delivery is complete and the (origin, group) is no longer pending.
TEST_F(ReportingDeliveryAgentTest, SerializeUploadsToGroup) {
  static const GURL kDifferentEndpoint("https://endpoint2/");

  cache_.SetClient(kOrigin_, kEndpoint_, ReportingClient::Subdomains::EXCLUDE,
                   kGroup_, tomorrow());
  cache_.SetClient(kOrigin_, kDifferentEndpoint,
                   ReportingClient::Subdomains::EXCLUDE, kGroup_, tomorrow());

  cache_.AddReport(kUrl_, kGroup_, kType_,
                   base::MakeUnique<base::DictionaryValue>(), clock_.NowTicks(),
                   0);

  agent_.SendReports();
  EXPECT_EQ(1u, pending_uploads().size());

  cache_.AddReport(kUrl_, kGroup_, kType_,
                   base::MakeUnique<base::DictionaryValue>(), clock_.NowTicks(),
                   0);

  agent_.SendReports();
  EXPECT_EQ(1u, pending_uploads().size());

  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());

  agent_.SendReports();
  EXPECT_EQ(1u, pending_uploads().size());
}

// Tests that the agent will start parallel uploads to different groups within
// the same origin.
TEST_F(ReportingDeliveryAgentTest, ParallelizeUploadsAcrossGroups) {
  static const GURL kDifferentEndpoint("https://endpoint2/");
  static const std::string kDifferentGroup("group2");

  cache_.SetClient(kOrigin_, kEndpoint_, ReportingClient::Subdomains::EXCLUDE,
                   kGroup_, tomorrow());
  cache_.SetClient(kOrigin_, kDifferentEndpoint,
                   ReportingClient::Subdomains::EXCLUDE, kDifferentGroup,
                   tomorrow());

  cache_.AddReport(kUrl_, kGroup_, kType_,
                   base::MakeUnique<base::DictionaryValue>(), clock_.NowTicks(),
                   0);
  cache_.AddReport(kUrl_, kDifferentGroup, kType_,
                   base::MakeUnique<base::DictionaryValue>(), clock_.NowTicks(),
                   0);

  agent_.SendReports();
  EXPECT_EQ(2u, pending_uploads().size());

  pending_uploads()[1]->Complete(ReportingUploader::Outcome::SUCCESS);
  pending_uploads()[0]->Complete(ReportingUploader::Outcome::SUCCESS);
  EXPECT_EQ(0u, pending_uploads().size());
}

}  // namespace
}  // namespace net
