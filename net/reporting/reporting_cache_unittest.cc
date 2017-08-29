// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_cache.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/reporting/reporting_client.h"
#include "net/reporting/reporting_observer.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

class TestReportingObserver : public ReportingObserver {
 public:
  TestReportingObserver() : cache_update_count_(0) {}

  void OnCacheUpdated() override { ++cache_update_count_; }

  int cache_update_count() const { return cache_update_count_; }

 private:
  int cache_update_count_;
};

class ReportingCacheTest : public ReportingTestBase {
 protected:
  ReportingCacheTest() : ReportingTestBase() {
    ReportingPolicy policy;
    policy.max_report_count = 5;
    policy.max_client_count = 5;
    UsePolicy(policy);

    context()->AddObserver(&observer_);
  }

  ~ReportingCacheTest() override { context()->RemoveObserver(&observer_); }

  TestReportingObserver* observer() { return &observer_; }

  size_t report_count() {
    std::vector<const ReportingReport*> reports;
    cache()->GetReports(&reports);
    return reports.size();
  }

  size_t client_count() {
    std::vector<const ReportingClient*> clients;
    cache()->GetClients(&clients);
    return clients.size();
  }

  const GURL kUrl1_ = GURL("https://origin1/path");
  const url::Origin kOrigin1_ = url::Origin(GURL("https://origin1/"));
  const url::Origin kOrigin2_ = url::Origin(GURL("https://origin2/"));
  const GURL kEndpoint1_ = GURL("https://endpoint1/");
  const GURL kEndpoint2_ = GURL("https://endpoint2/");
  const std::string kGroup1_ = "group1";
  const std::string kGroup2 = "group2";
  const std::string kType_ = "default";
  const base::TimeTicks kNow_ = base::TimeTicks::Now();
  const base::TimeTicks kExpires1_ = kNow_ + base::TimeDelta::FromDays(7);
  const base::TimeTicks kExpires2_ = kExpires1_ + base::TimeDelta::FromDays(7);

 private:
  TestReportingObserver observer_;
};

TEST_F(ReportingCacheTest, Reports) {
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());

  cache()->AddReport(kUrl1_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), kNow_, 0);
  EXPECT_EQ(1, observer()->cache_update_count());

  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  const ReportingReport* report = reports[0];
  ASSERT_TRUE(report);
  EXPECT_EQ(kUrl1_, report->url);
  EXPECT_EQ(kGroup1_, report->group);
  EXPECT_EQ(kType_, report->type);
  // TODO(juliatuttle): Check body?
  EXPECT_EQ(kNow_, report->queued);
  EXPECT_EQ(0, report->attempts);
  EXPECT_FALSE(cache()->IsReportPendingForTesting(report));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(report));

  cache()->IncrementReportsAttempts(reports);
  EXPECT_EQ(2, observer()->cache_update_count());

  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  report = reports[0];
  ASSERT_TRUE(report);
  EXPECT_EQ(1, report->attempts);

  cache()->RemoveReports(reports, ReportingReport::Outcome::UNKNOWN);
  EXPECT_EQ(3, observer()->cache_update_count());

  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_F(ReportingCacheTest, RemoveAllReports) {
  cache()->AddReport(kUrl1_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), kNow_, 0);
  cache()->AddReport(kUrl1_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), kNow_, 0);
  EXPECT_EQ(2, observer()->cache_update_count());

  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(2u, reports.size());

  cache()->RemoveAllReports(ReportingReport::Outcome::UNKNOWN);
  EXPECT_EQ(3, observer()->cache_update_count());

  cache()->GetReports(&reports);
  EXPECT_TRUE(reports.empty());
}

TEST_F(ReportingCacheTest, RemovePendingReports) {
  cache()->AddReport(kUrl1_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), kNow_, 0);
  EXPECT_EQ(1, observer()->cache_update_count());

  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_FALSE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(reports[0]));

  cache()->SetReportsPending(reports);
  EXPECT_TRUE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(reports[0]));

  cache()->RemoveReports(reports, ReportingReport::Outcome::UNKNOWN);
  EXPECT_TRUE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_TRUE(cache()->IsReportDoomedForTesting(reports[0]));
  EXPECT_EQ(2, observer()->cache_update_count());

  // After removing report, future calls to GetReports should not return it.
  std::vector<const ReportingReport*> visible_reports;
  cache()->GetReports(&visible_reports);
  EXPECT_TRUE(visible_reports.empty());
  EXPECT_EQ(1u, cache()->GetFullReportCountForTesting());

  // After clearing pending flag, report should be deleted.
  cache()->ClearReportsPending(reports);
  EXPECT_EQ(0u, cache()->GetFullReportCountForTesting());
}

