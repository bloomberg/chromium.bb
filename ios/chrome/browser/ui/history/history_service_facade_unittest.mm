// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/history/history_service_facade.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/ui/history/history_entry.h"
#import "ios/chrome/browser/ui/history/history_service_facade_delegate.h"
#include "ios/web/public/test/test_web_thread.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

namespace {

struct TestResult {
  GURL url;
  int64_t hour_offset;  // Visit time in hours past the baseline time.
};

// Duplicates on the same day in the local timezone are removed, so set a
// baseline time in local time.
const base::Time baseline_time = base::Time::UnixEpoch().LocalMidnight();

// Returns true if |result| matches the test data given by |correct_result|,
// otherwise returns false.
bool ResultEquals(const history::HistoryEntry& result,
                  const TestResult& correct_result) {
  base::Time correct_time =
      baseline_time + base::TimeDelta::FromHours(correct_result.hour_offset);

  return result.time == correct_time && result.url == correct_result.url;
}

// Returns RemovedEntry using |time_offset|
HistoryServiceFacade::RemovedEntry EntryForRemoval(const GURL& url,
                                                   int64_t time_offset) {
  return HistoryServiceFacade::RemovedEntry(
      url, baseline_time + base::TimeDelta::FromHours(time_offset));
}
}  // namespace

// Mock delegate for verifying callback behavior.
@interface MockHistoryServiceFacadeDelegate
    : NSObject<HistoryServiceFacadeDelegate> {
  std::vector<history::HistoryEntry> _received_entries;
}
@property BOOL didCompleteRemoval;
@property BOOL didReceiveOtherBrowsingHistoryCallback;
- (BOOL)delegateDidReceiveResult:(const TestResult&)expected_result;
@end

@implementation MockHistoryServiceFacadeDelegate
@synthesize didCompleteRemoval = _didCompleteRemoval;
@synthesize didReceiveOtherBrowsingHistoryCallback =
    _didReceiveOtherBrowsingHistoryCallback;

- (void)historyServiceFacade:(HistoryServiceFacade*)facade
       didReceiveQueryResult:(HistoryServiceFacade::QueryResult)result {
  _received_entries = result.entries;
}

- (void)historyServiceFacade:(HistoryServiceFacade*)facade
    shouldShowNoticeAboutOtherFormsOfBrowsingHistory:(BOOL)shouldShowNotice {
  _didReceiveOtherBrowsingHistoryCallback = YES;
}

// Notifies the delegate that history entry deletion has completed.
- (void)historyServiceFacadeDidCompleteEntryRemoval:
    (HistoryServiceFacade*)facade {
  _didCompleteRemoval = YES;
}

- (BOOL)delegateDidReceiveResult:(const TestResult&)expected_result {
  for (history::HistoryEntry entry : _received_entries) {
    if (ResultEquals(entry, expected_result))
      return YES;
  }
  return NO;
}

@end

