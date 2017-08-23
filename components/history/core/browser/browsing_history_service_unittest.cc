// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/browsing_history_service.h"

#include <utility>

#include "base/callback_forward.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
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

namespace history {

namespace {

const char kUrl1[] = "http://www.one.com";
const char kUrl2[] = "http://www.two.com";

struct TestResult {
  std::string url;
  int64_t hour_offset;  // Visit time in hours past the baseline time.
};

// For each item in |results|, create a new Value representing the visit, and
// insert it into |list_value|.
void AddQueryResults(
    const base::Time baseline_time,
    TestResult* test_results,
    int test_results_size,
    std::vector<BrowsingHistoryService::HistoryEntry>* results) {
  for (int i = 0; i < test_results_size; ++i) {
    BrowsingHistoryService::HistoryEntry entry;
    entry.time =
        baseline_time + base::TimeDelta::FromHours(test_results[i].hour_offset);
    entry.url = GURL(test_results[i].url);
    entry.all_timestamps.insert(entry.time.ToInternalValue());
    results->push_back(entry);
  }
}

// Returns true if |result| matches the test data given by |correct_result|,
// otherwise returns false.
bool ResultEquals(const base::Time baseline_time,
                  const BrowsingHistoryService::HistoryEntry& result,
                  const TestResult& correct_result) {
  base::Time correct_time =
      baseline_time + base::TimeDelta::FromHours(correct_result.hour_offset);

  return result.time == correct_time && result.url == GURL(correct_result.url);
}

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

  using QueryResult =
      std::pair<std::vector<BrowsingHistoryService::HistoryEntry>,
                BrowsingHistoryService::QueryResultsInfo>;
  std::vector<QueryResult> GetQuestResults() { return query_results_; }

  int GetHistoryDeletedCount() { return history_deleted_count_; }

