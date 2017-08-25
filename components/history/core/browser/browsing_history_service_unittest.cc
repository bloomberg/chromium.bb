// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/browsing_history_service.h"

#include <utility>

#include "base/callback_forward.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/scoped_task_environment.h"
#include "base/values.h"
#include "components/history/core/browser/browsing_history_driver.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Time;
using base::TimeDelta;

namespace history {

using HistoryEntry = BrowsingHistoryService::HistoryEntry;

namespace {

const char kUrl1[] = "http://www.one.com";
const char kUrl2[] = "http://www.two.com";
const char kUrl3[] = "http://www.three.com";
const char kUrl4[] = "http://www.four.com";

const HistoryEntry::EntryType kLocal = HistoryEntry::LOCAL_ENTRY;
const HistoryEntry::EntryType kRemote = HistoryEntry::REMOTE_ENTRY;
const HistoryEntry::EntryType kBoth = HistoryEntry::COMBINED_ENTRY;

struct TestResult {
  std::string url;
  int64_t hour_offset;  // Visit time in hours past the baseline time.
  HistoryEntry::EntryType type;
};

// Used to bind a callback.
void DoNothing(bool ignored) {}

class QuittingHistoryDBTask : public HistoryDBTask {
 public:
  explicit QuittingHistoryDBTask(base::OnceClosure done_closure)
      : done_closure_(std::move(done_closure)) {}

  // HistoryDBTask implementation.
  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    return true;
  }
  void DoneRunOnMainThread() override { std::move(done_closure_).Run(); }

 private:
  base::OnceClosure done_closure_;
  DISALLOW_COPY_AND_ASSIGN(QuittingHistoryDBTask);
};

class TestSyncService : public syncer::FakeSyncService {
 public:
  int GetObserverCount() { return observer_count_; }

 private:
  void AddObserver(syncer::SyncServiceObserver* observer) override {
    observer_count_++;
  }
  void RemoveObserver(syncer::SyncServiceObserver* observer) override {
    observer_count_--;
  }

  int observer_count_ = 0;
};

class TestBrowsingHistoryDriver : public BrowsingHistoryDriver {
 public:
  explicit TestBrowsingHistoryDriver(WebHistoryService* web_history)
      : web_history_(web_history) {}

  void SetWebHistory(WebHistoryService* web_history) {
    web_history_ = web_history;
  }

  using QueryResult = std::pair<std::vector<HistoryEntry>,
                                BrowsingHistoryService::QueryResultsInfo>;
  std::vector<QueryResult> GetQueryResults() { return query_results_; }

  int GetHistoryDeletedCount() { return history_deleted_count_; }

 private:
  // BrowsingHistoryDriver implementation.
  void OnQueryComplete(const std::vector<HistoryEntry>& results,
                       const BrowsingHistoryService::QueryResultsInfo&
                           query_results_info) override {
    query_results_.push_back(QueryResult(results, query_results_info));
  }
  void OnRemoveVisitsComplete() override {}
  void OnRemoveVisitsFailed() override {}
  void OnRemoveVisits(
      const std::vector<ExpireHistoryArgs>& expire_list) override {}
  void HistoryDeleted() override { history_deleted_count_++; }
  void HasOtherFormsOfBrowsingHistory(bool has_other_forms,
                                      bool has_synced_results) override {}
  bool AllowHistoryDeletions() override { return true; }
  bool ShouldHideWebHistoryUrl(const GURL& url) override { return false; }
  WebHistoryService* GetWebHistoryService() override { return web_history_; }
  void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      const syncer::SyncService* sync_service,
      WebHistoryService* local_history,
      base::Callback<void(bool)> callback) override {}

  int history_deleted_count_ = 0;
  std::vector<QueryResult> query_results_;
  WebHistoryService* web_history_;
};

class TestWebHistoryService : public FakeWebHistoryService {
 public:
  TestWebHistoryService()
      : FakeWebHistoryService(nullptr,
                              nullptr,
                              scoped_refptr<net::URLRequestContextGetter>()) {}