TEST_F(ReportingCacheTest, RemoveAllPendingReports) {
  cache()->AddReport(kUrl1_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), kNow_, 0);
  EXPECT_EQ(1, observer()->cache_update_count());

  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_FALSE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(reports[0]));

  cache()->SetReportsPending(reports);
  EXPECT_TRUE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_FALSE(cache()->IsReportDoomedForTesting(reports[0]));

  cache()->RemoveAllReports(ReportingReport::Outcome::UNKNOWN);
  EXPECT_TRUE(cache()->IsReportPendingForTesting(reports[0]));
  EXPECT_TRUE(cache()->IsReportDoomedForTesting(reports[0]));
  EXPECT_EQ(2, observer()->cache_update_count());

  // After removing report, future calls to GetReports should not return it.
  std::vector<const ReportingReport*> visible_reports;
  cache()->GetReports(&visible_reports);
  EXPECT_TRUE(visible_reports.empty());
  EXPECT_EQ(1u, cache()->GetFullReportCountForTesting());

  // After clearing pending flag, report should be deleted.
  cache()->ClearReportsPending(reports);
  EXPECT_EQ(0u, cache()->GetFullReportCountForTesting());
}

TEST_F(ReportingCacheTest, Endpoints) {
  cache()->SetClient(kOrigin1_, kEndpoint1_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     kExpires1_);
  EXPECT_EQ(1, observer()->cache_update_count());

  const ReportingClient* client =
      FindClientInCache(cache(), kOrigin1_, kEndpoint1_);
  ASSERT_TRUE(client);
  EXPECT_EQ(kOrigin1_, client->origin);
  EXPECT_EQ(kEndpoint1_, client->endpoint);
  EXPECT_EQ(ReportingClient::Subdomains::EXCLUDE, client->subdomains);
  EXPECT_EQ(kGroup1_, client->group);
  EXPECT_EQ(kExpires1_, client->expires);

  cache()->SetClient(kOrigin1_, kEndpoint1_,
                     ReportingClient::Subdomains::INCLUDE, kGroup2, kExpires2_);
  EXPECT_EQ(2, observer()->cache_update_count());

  client = FindClientInCache(cache(), kOrigin1_, kEndpoint1_);
  ASSERT_TRUE(client);
  EXPECT_EQ(kOrigin1_, client->origin);
  EXPECT_EQ(kEndpoint1_, client->endpoint);
  EXPECT_EQ(ReportingClient::Subdomains::INCLUDE, client->subdomains);
  EXPECT_EQ(kGroup2, client->group);
  EXPECT_EQ(kExpires2_, client->expires);

  cache()->RemoveClients(std::vector<const ReportingClient*>{client});
  EXPECT_EQ(3, observer()->cache_update_count());

  client = FindClientInCache(cache(), kOrigin1_, kEndpoint1_);
  EXPECT_FALSE(client);
}

TEST_F(ReportingCacheTest, GetClientsForOriginAndGroup) {
  cache()->SetClient(kOrigin1_, kEndpoint1_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     kExpires1_);
  cache()->SetClient(kOrigin1_, kEndpoint2_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup2, kExpires1_);
  cache()->SetClient(kOrigin2_, kEndpoint1_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     kExpires1_);

  std::vector<const ReportingClient*> clients;
  cache()->GetClientsForOriginAndGroup(kOrigin1_, kGroup1_, &clients);
  ASSERT_EQ(1u, clients.size());
  const ReportingClient* client = clients[0];
  ASSERT_TRUE(client);
  EXPECT_EQ(kOrigin1_, client->origin);
  EXPECT_EQ(kGroup1_, client->group);
}

