// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_remover.h"

#include "base/message_loop.h"
#include "chrome/browser/history/history.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BrowsingDataRemoverTester : public BrowsingDataRemover::Observer {
 public:
  BrowsingDataRemoverTester() {}
  virtual ~BrowsingDataRemoverTester() {}

  void BlockUntilNotified() {
    MessageLoop::current()->Run();
  }

 protected:
  // BrowsingDataRemover::Observer implementation.
  virtual void OnBrowsingDataRemoverDone() {
    Notify();
  }

  void Notify() {
    MessageLoop::current()->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverTester);
};

class RemoveHistoryTester : public BrowsingDataRemoverTester {
 public:
  explicit RemoveHistoryTester(TestingProfile* profile)
      : query_url_success_(false) {
    profile->CreateHistoryService(true, false);
    history_service_ = profile->GetHistoryService(Profile::EXPLICIT_ACCESS);
  }

  // Returns true, if the given URL exists in the history service.
  bool HistoryContainsURL(const GURL& url) {
    history_service_->QueryURL(
        url,
        true,
        &consumer_,
        NewCallback(this, &RemoveHistoryTester::SaveResultAndQuit));
    BlockUntilNotified();
    return query_url_success_;
  }

  void AddHistory(const GURL& url, base::Time time) {
    history_service_->AddPage(url, time, NULL, 0, GURL(), PageTransition::LINK,
        history::RedirectList(), history::SOURCE_BROWSED, false);
  }

 private:
  // Callback for HistoryService::QueryURL.
  void SaveResultAndQuit(HistoryService::Handle,
                         bool success,
                         const history::URLRow*,
                         history::VisitVector*) {
    query_url_success_ = success;
    Notify();
  }


  // For History requests.
  CancelableRequestConsumer consumer_;
  bool query_url_success_;

  // TestingProfile owns the history service; we shouldn't delete it.
  HistoryService* history_service_;

  DISALLOW_COPY_AND_ASSIGN(RemoveHistoryTester);
};

class BrowsingDataRemoverTest : public testing::Test {
 public:
  BrowsingDataRemoverTest() {}
  virtual ~BrowsingDataRemoverTest() {}

  void BlockUntilBrowsingDataRemoved(BrowsingDataRemover::TimePeriod period,
                                     base::Time delete_end,
                                     int remove_mask,
                                     BrowsingDataRemoverTester* tester) {
    BrowsingDataRemover* remover = new BrowsingDataRemover(&profile_,
        period, delete_end);
    remover->AddObserver(tester);

    // BrowsingDataRemover deletes itself when it completes.
    remover->Remove(remove_mask);
    tester->BlockUntilNotified();
  }

  TestingProfile* GetProfile() {
    return &profile_;
  }

 private:
  // message_loop_ must be defined before profile_ to prevent explosions.
  MessageLoopForUI message_loop_;
  TestingProfile profile_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverTest);
};

TEST_F(BrowsingDataRemoverTest, RemoveHistoryForever) {
  scoped_ptr<RemoveHistoryTester> tester(
      new RemoveHistoryTester(GetProfile()));

  GURL test_url("http://example.com/");
  tester->AddHistory(test_url, base::Time::Now());
  ASSERT_TRUE(tester->HistoryContainsURL(test_url));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      base::Time::Now(), BrowsingDataRemover::REMOVE_HISTORY, tester.get());

  EXPECT_FALSE(tester->HistoryContainsURL(test_url));
}

TEST_F(BrowsingDataRemoverTest, RemoveHistoryForLastHour) {
  scoped_ptr<RemoveHistoryTester> tester(
      new RemoveHistoryTester(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  GURL test_url1("http://example.com/1");
  GURL test_url2("http://example.com/2");
  tester->AddHistory(test_url1, base::Time::Now());
  tester->AddHistory(test_url2, two_hours_ago);
  ASSERT_TRUE(tester->HistoryContainsURL(test_url1));
  ASSERT_TRUE(tester->HistoryContainsURL(test_url2));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      base::Time::Now(), BrowsingDataRemover::REMOVE_HISTORY, tester.get());

  EXPECT_FALSE(tester->HistoryContainsURL(test_url1));
  EXPECT_TRUE(tester->HistoryContainsURL(test_url2));
}

}  // namespace