  void TriggerOnWebHistoryDeleted() {
    TestRequest request;
    ExpireHistoryCompletionCallback(base::Bind(&DoNothing), &request, true);
  }

 private:
  class TestRequest : public WebHistoryService::Request {
   private:
    // WebHistoryService::Request implementation.
    bool IsPending() override { return false; }
    int GetResponseCode() override { return net::HTTP_OK; }
    const std::string& GetResponseBody() override { return body_; }
    void SetPostData(const std::string& post_data) override{};
    void SetPostDataAndType(const std::string& post_data,
                            const std::string& mime_type) override{};
    void SetUserAgent(const std::string& user_agent) override{};
    void Start() override{};

    std::string body_ = "{}";
  };
};

class BrowsingHistoryServiceTest : public ::testing::Test {
 protected:
  // WebHistory API is to pass time ranges as the number of microseconds since
  // Time::UnixEpoch() as a query parameter. This becomes a problem when we use
  // Time::LocalMidnight(), which rounds _down_, and will result in a  time
  // before Time::UnixEpoch() that cannot be represented. By adding 1 day we
  // ensure all test data is after Time::UnixEpoch().
  BrowsingHistoryServiceTest()
      : baseline_time_(Time::UnixEpoch().LocalMidnight() +
                       TimeDelta::FromDays(1)),
        driver_(&web_history_) {
    EXPECT_TRUE(history_dir_.CreateUniqueTempDir());
    local_history_ = CreateHistoryService(history_dir_.GetPath(), true);

    // Default initialization, tests can override with ResetService().
    browsing_history_service_ = std::make_unique<BrowsingHistoryService>(
        driver(), local_history(), sync());
  }

  void ResetService(BrowsingHistoryDriver* driver,
                    HistoryService* local_history,
                    syncer::SyncService* sync_service) {
    browsing_history_service_ = std::make_unique<BrowsingHistoryService>(
        driver, local_history, sync_service);
  }

  void BlockUntilHistoryProcessesPendingRequests() {
    base::CancelableTaskTracker tracker;
    base::RunLoop run_loop;
    local_history_->ScheduleDBTask(
        std::make_unique<QuittingHistoryDBTask>(run_loop.QuitWhenIdleClosure()),
        &tracker);
    run_loop.Run();
  }

  Time OffsetToTime(int64_t hour_offset) {
    return baseline_time_ + TimeDelta::FromHours(hour_offset);
  }

  // For each item in |test_results|, create a new HistoryEntry representing the
  // and insert it into |results|.
  void AddQueryResults(const std::vector<TestResult>& test_results,
                       std::vector<HistoryEntry>* results) {
    for (const TestResult& result : test_results) {
      HistoryEntry entry;
      entry.time = OffsetToTime(result.hour_offset);
      entry.url = GURL(result.url);
      entry.all_timestamps.insert(entry.time.ToInternalValue());
      entry.entry_type = result.type;
      results->push_back(entry);
    }
  }

  void AddHistory(const std::vector<TestResult>& data) {
    for (const TestResult& entry : data) {
      if (entry.type == kLocal) {
        local_history()->AddPage(GURL(entry.url),
                                 OffsetToTime(entry.hour_offset),
                                 VisitSource::SOURCE_BROWSED);
      } else if (entry.type == kRemote) {
        web_history()->AddSyncedVisit(entry.url,
                                      OffsetToTime(entry.hour_offset));
      } else {
        NOTREACHED();
      }
    }
  }

  void VerifyEntry(const TestResult& expected, const HistoryEntry& actual) {
    EXPECT_EQ(GURL(expected.url), actual.url);
    EXPECT_EQ(OffsetToTime(expected.hour_offset), actual.time);
    EXPECT_EQ(static_cast<int>(expected.type),
              static_cast<int>(actual.entry_type));
  }