TEST_F(ReportingCacheTest, RemoveClientForOriginAndEndpoint) {
  cache()->SetClient(kOrigin1_, kEndpoint1_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     kExpires1_);
  cache()->SetClient(kOrigin1_, kEndpoint2_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup2, kExpires1_);
  cache()->SetClient(kOrigin2_, kEndpoint1_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     kExpires1_);
  EXPECT_EQ(3, observer()->cache_update_count());

  cache()->RemoveClientForOriginAndEndpoint(kOrigin1_, kEndpoint1_);
  EXPECT_EQ(4, observer()->cache_update_count());

  std::vector<const ReportingClient*> clients;
  cache()->GetClientsForOriginAndGroup(kOrigin1_, kGroup1_, &clients);
  EXPECT_TRUE(clients.empty());

  cache()->GetClientsForOriginAndGroup(kOrigin1_, kGroup2, &clients);
  EXPECT_EQ(1u, clients.size());

  cache()->GetClientsForOriginAndGroup(kOrigin2_, kGroup1_, &clients);
  EXPECT_EQ(1u, clients.size());
}

TEST_F(ReportingCacheTest, RemoveClientsForEndpoint) {
  cache()->SetClient(kOrigin1_, kEndpoint1_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     kExpires1_);
  cache()->SetClient(kOrigin1_, kEndpoint2_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup2, kExpires1_);
  cache()->SetClient(kOrigin2_, kEndpoint1_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     kExpires1_);
  EXPECT_EQ(3, observer()->cache_update_count());

  cache()->RemoveClientsForEndpoint(kEndpoint1_);
  EXPECT_EQ(4, observer()->cache_update_count());

  std::vector<const ReportingClient*> clients;
  cache()->GetClientsForOriginAndGroup(kOrigin1_, kGroup1_, &clients);
  EXPECT_TRUE(clients.empty());

  cache()->GetClientsForOriginAndGroup(kOrigin1_, kGroup2, &clients);
  EXPECT_EQ(1u, clients.size());

  cache()->GetClientsForOriginAndGroup(kOrigin2_, kGroup1_, &clients);
  EXPECT_TRUE(clients.empty());
}

TEST_F(ReportingCacheTest, RemoveAllClients) {
  cache()->SetClient(kOrigin1_, kEndpoint1_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     kExpires1_);
  cache()->SetClient(kOrigin2_, kEndpoint2_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     kExpires1_);
  EXPECT_EQ(2, observer()->cache_update_count());

  cache()->RemoveAllClients();
  EXPECT_EQ(3, observer()->cache_update_count());

  std::vector<const ReportingClient*> clients;
  cache()->GetClients(&clients);
  EXPECT_TRUE(clients.empty());
}

TEST_F(ReportingCacheTest, ExcludeSubdomainsDifferentPort) {
  const url::Origin kOrigin(GURL("https://example/"));
  const url::Origin kDifferentPortOrigin(GURL("https://example:444/"));

  cache()->SetClient(kDifferentPortOrigin, kEndpoint1_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     kExpires1_);

  std::vector<const ReportingClient*> clients;
  cache()->GetClientsForOriginAndGroup(kOrigin, kGroup1_, &clients);
  EXPECT_TRUE(clients.empty());
}

TEST_F(ReportingCacheTest, ExcludeSubdomainsSuperdomain) {
  const url::Origin kOrigin(GURL("https://foo.example/"));
  const url::Origin kSuperOrigin(GURL("https://example/"));

  cache()->SetClient(kSuperOrigin, kEndpoint1_,
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     kExpires1_);

  std::vector<const ReportingClient*> clients;
  cache()->GetClientsForOriginAndGroup(kOrigin, kGroup1_, &clients);
  EXPECT_TRUE(clients.empty());
}

TEST_F(ReportingCacheTest, IncludeSubdomainsDifferentPort) {
  const url::Origin kOrigin(GURL("https://example/"));
  const url::Origin kDifferentPortOrigin(GURL("https://example:444/"));

  cache()->SetClient(kDifferentPortOrigin, kEndpoint1_,
                     ReportingClient::Subdomains::INCLUDE, kGroup1_,
                     kExpires1_);

  std::vector<const ReportingClient*> clients;
  cache()->GetClientsForOriginAndGroup(kOrigin, kGroup1_, &clients);
  ASSERT_EQ(1u, clients.size());
  EXPECT_EQ(kDifferentPortOrigin, clients[0]->origin);
}

