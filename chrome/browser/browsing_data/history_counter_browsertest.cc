// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/history_counter.h"

#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/history/core/browser/history_service.h"

namespace {

class HistoryCounterTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    time_ = base::Time::Now();
    service_ = HistoryServiceFactory::GetForProfileWithoutCreating(
        browser()->profile());
    SetHistoryDeletionPref(true);
    SetDeletionPeriodPref(BrowsingDataRemover::EVERYTHING);
  }

  void AddVisit(const std::string url) {
    service_->AddPage(GURL(url), time_, history::SOURCE_BROWSED);
  }

  void RevertTimeInDays(int days) {
    time_ -= base::TimeDelta::FromDays(days);
  }

  void SetHistoryDeletionPref(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kDeleteBrowsingHistory, value);
  }

  void SetDeletionPeriodPref(BrowsingDataRemover::TimePeriod period) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  void WaitForCounting() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  BrowsingDataCounter::ResultInt GetResult() {
    DCHECK(finished_);
    return result_;
  }

  void Callback(bool finished, BrowsingDataCounter::ResultInt count) {
    finished_ = finished;
    result_ = count;
    if (run_loop_ && finished)
      run_loop_->Quit();
  }

 private:
  scoped_ptr<base::RunLoop> run_loop_;
  history::HistoryService* service_;
  base::Time time_;

  bool finished_;
  BrowsingDataCounter::ResultInt result_;
};

// Tests that the counter considers duplicate visits from the same day
// to be a single item.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, DuplicateVisits) {
  AddVisit("https://www.google.com");   // 1 item
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");   // 2 items
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.example.com");  // 3 items

  RevertTimeInDays(1);
  AddVisit("https://www.google.com");   // 4 items
  AddVisit("https://www.example.com");  // 5 items
  AddVisit("https://www.example.com");

  RevertTimeInDays(1);
  AddVisit("https://www.chrome.com");   // 6 items
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.google.com");   // 7 items
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.google.com");
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");

  HistoryCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&HistoryCounterTest::Callback,
                          base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ(7u, GetResult());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, PrefChanged) {
  SetHistoryDeletionPref(false);
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");

  HistoryCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&HistoryCounterTest::Callback,
                          base::Unretained(this)));
  SetHistoryDeletionPref(true);

  WaitForCounting();
  EXPECT_EQ(2u, GetResult());
}

// Tests that the counter does not count history if the deletion
// preference is false.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, PrefIsFalse) {
  SetHistoryDeletionPref(false);
  AddVisit("https://www.google.com");

  HistoryCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&HistoryCounterTest::Callback,
                          base::Unretained(this)));
  counter.Restart();

  EXPECT_FALSE(counter.HasTrackedTasks());
}

// Tests that changing the deletion period restarts the counting, and that
// the result takes visit dates into account.
IN_PROC_BROWSER_TEST_F(HistoryCounterTest, PeriodChanged) {
  AddVisit("https://www.google.com");

  RevertTimeInDays(2);
  AddVisit("https://www.google.com");
  AddVisit("https://www.example.com");

  RevertTimeInDays(4);
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.example.com");

  RevertTimeInDays(20);
  AddVisit("https://www.google.com");
  AddVisit("https://www.chrome.com");
  AddVisit("https://www.example.com");

  RevertTimeInDays(10);
  AddVisit("https://www.example.com");
  AddVisit("https://www.example.com");
  AddVisit("https://www.example.com");

  HistoryCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&HistoryCounterTest::Callback,
                          base::Unretained(this)));

  SetDeletionPeriodPref(BrowsingDataRemover::LAST_HOUR);
  WaitForCounting();
  EXPECT_EQ(1u, GetResult());

  SetDeletionPeriodPref(BrowsingDataRemover::LAST_DAY);
  WaitForCounting();
  EXPECT_EQ(1u, GetResult());

  SetDeletionPeriodPref(BrowsingDataRemover::LAST_WEEK);
  WaitForCounting();
  EXPECT_EQ(5u, GetResult());

  SetDeletionPeriodPref(BrowsingDataRemover::FOUR_WEEKS);
  WaitForCounting();
  EXPECT_EQ(8u, GetResult());

  SetDeletionPeriodPref(BrowsingDataRemover::EVERYTHING);
  WaitForCounting();
  EXPECT_EQ(9u, GetResult());
}

}  // namespace