  TestBrowsingHistoryDriver::QueryResult QueryHistory(
      const QueryOptions& options) {
    size_t previous_results_count = driver()->GetQueryResults().size();
    service()->QueryHistory(base::string16(), options);
    BlockUntilHistoryProcessesPendingRequests();
    const std::vector<TestBrowsingHistoryDriver::QueryResult> all_results =
        driver()->GetQueryResults();
    EXPECT_EQ(previous_results_count + 1, all_results.size());
    return *all_results.rbegin();
  }

  void VerifyQueryResult(bool reached_beginning,
                         bool has_synced_results,
                         const std::vector<TestResult>& expected_entries,
                         TestBrowsingHistoryDriver::QueryResult result) {
    EXPECT_EQ(reached_beginning, result.second.reached_beginning);
    EXPECT_EQ(has_synced_results, result.second.has_synced_results);
    EXPECT_EQ(expected_entries.size(), result.first.size());
    for (size_t i = 0; i < expected_entries.size(); ++i) {
      VerifyEntry(expected_entries[i], result.first[i]);
    }
  }

  void QueryAndVerifySingleQueryResult(
      bool reached_beginning,
      bool has_synced_results,
      const std::vector<TestResult>& expected_entries) {
    EXPECT_EQ(0U, driver()->GetQueryResults().size());
    TestBrowsingHistoryDriver::QueryResult result =
        QueryHistory(QueryOptions());
    VerifyQueryResult(reached_beginning, has_synced_results, expected_entries,
                      result);
  }

