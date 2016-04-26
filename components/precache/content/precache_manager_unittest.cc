// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/content/precache_manager.h"

#include <stddef.h>

#include <map>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/test/histogram_tester.h"
#include "base/thread_task_runner_handle.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/precache/core/precache_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace precache {

namespace {

using ::testing::_;
using ::testing::ContainerEq;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SaveArg;

const char kConfigURL[] = "http://config-url.com";
const char kManifestURLPrefix[] = "http://manifest-url-prefix.com/";
const char kGoodManifestURL[] =
    "http://manifest-url-prefix.com/good-manifest.com";

class TestURLFetcherCallback {
 public:
  std::unique_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* delegate,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    std::unique_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
        url, delegate, response_data, response_code, status));

    requested_urls_.insert(url);
    return fetcher;
  }

  const std::multiset<GURL>& requested_urls() const {
    return requested_urls_;
  }

 private:
  // Multiset with one entry for each URL requested.
  std::multiset<GURL> requested_urls_;
};

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() {
    ON_CALL(*this, HostRankIfAvailable(_, _))
        .WillByDefault(Invoke(
            [](const GURL& url, const base::Callback<void(int)>& callback) {
              callback.Run(history::kMaxTopHosts);
            }));
  }

  MOCK_CONST_METHOD2(TopHosts,
                     void(size_t num_hosts, const TopHostsCallback& callback));

  MOCK_CONST_METHOD2(HostRankIfAvailable,
                     void(const GURL& url,
                          const base::Callback<void(int)>& callback));
};

ACTION_P(ReturnHosts, starting_hosts) {
  arg1.Run(starting_hosts);
}

class TestPrecacheCompletionCallback {
 public:
  TestPrecacheCompletionCallback() : was_on_done_called_(false) {}

  void OnDone(bool precaching_started) { was_on_done_called_ = true; }

  PrecacheManager::PrecacheCompletionCallback GetCallback() {
    return base::Bind(&TestPrecacheCompletionCallback::OnDone,
                      base::Unretained(this));
  }

  bool was_on_done_called() const {
    return was_on_done_called_;
  }

 private:
  bool was_on_done_called_;
};

class PrecacheManagerUnderTest : public PrecacheManager {
 public:
  PrecacheManagerUnderTest(content::BrowserContext* browser_context,
                           const sync_driver::SyncService* const sync_service,
                           const history::HistoryService* const history_service)
      : PrecacheManager(browser_context, sync_service, history_service),
        control_group_(false) {}
  bool IsInExperimentGroup() const override { return !control_group_; }
  bool IsInControlGroup() const override { return control_group_; }
  bool IsPrecachingAllowed() const override { return true; }
  void SetInControlGroup(bool in_control_group) {
    control_group_ = in_control_group;
  }

 private:
  bool control_group_;
};

class PrecacheManagerTest : public testing::Test {
 public:
  PrecacheManagerTest()
      : precache_manager_(&browser_context_,
                          nullptr /* sync_service */,
                          &history_service_),
        factory_(nullptr,
                 base::Bind(&TestURLFetcherCallback::CreateURLFetcher,
                            base::Unretained(&url_callback_))) {}

  ~PrecacheManagerTest() {
    // precache_manager_'s constructor releases a PrecacheDatabase and deletes
    // it on the DB thread. PrecacheDatabase already has a pending Init call
    // which will assert in debug builds because the directory passed to it is
    // deleted. So manually ensure that the task is run before browser_context_
    // is destructed.
    base::MessageLoop::current()->RunUntilIdle();
  }

 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kPrecacheConfigSettingsURL, kConfigURL);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kPrecacheManifestURLPrefix, kManifestURLPrefix);

    // Make the fetch of the precache configuration settings fail. Precaching
    // should still complete normally in this case.
    factory_.SetFakeResponse(GURL(kConfigURL), "",
                             net::HTTP_INTERNAL_SERVER_ERROR,
                             net::URLRequestStatus::FAILED);
  }

  // Must be declared first so that it is destroyed last.
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  content::TestBrowserContext browser_context_;
  PrecacheManagerUnderTest precache_manager_;
  TestURLFetcherCallback url_callback_;
  net::FakeURLFetcherFactory factory_;
  TestPrecacheCompletionCallback precache_callback_;
  testing::NiceMock<MockHistoryService> history_service_;
  base::HistogramTester histograms_;
};

