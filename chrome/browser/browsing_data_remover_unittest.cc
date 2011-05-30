// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_remover.h"

#include "base/message_loop.h"
#include "chrome/browser/history/history.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BrowsingDataRemoverTest : public testing::Test,
                                public BrowsingDataRemover::Observer {
 public:
  BrowsingDataRemoverTest()
      : query_url_success_(false) {
  }
  virtual ~BrowsingDataRemoverTest() {}

 protected:
  // Returns true, if the given URL exists in the history service.
  bool QueryURL(HistoryService* history_service, const GURL& url) {
    history_service->QueryURL(
        url,
        true,
        &consumer_,
        NewCallback(this, &BrowsingDataRemoverTest::SaveResultAndQuit));
    MessageLoop::current()->Run();
    return query_url_success_;
  }

  // BrowsingDataRemover::Observer implementation.
  virtual void OnBrowsingDataRemoverDone() {
    MessageLoop::current()->Quit();
  }

 private:
  // Callback for HistoryService::QueryURL.
  void SaveResultAndQuit(HistoryService::Handle,
                         bool success,
                         const history::URLRow*,
                         history::VisitVector*) {
    query_url_success_ = success;
    MessageLoop::current()->Quit();
  }

  MessageLoopForUI message_loop_;

  // For history requests.
  CancelableRequestConsumer consumer_;
  bool query_url_success_;
};

TEST_F(BrowsingDataRemoverTest, RemoveAllHistory) {
  TestingProfile profile;
  profile.CreateHistoryService(true, false);
  HistoryService* history =
      profile.GetHistoryService(Profile::EXPLICIT_ACCESS);
  GURL test_url("http://test.com/");
  history->AddPage(test_url, history::SOURCE_BROWSED);
  ASSERT_TRUE(QueryURL(history, test_url));

  BrowsingDataRemover* remover = new BrowsingDataRemover(
      &profile, BrowsingDataRemover::EVERYTHING, base::Time::Now());
  remover->AddObserver(this);

  // BrowsingDataRemover deletes itself when it completes.
  remover->Remove(BrowsingDataRemover::REMOVE_HISTORY);
  MessageLoop::current()->Run();

  EXPECT_FALSE(QueryURL(history, test_url));

  profile.DestroyHistoryService();
}

}  // namespace