  HistoryService* local_history() { return local_history_.get(); }
  TestWebHistoryService* web_history() { return &web_history_; }
  TestSyncService* sync() { return &sync_service_; }
  TestBrowsingHistoryDriver* driver() { return &driver_; }
  BrowsingHistoryService* service() { return browsing_history_service_.get(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // Duplicates on the same day in the local timezone are removed, so set a
  // baseline time in local time.
  Time baseline_time_;

  base::ScopedTempDir history_dir_;
  std::unique_ptr<HistoryService> local_history_;
  TestWebHistoryService web_history_;
  TestSyncService sync_service_;
  TestBrowsingHistoryDriver driver_;
  std::unique_ptr<BrowsingHistoryService> browsing_history_service_;
};

// Tests that the MergeDuplicateResults method correctly removes duplicate
// visits to the same URL on the same day.
TEST_F(BrowsingHistoryServiceTest, MergeDuplicateResults) {
  {
    // Basic test that duplicates on the same day are removed.
    const std::vector<TestResult> test_data{
        {kUrl1, 0, kLocal},
        {kUrl2, 1, kLocal},
        {kUrl1, 2, kLocal},
        {kUrl1, 3, kLocal}  // Most recent.
    };
    std::vector<HistoryEntry> results;
    AddQueryResults(test_data, &results);
    BrowsingHistoryService::MergeDuplicateResults(&results);

    ASSERT_EQ(2U, results.size());
    VerifyEntry(test_data[3], results[0]);
    VerifyEntry(test_data[1], results[1]);
  }

  {
    // Test that a duplicate URL on the next day is not removed.
    const std::vector<TestResult> test_data{
        {kUrl1, 0, kLocal},
        {kUrl1, 23, kLocal},
        {kUrl1, 24, kLocal},  // Most recent.
    };
    std::vector<HistoryEntry> results;
    AddQueryResults(test_data, &results);
    BrowsingHistoryService::MergeDuplicateResults(&results);

    ASSERT_EQ(2U, results.size());
    VerifyEntry(test_data[2], results[0]);
    VerifyEntry(test_data[1], results[1]);
  }

  {
    // Test multiple duplicates across multiple days.
    const std::vector<TestResult> test_data{
        // First day.
        {kUrl2, 0, kLocal},
        {kUrl1, 1, kLocal},
        {kUrl2, 2, kLocal},
        {kUrl1, 3, kLocal},

        // Second day.
        {kUrl2, 24, kLocal},
        {kUrl1, 25, kLocal},
        {kUrl2, 26, kLocal},
        {kUrl1, 27, kLocal},  // Most recent.
    };
    std::vector<HistoryEntry> results;
    AddQueryResults(test_data, &results);
    BrowsingHistoryService::MergeDuplicateResults(&results);

    ASSERT_EQ(4U, results.size());
    VerifyEntry(test_data[7], results[0]);
    VerifyEntry(test_data[6], results[1]);
    VerifyEntry(test_data[3], results[2]);
    VerifyEntry(test_data[2], results[3]);
  }

  {
    // Test that timestamps for duplicates are properly saved.
    const std::vector<TestResult> test_data{
        {kUrl1, 0, kLocal},
        {kUrl2, 1, kLocal},
        {kUrl1, 2, kLocal},
        {kUrl1, 3, kLocal}  // Most recent.
    };
    std::vector<HistoryEntry> results;
    AddQueryResults(test_data, &results);
    BrowsingHistoryService::MergeDuplicateResults(&results);

    ASSERT_EQ(2U, results.size());
    VerifyEntry(test_data[3], results[0]);
    VerifyEntry(test_data[1], results[1]);
    EXPECT_EQ(3u, results[0].all_timestamps.size());
    EXPECT_EQ(1u, results[1].all_timestamps.size());
  }
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryNoSources) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), nullptr, nullptr);

  // TODO(skym): Fix service to return results when neither local or web
  // history dependencies are given. Right now it never attempts to invoke
  // callback.
  // QueryAndVerifySingleQueryResult(false, false, {});
  service()->QueryHistory(base::string16(), QueryOptions());
  BlockUntilHistoryProcessesPendingRequests();
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryJustLocal) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), local_history(), nullptr);
  QueryAndVerifySingleQueryResult(/*reached_beginning*/ true,
                                  /*has_synced_results*/ false, {});
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryJustLocal) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), local_history(), nullptr);
  AddHistory({{kUrl1, 1, kLocal}});
  QueryAndVerifySingleQueryResult(/*reached_beginning*/ true,
                                  /*has_synced_results*/ false,
                                  {{kUrl1, 1, kLocal}});
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryJustWeb) {
  ResetService(driver(), nullptr, nullptr);
  QueryAndVerifySingleQueryResult(/*reached_beginning*/ false,
                                  /*has_synced_results*/ true, {});
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryDelayedWeb) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), nullptr, sync());
  driver()->SetWebHistory(web_history());
  QueryAndVerifySingleQueryResult(/*reached_beginning*/ false,
                                  /*has_synced_results*/ true, {});
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryJustWeb) {
  ResetService(driver(), nullptr, sync());
  AddHistory({{kUrl1, 1, kRemote}});
  QueryAndVerifySingleQueryResult(/*reached_beginning*/ false,
                                  /*has_synced_results*/ true,
                                  {{kUrl1, 1, kRemote}});
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryBothSources) {
  ResetService(driver(), local_history(), sync());
  QueryAndVerifySingleQueryResult(/*reached_beginning*/ true,
                                  /*has_synced_results*/ true, {});
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryAllSources) {
  ResetService(driver(), local_history(), sync());
  AddHistory({{kUrl1, 1, kLocal},
              {kUrl2, 2, kLocal},
              {kUrl3, 3, kRemote},
              {kUrl1, 4, kRemote}});
  QueryAndVerifySingleQueryResult(
      /*reached_beginning*/ true, /*has_synced_results*/ true,
      {{kUrl1, 4, kBoth}, {kUrl3, 3, kRemote}, {kUrl2, 2, kLocal}});
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryLocalTimeRanges) {
  AddHistory({{kUrl1, 1, kLocal},
              {kUrl2, 2, kLocal},
              {kUrl3, 3, kLocal},
              {kUrl4, 4, kLocal}});
  QueryOptions options;
  options.begin_time = OffsetToTime(2);
  options.end_time = OffsetToTime(4);
  // Having a |reached_beginning| value of false here seems counterintuitive.
  // Seems to be for paging by |begin_time| instead of |count|. If the local
  // history implementation changes, feel free to update this value, all this
  // test cares about is that BrowsingHistoryService passes the values through
  // correctly.
  VerifyQueryResult(/*reached_beginning*/ false, /*has_synced_results*/ true,
                    {{kUrl3, 3, kLocal}, {kUrl2, 2, kLocal}},
                    QueryHistory(options));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryRemoteTimeRanges) {
  AddHistory({{kUrl1, 1, kRemote},
              {kUrl2, 2, kRemote},
              {kUrl3, 3, kRemote},
              {kUrl4, 4, kRemote}});
  QueryOptions options;
  options.begin_time = OffsetToTime(2);
  options.end_time = OffsetToTime(4);
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl3, 3, kRemote}, {kUrl2, 2, kRemote}},
                    QueryHistory(options));
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryLocalPaging) {
  AddHistory({{kUrl1, 1, kLocal}, {kUrl2, 2, kLocal}, {kUrl3, 3, kLocal}});

  QueryOptions options;
  options.max_count = 2;
  VerifyQueryResult(/*reached_beginning*/ false, /*has_synced_results*/ true,
                    {{kUrl3, 3, kLocal}, {kUrl2, 2, kLocal}},
                    QueryHistory(options));

  options.end_time = OffsetToTime(2);
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true,
                    {{kUrl1, 1, kLocal}}, QueryHistory(options));

  options.end_time = Time();
  options.max_count = 3;
  VerifyQueryResult(
      /*reached_beginning*/ true, /*has_synced_results*/ true,
      {{kUrl3, 3, kLocal}, {kUrl2, 2, kLocal}, {kUrl1, 1, kLocal}},
      QueryHistory(options));

  options.end_time = OffsetToTime(1);
  VerifyQueryResult(/*reached_beginning*/ true, /*has_synced_results*/ true, {},
                    QueryHistory(options));
}