class HistoryServiceFacadeTest : public PlatformTest,
                                 public history::HistoryServiceObserver {
 public:
  HistoryServiceFacadeTest() {}
  ~HistoryServiceFacadeTest() override {}

  void SetUp() override {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
    bool success = browser_state_->CreateHistoryService(true);
    EXPECT_TRUE(success);

    mock_delegate_ = [[MockHistoryServiceFacadeDelegate alloc] init];
    history_service_facade_.reset(
        new HistoryServiceFacade(browser_state_.get(), mock_delegate_));
  }

  // Cycles the runloop until the condition is met (up to 10 seconds).
  typedef base::Callback<bool(void)> WaitCondition;
  bool WaitUntilLoop(WaitCondition condition) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    base::Time maxDate = base::Time::Now() + base::TimeDelta::FromSeconds(10);
    while (!condition.Run()) {
      if (base::Time::Now() > maxDate)
        return false;
      base::RunLoop().RunUntilIdle();
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
    }
    return true;
  }

  // Adds a visit to history.
  void AddVisit(const GURL& url, int64_t time_offset) {
    history::HistoryService* history_service =
        ios::HistoryServiceFactory::GetForBrowserStateIfExists(
            browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
    EXPECT_TRUE(history_service);

    history_service->AddObserver(this);
    base::Time time = baseline_time + base::TimeDelta::FromHours(time_offset);
    history_service->AddPage(url, time, history::VisitSource::SOURCE_BROWSED);

    // Wait until the data is in the db.
    EXPECT_TRUE(WaitUntilLoop(base::Bind(&HistoryServiceFacadeTest::VerifyVisit,
                                         base::Unretained(this), url, time)));

    history_service->RemoveObserver(this);
  }

  // history::HistoryServiceObserver
  void OnURLVisited(history::HistoryService* history_service,
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override {
    visited_url_ = row.url();
    visited_time_ = row.last_visit();
  }

  // Verify visit to |url| at |time| was observed.
  bool VerifyVisit(const GURL& url, const base::Time& time) {
    return visited_url_ == url && visited_time_ == time;
  }

  // Verify that a query result was received by delegate callback.
  bool VerifyQueryResult(const TestResult& result) {
    return [mock_delegate_ delegateDidReceiveResult:result];
  }

  // Verify that entry removal completion delegate callback was called.
  bool VerifyEntryRemoval() { return [mock_delegate_ didCompleteRemoval]; }

  // Verify that the
  // historyServiceFacade:shouldShowNoticeAboutOtherFormsOfBrowsingHistory
  // delegate callback was called.
  bool VerifyOtherHistoryCallback() {
    return [mock_delegate_ didReceiveOtherBrowsingHistoryCallback];
  }

 protected:
  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  MockHistoryServiceFacadeDelegate* mock_delegate_;
  std::unique_ptr<HistoryServiceFacade> history_service_facade_;
  GURL visited_url_;
  base::Time visited_time_;
  DISALLOW_COPY_AND_ASSIGN(HistoryServiceFacadeTest);
};

// Tests that invoking QueryHistory results in the delegate receiving an added
// history entry.
TEST_F(HistoryServiceFacadeTest, TestQueryHistory) {
  GURL url("http://www.testurl.com");
  AddVisit(url, 0);

  base::string16 query = base::string16();
  history::QueryOptions options;
  history_service_facade_->QueryHistory(query, options);

  TestResult expected_result = {url, 0};
  EXPECT_TRUE(
      WaitUntilLoop(base::Bind(&HistoryServiceFacadeTest::VerifyQueryResult,
                               base::Unretained(this), expected_result)));
}

// Tests that invoking RemoveHistoryEntries completes with callback to delegate
// method historyServiceFacadeDidCompleteEntryRemoval.
TEST_F(HistoryServiceFacadeTest, TestRemoveHistoryEntries) {
  GURL url("http://www.testurl.com");
  AddVisit(url, 0);

  std::vector<HistoryServiceFacade::RemovedEntry> entries_to_remove{
      EntryForRemoval(url, 0)};
  history_service_facade_->RemoveHistoryEntries(entries_to_remove);
  EXPECT_TRUE(WaitUntilLoop(base::Bind(
      &HistoryServiceFacadeTest::VerifyEntryRemoval, base::Unretained(this))));
}

// Tests that invoking QueryOtherFormsOfBrowsingHistory completes with callback
// to delegate method
// historyServiceFacade:shouldShowNoticeAboutOtherFormsOfBrowsingHistory:.
TEST_F(HistoryServiceFacadeTest, TestQueryOtherFormsOfBrowsingHistory) {
  history_service_facade_->QueryOtherFormsOfBrowsingHistory();
  EXPECT_TRUE(WaitUntilLoop(
      base::Bind(&HistoryServiceFacadeTest::VerifyOtherHistoryCallback,
                 base::Unretained(this))));
}