TEST_F(PrecacheManagerTest, StartAndFinishPrecaching) {
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  MockHistoryService::TopHostsCallback top_hosts_callback;
  EXPECT_CALL(history_service_, TopHosts(NumTopHosts(), _))
      .WillOnce(SaveArg<1>(&top_hosts_callback));

  factory_.SetFakeResponse(GURL(kGoodManifestURL), "", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  precache_manager_.StartPrecaching(precache_callback_.GetCallback());

  EXPECT_TRUE(precache_manager_.IsPrecaching());

  top_hosts_callback.Run(
      history::TopHostsList(1, std::make_pair("good-manifest.com", 1)));
  base::MessageLoop::current()->RunUntilIdle();  // For PrecacheFetcher.
  EXPECT_FALSE(precache_manager_.IsPrecaching());
  EXPECT_TRUE(precache_callback_.was_on_done_called());

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(GURL(kGoodManifestURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());
}

TEST_F(PrecacheManagerTest, StartAndCancelPrecachingBeforeTopHostsCompleted) {
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  MockHistoryService::TopHostsCallback top_hosts_callback;
  EXPECT_CALL(history_service_, TopHosts(NumTopHosts(), _))
      .WillOnce(SaveArg<1>(&top_hosts_callback));

  precache_manager_.SetInControlGroup(true);
  precache_manager_.StartPrecaching(precache_callback_.GetCallback());
  EXPECT_TRUE(precache_manager_.IsPrecaching());

  precache_manager_.CancelPrecaching();
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  top_hosts_callback.Run(
      history::TopHostsList(1, std::make_pair("starting-url.com", 1)));
  EXPECT_FALSE(precache_manager_.IsPrecaching());
  EXPECT_FALSE(precache_callback_.was_on_done_called());
}

TEST_F(PrecacheManagerTest, StartAndCancelPrecachingBeforeURLsReceived) {
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  MockHistoryService::TopHostsCallback top_hosts_callback;
  EXPECT_CALL(history_service_, TopHosts(NumTopHosts(), _))
      .WillOnce(SaveArg<1>(&top_hosts_callback));

  precache_manager_.StartPrecaching(precache_callback_.GetCallback());
  EXPECT_TRUE(precache_manager_.IsPrecaching());

  precache_manager_.CancelPrecaching();
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  top_hosts_callback.Run(
      history::TopHostsList(1, std::make_pair("starting-url.com", 1)));
  base::MessageLoop::current()->RunUntilIdle();  // For PrecacheFetcher.
  EXPECT_FALSE(precache_manager_.IsPrecaching());
  EXPECT_FALSE(precache_callback_.was_on_done_called());
  EXPECT_TRUE(url_callback_.requested_urls().empty());
}

TEST_F(PrecacheManagerTest, StartAndCancelPrecachingAfterURLsReceived) {
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  EXPECT_CALL(history_service_, TopHosts(NumTopHosts(), _))
      .WillOnce(ReturnHosts(
          history::TopHostsList(1, std::make_pair("starting-url.com", 1))));

  precache_manager_.StartPrecaching(precache_callback_.GetCallback());

  // Since the |history_service_| ran the callback immediately, Start() has
  // been called on the PrecacheFetcher, and the precache config settings have
  // been requested. The response has not yet been received though, so
  // precaching is still in progress.
  EXPECT_TRUE(precache_manager_.IsPrecaching());

  precache_manager_.CancelPrecaching();
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  base::MessageLoop::current()->RunUntilIdle();  // For PrecacheFetcher.
  EXPECT_FALSE(precache_manager_.IsPrecaching());
  EXPECT_FALSE(precache_callback_.was_on_done_called());

  // Even though the response for the precache config settings should not have
  // been received, the request should still have been made.
  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());
}

TEST_F(PrecacheManagerTest, RecordStatsForFetchWithSizeZero) {
  // Fetches with size 0 should be ignored.
  precache_manager_.RecordStatsForFetch(GURL("http://url.com"), GURL(),
                                        base::TimeDelta(), base::Time(), 0,
                                        false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_THAT(histograms_.GetTotalCountsForPrefix("Precache."), IsEmpty());
}

TEST_F(PrecacheManagerTest, RecordStatsForFetchWithNonHTTP) {
  // Fetches for URLs with schemes other than HTTP or HTTPS should be ignored.
  precache_manager_.RecordStatsForFetch(GURL("ftp://ftp.com"), GURL(),
                                        base::TimeDelta(), base::Time(), 1000,
                                        false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_THAT(histograms_.GetTotalCountsForPrefix("Precache."), IsEmpty());
}

TEST_F(PrecacheManagerTest, RecordStatsForFetchWithEmptyURL) {
  // Fetches for empty URLs should be ignored.
  precache_manager_.RecordStatsForFetch(GURL(), GURL(), base::TimeDelta(),
                                        base::Time(), 1000, false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_THAT(histograms_.GetTotalCountsForPrefix("Precache."), IsEmpty());
}

TEST_F(PrecacheManagerTest, RecordStatsForFetchDuringPrecaching) {
  EXPECT_CALL(history_service_, TopHosts(NumTopHosts(), _))
      .WillOnce(ReturnHosts(history::TopHostsList()));

  precache_manager_.StartPrecaching(precache_callback_.GetCallback());

  EXPECT_TRUE(precache_manager_.IsPrecaching());
  precache_manager_.RecordStatsForFetch(GURL("http://url.com"), GURL(),
                                        base::TimeDelta(), base::Time(), 1000,
                                        false);

  precache_manager_.CancelPrecaching();

  // For PrecacheFetcher and RecordURLPrecached.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_THAT(histograms_.GetTotalCountsForPrefix("Precache."),
              ElementsAre(Pair("Precache.DownloadedPrecacheMotivated", 1),
                          Pair("Precache.Fetch.PercentCompleted", 1),
                          Pair("Precache.Fetch.ResponseBytes.Network", 1),
                          Pair("Precache.Fetch.ResponseBytes.Total", 1),
                          Pair("Precache.Fetch.TimeToComplete", 1),
                          Pair("Precache.Latency.Prefetch", 1)));
}

TEST_F(PrecacheManagerTest, RecordStatsForFetchHTTP) {
  precache_manager_.RecordStatsForFetch(GURL("http://http-url.com"), GURL(),
                                        base::TimeDelta(), base::Time(), 1000,
                                        false);
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_THAT(histograms_.GetTotalCountsForPrefix("Precache."),
              ElementsAre(Pair("Precache.DownloadedNonPrecache", 1),
                          Pair("Precache.Latency.NonPrefetch", 1),
                          Pair("Precache.Latency.NonPrefetch.NonTopHosts", 1)));
}

TEST_F(PrecacheManagerTest, RecordStatsForFetchHTTPS) {
  precache_manager_.RecordStatsForFetch(GURL("https://https-url.com"), GURL(),
                                        base::TimeDelta(), base::Time(), 1000,
                                        false);
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_THAT(histograms_.GetTotalCountsForPrefix("Precache."),
              ElementsAre(Pair("Precache.DownloadedNonPrecache", 1),
                          Pair("Precache.Latency.NonPrefetch", 1),
                          Pair("Precache.Latency.NonPrefetch.NonTopHosts", 1)));
}

TEST_F(PrecacheManagerTest, RecordStatsForFetchInTopHosts) {
  EXPECT_CALL(history_service_,
              HostRankIfAvailable(GURL("http://referrer.com"), _))
      .WillOnce(Invoke(
          [](const GURL& url, const base::Callback<void(int)>& callback) {
            callback.Run(0);
          }));
  precache_manager_.RecordStatsForFetch(
      GURL("http://http-url.com"), GURL("http://referrer.com"),
      base::TimeDelta(), base::Time(), 1000, false);
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_THAT(histograms_.GetTotalCountsForPrefix("Precache."),
              ElementsAre(Pair("Precache.DownloadedNonPrecache", 1),
                          Pair("Precache.Latency.NonPrefetch", 1),
                          Pair("Precache.Latency.NonPrefetch.TopHosts", 1)));
}

TEST_F(PrecacheManagerTest, DeleteExpiredPrecacheHistory) {
  // TODO(twifkak): Split this into multiple tests.
  base::HistogramTester::CountsMap expected_histogram_count_map;

  // This test has to use Time::Now() because StartPrecaching uses Time::Now().
  const base::Time kCurrentTime = base::Time::Now();
  EXPECT_CALL(history_service_, TopHosts(NumTopHosts(), _))
      .Times(2)
      .WillRepeatedly(ReturnHosts(history::TopHostsList()));

  precache_manager_.StartPrecaching(precache_callback_.GetCallback());
  EXPECT_TRUE(precache_manager_.IsPrecaching());

  // Precache a bunch of URLs, with different fetch times.
  precache_manager_.RecordStatsForFetch(
      GURL("http://old-fetch.com"), GURL(), base::TimeDelta(),
      kCurrentTime - base::TimeDelta::FromDays(61), 1000, false);
  precache_manager_.RecordStatsForFetch(
      GURL("http://recent-fetch.com"), GURL(), base::TimeDelta(),
      kCurrentTime - base::TimeDelta::FromDays(59), 1000, false);
  precache_manager_.RecordStatsForFetch(
      GURL("http://yesterday-fetch.com"), GURL(), base::TimeDelta(),
      kCurrentTime - base::TimeDelta::FromDays(1), 1000, false);
  expected_histogram_count_map["Precache.DownloadedPrecacheMotivated"] += 3;
  expected_histogram_count_map["Precache.Fetch.PercentCompleted"]++;
  expected_histogram_count_map["Precache.Fetch.ResponseBytes.Network"]++;
  expected_histogram_count_map["Precache.Fetch.ResponseBytes.Total"]++;
  expected_histogram_count_map["Precache.Fetch.TimeToComplete"]++;
  expected_histogram_count_map["Precache.Latency.Prefetch"] += 3;

  precache_manager_.CancelPrecaching();
  // For PrecacheFetcher and RecordURLPrecached.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_THAT(histograms_.GetTotalCountsForPrefix("Precache."),
              ContainerEq(expected_histogram_count_map));

  // The expired precache will be deleted during precaching this time.
  precache_manager_.StartPrecaching(precache_callback_.GetCallback());
  EXPECT_TRUE(precache_manager_.IsPrecaching());
  expected_histogram_count_map["Precache.Fetch.PercentCompleted"]++;
  expected_histogram_count_map["Precache.Fetch.ResponseBytes.Network"]++;
  expected_histogram_count_map["Precache.Fetch.ResponseBytes.Total"]++;

  precache_manager_.CancelPrecaching();
  // For PrecacheFetcher and RecordURLPrecached.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(precache_manager_.IsPrecaching());

  // A fetch for the same URL as the expired precache was served from the cache,
  // but it isn't reported as saved bytes because it had expired in the precache
  // history.
  precache_manager_.RecordStatsForFetch(GURL("http://old-fetch.com"), GURL(),
                                        base::TimeDelta(), kCurrentTime, 1000,
                                        true);
  expected_histogram_count_map["Precache.Fetch.TimeToComplete"]++;
  expected_histogram_count_map["Precache.Latency.NonPrefetch"]++;
  expected_histogram_count_map["Precache.Latency.NonPrefetch.NonTopHosts"]++;

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_THAT(histograms_.GetTotalCountsForPrefix("Precache."),
              ContainerEq(expected_histogram_count_map));

  // The other precaches should not have expired, so the following fetches from
  // the cache should count as saved bytes.
  precache_manager_.RecordStatsForFetch(GURL("http://recent-fetch.com"), GURL(),
                                        base::TimeDelta(), kCurrentTime, 1000,
                                        true);
  precache_manager_.RecordStatsForFetch(GURL("http://yesterday-fetch.com"),
                                        GURL(), base::TimeDelta(), kCurrentTime,
                                        1000, true);
  expected_histogram_count_map["Precache.Latency.NonPrefetch"] += 2;
  expected_histogram_count_map["Precache.Latency.NonPrefetch.NonTopHosts"] += 2;
  expected_histogram_count_map["Precache.Saved"] += 2;

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_THAT(histograms_.GetTotalCountsForPrefix("Precache."),
              ContainerEq(expected_histogram_count_map));
}

}  // namespace

}  // namespace precache