// TODO(skym, crbug.com/756097): When the concept of reaching end of sync
// results is added, add tests that verify this works correctly.

// TODO(skym, crbug.com/728727): Add more test cases, particularly when
// QueryHistory returns partial results and sequential calls are made.

TEST_F(BrowsingHistoryServiceTest, ObservingWebHistory) {
  ResetService(driver(), nullptr, sync());

  // No need to observe SyncService since we have a WebHistory already.
  EXPECT_EQ(0, sync()->GetObserverCount());

  web_history()->TriggerOnWebHistoryDeleted();
  EXPECT_EQ(1, driver()->GetHistoryDeletedCount());
}

TEST_F(BrowsingHistoryServiceTest, ObservingWebHistoryDelayedWeb) {
  driver()->SetWebHistory(nullptr);
  ResetService(driver(), nullptr, sync());

  // Since there's no WebHistory, observations should have been setup on Sync.
  EXPECT_EQ(1, sync()->GetObserverCount());

  // OnStateChanged() is a no-op if WebHistory is still inaccessible.
  service()->OnStateChanged(sync());
  EXPECT_EQ(1, sync()->GetObserverCount());

  driver()->SetWebHistory(web_history());
  // Since WebHistory is currently not being observed, triggering a history
  // deletion will not be propagated to the driver.
  web_history()->TriggerOnWebHistoryDeleted();
  EXPECT_EQ(0, driver()->GetHistoryDeletedCount());

  // Once OnStateChanged() gets called, the BrowsingHistoryService switches from
  // observing SyncService to WebHistoryService. As such, RemoveObserver should
  // have been called on SyncService, so lets verify.
  service()->OnStateChanged(sync());
  EXPECT_EQ(0, sync()->GetObserverCount());

  // Only now should deletion should be propagated through.
  web_history()->TriggerOnWebHistoryDeleted();
  EXPECT_EQ(1, driver()->GetHistoryDeletedCount());
}

}  // namespace

}  // namespace history
