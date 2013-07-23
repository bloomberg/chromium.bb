// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_prompt_controller.h"

#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/bookmarks/bookmark_prompt_prefs.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

class BookmarkPromptControllerTest : public BrowserWithTestWindowTest {
 public:
  BookmarkPromptControllerTest() : field_trial_list_(NULL), page_id_(0) {
    base::FieldTrialList::CreateFieldTrial("BookmarkPrompt", "Experiment");
  }

 protected:
  int show_prompt_call_count() const {
    return static_cast<MyTestBrowserWindow*>(browser()->window())->
        show_prompt_call_count();
  }

  void Visit(const GURL& url) {
    AddTab(browser(), url);

    // Simulate page loaded.
    ++page_id_;
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::NotificationService::current()->Notify(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::Source<content::WebContents>(web_contents),
        content::Details<int>(&page_id_));

    // Wait until HistoryService::QueryURL completion.
    static_cast<TestingProfile*>(browser()->profile())->
        BlockUntilHistoryProcessesPendingRequests();
  }

 private:
  class MyTestBrowserWindow : public TestBrowserWindow {
   public:
    MyTestBrowserWindow() : show_prompt_call_count_(0) {}
    int show_prompt_call_count() { return show_prompt_call_count_; }

   private:
    virtual bool IsActive() const OVERRIDE { return true; }
    virtual void ShowBookmarkPrompt() OVERRIDE { ++show_prompt_call_count_; }
    int show_prompt_call_count_;

    DISALLOW_COPY_AND_ASSIGN(MyTestBrowserWindow);
  };

  virtual void SetUp() OVERRIDE {
    TestingBrowserProcess::GetGlobal()->
        SetBookmarkPromptController(new BookmarkPromptController);
    BrowserWithTestWindowTest::SetUp();
    ASSERT_TRUE(static_cast<TestingProfile*>(browser()->profile())->
        CreateHistoryService(true, false));
    static_cast<TestingProfile*>(browser()->profile())->
        BlockUntilHistoryIndexIsRefreshed();
    // Simulate browser activation.
    BrowserList::SetLastActive(browser());
  }

  virtual void TearDown() OVERRIDE {
    TestingBrowserProcess::GetGlobal()->
        SetBookmarkPromptController(NULL);
    static_cast<TestingProfile*>(browser()->profile())->
        DestroyHistoryService();
    BrowserWithTestWindowTest::TearDown();
  }

  virtual BrowserWindow* CreateBrowserWindow() OVERRIDE {
    return new MyTestBrowserWindow;
  }

  base::FieldTrialList field_trial_list_;
  int page_id_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkPromptControllerTest);
};

// Test for maximum prompt impression count.
TEST_F(BookmarkPromptControllerTest, MaxImpressionCountTest) {
  BookmarkPromptPrefs prefs(browser()->profile()->GetPrefs());

  // Simulate we've already display bookmark prompt many times.
  for (int i = 0; i < BookmarkPromptController::kMaxPromptImpressionCount;
       ++i) {
    prefs.IncrementPromptImpressionCount();
  }

  // Visit the URL many times to display bookmark prompt.
  GURL url("http://foo");
  for (int visit_count = 1;
       visit_count <= BookmarkPromptController::kVisitCountForSessionTrigger;
       ++visit_count) {
    Visit(url);
  }

  // Although, we don't display bookmark prompt since we've already displayed
  // many times.
  EXPECT_EQ(0, show_prompt_call_count());
  EXPECT_EQ(BookmarkPromptController::kMaxPromptImpressionCount,
            prefs.GetPromptImpressionCount());
}

// Test for session trigger and permanent trigger.
TEST_F(BookmarkPromptControllerTest, TriggerTest) {
  BookmarkPromptPrefs prefs(browser()->profile()->GetPrefs());

  GURL url("http://foo");
  for (int visit_count = 1;
       visit_count < BookmarkPromptController::kVisitCountForPermanentTrigger;
       ++visit_count) {
    Visit(url);
    if (visit_count < BookmarkPromptController::kVisitCountForSessionTrigger) {
      EXPECT_EQ(0, show_prompt_call_count());
      EXPECT_EQ(0, prefs.GetPromptImpressionCount());
    } else {
      EXPECT_EQ(1, show_prompt_call_count());
      EXPECT_EQ(1, prefs.GetPromptImpressionCount());
    }
  }

  Visit(url);

  EXPECT_EQ(2, show_prompt_call_count());
  EXPECT_EQ(2, prefs.GetPromptImpressionCount());
}