TEST_F(ReportingCacheTest, IncludeSubdomainsSuperdomain) {
  const url::Origin kOrigin(GURL("https://foo.example/"));
  const url::Origin kSuperOrigin(GURL("https://example/"));

  cache()->SetClient(kSuperOrigin, kEndpoint1_,
                     ReportingClient::Subdomains::INCLUDE, kGroup1_,
                     kExpires1_);

  std::vector<const ReportingClient*> clients;
  cache()->GetClientsForOriginAndGroup(kOrigin, kGroup1_, &clients);
  ASSERT_EQ(1u, clients.size());
  EXPECT_EQ(kSuperOrigin, clients[0]->origin);
}

TEST_F(ReportingCacheTest, IncludeSubdomainsPreferOriginToDifferentPort) {
  const url::Origin kOrigin(GURL("https://foo.example/"));
  const url::Origin kDifferentPortOrigin(GURL("https://example:444/"));

  cache()->SetClient(kOrigin, kEndpoint1_, ReportingClient::Subdomains::INCLUDE,
                     kGroup1_, kExpires1_);
  cache()->SetClient(kDifferentPortOrigin, kEndpoint1_,
                     ReportingClient::Subdomains::INCLUDE, kGroup1_,
                     kExpires1_);

  std::vector<const ReportingClient*> clients;
  cache()->GetClientsForOriginAndGroup(kOrigin, kGroup1_, &clients);
  ASSERT_EQ(1u, clients.size());
  EXPECT_EQ(kOrigin, clients[0]->origin);
}

TEST_F(ReportingCacheTest, IncludeSubdomainsPreferOriginToSuperdomain) {
  const url::Origin kOrigin(GURL("https://foo.example/"));
  const url::Origin kSuperOrigin(GURL("https://example/"));

  cache()->SetClient(kOrigin, kEndpoint1_, ReportingClient::Subdomains::INCLUDE,
                     kGroup1_, kExpires1_);
  cache()->SetClient(kSuperOrigin, kEndpoint1_,
                     ReportingClient::Subdomains::INCLUDE, kGroup1_,
                     kExpires1_);

  std::vector<const ReportingClient*> clients;
  cache()->GetClientsForOriginAndGroup(kOrigin, kGroup1_, &clients);
  ASSERT_EQ(1u, clients.size());
  EXPECT_EQ(kOrigin, clients[0]->origin);
}

TEST_F(ReportingCacheTest, IncludeSubdomainsPreferMoreSpecificSuperdomain) {
  const url::Origin kOrigin(GURL("https://foo.bar.example/"));
  const url::Origin kSuperOrigin(GURL("https://bar.example/"));
  const url::Origin kSuperSuperOrigin(GURL("https://example/"));

  cache()->SetClient(kSuperOrigin, kEndpoint1_,
                     ReportingClient::Subdomains::INCLUDE, kGroup1_,
                     kExpires1_);
  cache()->SetClient(kSuperSuperOrigin, kEndpoint1_,
                     ReportingClient::Subdomains::INCLUDE, kGroup1_,
                     kExpires1_);

  std::vector<const ReportingClient*> clients;
  cache()->GetClientsForOriginAndGroup(kOrigin, kGroup1_, &clients);
  ASSERT_EQ(1u, clients.size());
  EXPECT_EQ(kSuperOrigin, clients[0]->origin);
}

TEST_F(ReportingCacheTest, EvictOldestReport) {
  size_t max_report_count = policy().max_report_count;

  ASSERT_LT(0u, max_report_count);
  ASSERT_GT(std::numeric_limits<size_t>::max(), max_report_count);

  base::TimeTicks earliest_queued = tick_clock()->NowTicks();

  // Enqueue the maximum number of reports, spaced apart in time.
  for (size_t i = 0; i < max_report_count; ++i) {
    cache()->AddReport(kUrl1_, kGroup1_, kType_,
                       std::make_unique<base::DictionaryValue>(),
                       tick_clock()->NowTicks(), 0);
    tick_clock()->Advance(base::TimeDelta::FromMinutes(1));
  }
  EXPECT_EQ(max_report_count, report_count());

  // Add one more report to force the cache to evict one.
  cache()->AddReport(kUrl1_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), kNow_, 0);

  // Make sure the cache evicted a report to make room for the new one, and make
  // sure the report evicted was the earliest-queued one.
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(max_report_count, reports.size());
  for (const ReportingReport* report : reports)
    EXPECT_NE(earliest_queued, report->queued);
}