 private:
  // BrowsingHistoryDriver implementation.
  void OnQueryComplete(
      const std::vector<BrowsingHistoryService::HistoryEntry>& results,
      const BrowsingHistoryService::QueryResultsInfo& query_results_info)
      override {
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
  BrowsingHistoryServiceTest()
      : baseline_time_(base::Time::UnixEpoch().LocalMidnight()),
        driver_(&web_history_) {
    EXPECT_TRUE(history_dir_.CreateUniqueTempDir());
    local_history_ = CreateHistoryService(history_dir_.GetPath(), true);
  }

  void BlockUntilHistoryProcessesPendingRequests() {
    base::CancelableTaskTracker tracker;
    base::RunLoop run_loop;
    local_history_->ScheduleDBTask(
        base::MakeUnique<QuittingHistoryDBTask>(run_loop.QuitWhenIdleClosure()),
        &tracker);
    run_loop.Run();
  }

  HistoryService* local_history() { return local_history_.get(); }
  TestWebHistoryService* web_history() { return &web_history_; }
  TestSyncService* sync_service() { return &sync_service_; }
  TestBrowsingHistoryDriver* driver() { return &driver_; }

  // Duplicates on the same day in the local timezone are removed, so set a
  // baseline time in local time.
  base::Time baseline_time_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<HistoryService> local_history_;
  TestWebHistoryService web_history_;
  TestSyncService sync_service_;
  TestBrowsingHistoryDriver driver_;
};

// Tests that the MergeDuplicateResults method correctly removes duplicate
// visits to the same URL on the same day.
TEST_F(BrowsingHistoryServiceTest, MergeDuplicateResults) {
  {
    // Basic test that duplicates on the same day are removed.
    TestResult test_data[] = {
        {kUrl1, 0}, {kUrl2, 1}, {kUrl1, 2}, {kUrl1, 3}  // Most recent.
    };
    std::vector<BrowsingHistoryService::HistoryEntry> results;
    AddQueryResults(baseline_time_, test_data, arraysize(test_data), &results);
    BrowsingHistoryService::MergeDuplicateResults(&results);

    ASSERT_EQ(2U, results.size());
    EXPECT_TRUE(ResultEquals(baseline_time_, results[0], test_data[3]));
    EXPECT_TRUE(ResultEquals(baseline_time_, results[1], test_data[1]));
  }

  {
    // Test that a duplicate URL on the next day is not removed.
    TestResult test_data[] = {
        {kUrl1, 0}, {kUrl1, 23}, {kUrl1, 24},  // Most recent.
    };
    std::vector<BrowsingHistoryService::HistoryEntry> results;
    AddQueryResults(baseline_time_, test_data, arraysize(test_data), &results);
    BrowsingHistoryService::MergeDuplicateResults(&results);

    ASSERT_EQ(2U, results.size());
    EXPECT_TRUE(ResultEquals(baseline_time_, results[0], test_data[2]));
    EXPECT_TRUE(ResultEquals(baseline_time_, results[1], test_data[1]));
  }

  {
    // Test multiple duplicates across multiple days.
    TestResult test_data[] = {
        // First day.
        {kUrl2, 0},
        {kUrl1, 1},
        {kUrl2, 2},
        {kUrl1, 3},

        // Second day.
        {kUrl2, 24},
        {kUrl1, 25},
        {kUrl2, 26},
        {kUrl1, 27},  // Most recent.
    };
    std::vector<BrowsingHistoryService::HistoryEntry> results;
    AddQueryResults(baseline_time_, test_data, arraysize(test_data), &results);
    BrowsingHistoryService::MergeDuplicateResults(&results);

    ASSERT_EQ(4U, results.size());
    EXPECT_TRUE(ResultEquals(baseline_time_, results[0], test_data[7]));
    EXPECT_TRUE(ResultEquals(baseline_time_, results[1], test_data[6]));
    EXPECT_TRUE(ResultEquals(baseline_time_, results[2], test_data[3]));
    EXPECT_TRUE(ResultEquals(baseline_time_, results[3], test_data[2]));
  }

  {
    // Test that timestamps for duplicates are properly saved.
    TestResult test_data[] = {
        {kUrl1, 0}, {kUrl2, 1}, {kUrl1, 2}, {kUrl1, 3}  // Most recent.
    };
    std::vector<BrowsingHistoryService::HistoryEntry> results;
    AddQueryResults(baseline_time_, test_data, arraysize(test_data), &results);
    BrowsingHistoryService::MergeDuplicateResults(&results);

    ASSERT_EQ(2U, results.size());
    EXPECT_TRUE(ResultEquals(baseline_time_, results[0], test_data[3]));
    EXPECT_TRUE(ResultEquals(baseline_time_, results[1], test_data[1]));
    EXPECT_EQ(3u, results[0].all_timestamps.size());
    EXPECT_EQ(1u, results[1].all_timestamps.size());
  }
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryNoSources) {
  driver()->SetWebHistory(nullptr);
  BrowsingHistoryService service(driver(), nullptr, nullptr);
  service.QueryHistory(base::string16(), QueryOptions());
  BlockUntilHistoryProcessesPendingRequests();

  // TODO(skym): Fix service to return results when neither local or web
  // history dependencies are given. Right now it never attempts to invoke
  // callback.
  // EXPECT_EQ(1U, driver()->GetQuestResults().size());
  // EXPECT_TRUE(driver()->GetQuestResults()[0].second.reached_beginning);
  // EXPECT_FALSE(driver()->GetQuestResults()[0].second.has_synced_results);
  // EXPECT_EQ(0U, driver()->GetQuestResults()[0].first.size());
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryJustLocal) {
  driver()->SetWebHistory(nullptr);
  BrowsingHistoryService service(driver(), local_history(), nullptr);
  service.QueryHistory(base::string16(), QueryOptions());
  BlockUntilHistoryProcessesPendingRequests();

  EXPECT_EQ(1U, driver()->GetQuestResults().size());
  EXPECT_TRUE(driver()->GetQuestResults()[0].second.reached_beginning);
  EXPECT_FALSE(driver()->GetQuestResults()[0].second.has_synced_results);
  EXPECT_EQ(0U, driver()->GetQuestResults()[0].first.size());
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryJustLocal) {
  driver()->SetWebHistory(nullptr);
  BrowsingHistoryService service(driver(), local_history(), nullptr);
  local_history()->AddPage(GURL(kUrl1), base::Time(),
                           VisitSource::SOURCE_BROWSED);
  service.QueryHistory(base::string16(), QueryOptions());
  BlockUntilHistoryProcessesPendingRequests();

  EXPECT_EQ(1U, driver()->GetQuestResults().size());
  EXPECT_TRUE(driver()->GetQuestResults()[0].second.reached_beginning);
  EXPECT_FALSE(driver()->GetQuestResults()[0].second.has_synced_results);
  std::vector<BrowsingHistoryService::HistoryEntry> entries =
      driver()->GetQuestResults()[0].first;
  EXPECT_EQ(1U, entries.size());
  EXPECT_EQ(GURL(kUrl1), entries[0].url);
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryJustWeb) {
  BrowsingHistoryService service(driver(), nullptr, nullptr);
  service.QueryHistory(base::string16(), QueryOptions());
  BlockUntilHistoryProcessesPendingRequests();

  EXPECT_EQ(1U, driver()->GetQuestResults().size());
  EXPECT_TRUE(driver()->GetQuestResults()[0].second.has_synced_results);
  EXPECT_EQ(0U, driver()->GetQuestResults()[0].first.size());
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryDelayedWeb) {
  driver()->SetWebHistory(nullptr);
  BrowsingHistoryService service(driver(), nullptr, sync_service());
  driver()->SetWebHistory(web_history());
  service.QueryHistory(base::string16(), QueryOptions());
  BlockUntilHistoryProcessesPendingRequests();

  EXPECT_EQ(1U, driver()->GetQuestResults().size());
  EXPECT_TRUE(driver()->GetQuestResults()[0].second.has_synced_results);
  EXPECT_EQ(0U, driver()->GetQuestResults()[0].first.size());
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryJustWeb) {
  // TODO(skym): Have a query that just returns a web result.
}

TEST_F(BrowsingHistoryServiceTest, EmptyQueryHistoryBothSources) {
  BrowsingHistoryService service(driver(), local_history(), sync_service());
  service.QueryHistory(base::string16(), QueryOptions());
  BlockUntilHistoryProcessesPendingRequests();

  EXPECT_EQ(1U, driver()->GetQuestResults().size());
  EXPECT_TRUE(driver()->GetQuestResults()[0].second.reached_beginning);
  EXPECT_TRUE(driver()->GetQuestResults()[0].second.has_synced_results);
  EXPECT_EQ(0U, driver()->GetQuestResults()[0].first.size());
}

TEST_F(BrowsingHistoryServiceTest, QueryHistoryAllSources) {
  BrowsingHistoryService service(driver(), local_history(), sync_service());
  local_history()->AddPage(GURL(kUrl1), base::Time(),
                           VisitSource::SOURCE_BROWSED);
  // TODO(skym): Add a web result as well.
  service.QueryHistory(base::string16(), QueryOptions());

  BlockUntilHistoryProcessesPendingRequests();
  EXPECT_EQ(1U, driver()->GetQuestResults().size());
  EXPECT_TRUE(driver()->GetQuestResults()[0].second.reached_beginning);
  EXPECT_TRUE(driver()->GetQuestResults()[0].second.has_synced_results);
  std::vector<BrowsingHistoryService::HistoryEntry> entries =
      driver()->GetQuestResults()[0].first;
  EXPECT_EQ(1U, entries.size());
  EXPECT_EQ(GURL(kUrl1), entries[0].url);
}

// TODO(skym, crbug.com/728727): Add more test cases, particularly when
// QueryHistory returns partial results and sequential calls are made.

TEST_F(BrowsingHistoryServiceTest, ObservingWebHistory) {
  BrowsingHistoryService service(driver(), nullptr, sync_service());

  // No need to observe SyncService since we have a WebHistory already.
  EXPECT_EQ(0, sync_service()->GetObserverCount());

  web_history()->TriggerOnWebHistoryDeleted();
  EXPECT_EQ(1, driver()->GetHistoryDeletedCount());
}

TEST_F(BrowsingHistoryServiceTest, ObservingWebHistoryDelayedWeb) {
  driver()->SetWebHistory(nullptr);
  BrowsingHistoryService service(driver(), nullptr, sync_service());

  // Since there's no WebHistory, observations should have been setup on Sync.
  EXPECT_EQ(1, sync_service()->GetObserverCount());

  // OnStateChanged() is a no-op if WebHistory is still inaccessible.
  service.OnStateChanged(sync_service());
  EXPECT_EQ(1, sync_service()->GetObserverCount());

  driver()->SetWebHistory(web_history());
  // Since WebHistory is currently not being observed, triggering a history
  // deletion will not be propagated to the driver.
  web_history()->TriggerOnWebHistoryDeleted();
  EXPECT_EQ(0, driver()->GetHistoryDeletedCount());

  // Once OnStateChanged() gets called, the BrowsingHistoryService switches from
  // observing SyncService to WebHistoryService. As such, RemoveObserver should
  // have been called on SyncService, so lets verify.
  service.OnStateChanged(sync_service());
  EXPECT_EQ(0, sync_service()->GetObserverCount());

  // Only now should deletion should be propagated through.
  web_history()->TriggerOnWebHistoryDeleted();
  EXPECT_EQ(1, driver()->GetHistoryDeletedCount());
}

}  // namespace

}  // namespace history