TEST_F(ReportingCacheTest, DontEvictPendingReports) {
  size_t max_report_count = policy().max_report_count;

  ASSERT_LT(0u, max_report_count);
  ASSERT_GT(std::numeric_limits<size_t>::max(), max_report_count);

  // Enqueue the maximum number of reports, spaced apart in time.
  for (size_t i = 0; i < max_report_count; ++i) {
    cache()->AddReport(kUrl1_, kGroup1_, kType_,
                       std::make_unique<base::DictionaryValue>(),
                       tick_clock()->NowTicks(), 0);
    tick_clock()->Advance(base::TimeDelta::FromMinutes(1));
  }
  EXPECT_EQ(max_report_count, report_count());

  // Mark all of the queued reports pending.
  std::vector<const ReportingReport*> queued_reports;
  cache()->GetReports(&queued_reports);
  cache()->SetReportsPending(queued_reports);

  // Add one more report to force the cache to evict one. Since the cache has
  // only pending reports, it will be forced to evict the *new* report!
  cache()->AddReport(kUrl1_, kGroup1_, kType_,
                     std::make_unique<base::DictionaryValue>(), kNow_, 0);

  // Make sure the cache evicted a report, and make sure the report evicted was
  // the new, non-pending one.
  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  EXPECT_EQ(max_report_count, reports.size());
  for (const ReportingReport* report : reports)
    EXPECT_TRUE(cache()->IsReportPendingForTesting(report));
}

GURL MakeEndpoint(size_t index) {
  return GURL(base::StringPrintf("https://endpoint/%zd", index));
}

TEST_F(ReportingCacheTest, EvictLRUClient) {
  size_t max_client_count = policy().max_client_count;

  ASSERT_LT(0u, max_client_count);
  ASSERT_GT(std::numeric_limits<size_t>::max(), max_client_count);

  for (size_t i = 0; i < max_client_count; ++i) {
    cache()->SetClient(kOrigin1_, MakeEndpoint(i),
                       ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                       tomorrow());
  }
  EXPECT_EQ(max_client_count, client_count());

  // Use clients in reverse order, so client (max_client_count - 1) is LRU.
  for (size_t i = 1; i <= max_client_count; ++i) {
    cache()->MarkClientUsed(kOrigin1_, MakeEndpoint(max_client_count - i));
    tick_clock()->Advance(base::TimeDelta::FromSeconds(1));
  }

  // Add one more client, forcing the cache to evict the LRU.
  cache()->SetClient(kOrigin1_, MakeEndpoint(max_client_count),
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     tomorrow());
  EXPECT_EQ(max_client_count, client_count());
  EXPECT_FALSE(FindClientInCache(cache(), kOrigin1_,
                                 MakeEndpoint(max_client_count - 1)));
}

TEST_F(ReportingCacheTest, EvictExpiredClient) {
  size_t max_client_count = policy().max_client_count;

  ASSERT_LT(0u, max_client_count);
  ASSERT_GT(std::numeric_limits<size_t>::max(), max_client_count);

  for (size_t i = 0; i < max_client_count; ++i) {
    base::TimeTicks expires =
        (i == max_client_count - 1) ? yesterday() : tomorrow();
    cache()->SetClient(kOrigin1_, MakeEndpoint(i),
                       ReportingClient::Subdomains::EXCLUDE, kGroup1_, expires);
  }
  EXPECT_EQ(max_client_count, client_count());

  // Add one more client, forcing the cache to evict the expired one.
  cache()->SetClient(kOrigin1_, MakeEndpoint(max_client_count),
                     ReportingClient::Subdomains::EXCLUDE, kGroup1_,
                     tomorrow());
  EXPECT_EQ(max_client_count, client_count());
  EXPECT_FALSE(FindClientInCache(cache(), kOrigin1_,
                                 MakeEndpoint(max_client_count - 1)));
}

}  // namespace
}  // namespace net
