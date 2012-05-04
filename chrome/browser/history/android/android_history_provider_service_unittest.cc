// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/android_history_provider_service.h"

#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/android/android_history_types.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::Bind;
using base::Time;
using content::BrowserThread;
using history::AndroidStatement;
using history::HistoryAndBookmarkRow;
using history::SearchRow;

// The test cases in this file don't intent to test the detail features of
// Android content provider which have been covered by
// android_provider_backend_unittest.cc, instead, they verify the code path to
// AndroidProviderBackend working fine.

class AndroidHistoryProviderServiceTest : public testing::Test {
 public:
  AndroidHistoryProviderServiceTest()
      : profile_manager_(
          static_cast<TestingBrowserProcess*>(g_browser_process)),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {
  }
  virtual ~AndroidHistoryProviderServiceTest() {
  }

 protected:
  virtual void SetUp() OVERRIDE {
    // Setup the testing profile, so the bookmark_model_sql_handler could
    // get the bookmark model from it.
    ASSERT_TRUE(profile_manager_.SetUp());
    // It seems that the name has to be chrome::kInitialProfile, so it
    // could be found by ProfileManager::GetLastUsedProfile().
    testing_profile_ = profile_manager_.CreateTestingProfile(
        chrome::kInitialProfile);

    testing_profile_->CreateBookmarkModel(true);
    testing_profile_->BlockUntilBookmarkModelLoaded();
    testing_profile_->CreateHistoryService(true, false);
    service_.reset(new AndroidHistoryProviderService(testing_profile_));
  }

  virtual void TearDown() OVERRIDE {
    testing_profile_->DestroyHistoryService();
    profile_manager_.DeleteTestingProfile(chrome::kInitialProfile);
    testing_profile_=NULL;
  }

 protected:
  TestingProfileManager profile_manager_;
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_ptr<AndroidHistoryProviderService> service_;
  CancelableRequestConsumer cancelable_consumer_;
  TestingProfile* testing_profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidHistoryProviderServiceTest);
};

class CallbackHelper : public base::RefCountedThreadSafe<CallbackHelper> {
 public:
  CallbackHelper()
      : success_(false),
        statement_(NULL),
        cursor_position_(0),
        count_(0) {
  }

  bool success() const {
    return success_;
  }

  AndroidStatement* statement() const {
    return statement_;
  }

  int cursor_position() const {
    return cursor_position_;
  }

  int count() const {
    return count_;
  }

  void OnInserted(AndroidHistoryProviderService::Handle handle,
                  bool success,
                  int64 id) {
    success_ = success;
    MessageLoop::current()->Quit();
  }

  void OnQueryResult(AndroidHistoryProviderService::Handle handle,
                     bool success,
                     AndroidStatement* statement) {
    success_ = success;
    statement_ = statement;
    MessageLoop::current()->Quit();
  }

  void OnUpdated(AndroidHistoryProviderService::Handle handle,
                 bool success,
                 int count) {
    success_ = success;
    count_ = count;
    MessageLoop::current()->Quit();
  }

  void OnDeleted(AndroidHistoryProviderService::Handle handle,
                 bool success,
                 int count) {
    success_ = success;
    count_ = count;
    MessageLoop::current()->Quit();
  }

  void OnStatementMoved(AndroidHistoryProviderService::Handle handle,
                        int cursor_position) {
    cursor_position_ = cursor_position;
    MessageLoop::current()->Quit();
  }

 private:
  friend class base::RefCountedThreadSafe<CallbackHelper>;
  ~CallbackHelper() {
  }

  bool success_;
  AndroidStatement* statement_;
  int cursor_position_;
  int count_;

  DISALLOW_COPY_AND_ASSIGN(CallbackHelper);
};

TEST_F(AndroidHistoryProviderServiceTest, TestHistoryAndBookmark) {
  HistoryAndBookmarkRow row;
  row.set_raw_url("http://www.google.com");
  row.set_url(GURL("http://www.google.com"));

  scoped_refptr<CallbackHelper> callback(new CallbackHelper());

  // Insert a row and verify it succeeded.
  service_->InsertHistoryAndBookmark(row, &cancelable_consumer_,
      Bind(&CallbackHelper::OnInserted, callback.get()));

  MessageLoop::current()->Run();
  EXPECT_TRUE(callback->success());

  std::vector<HistoryAndBookmarkRow::ColumnID> projections;
  projections.push_back(HistoryAndBookmarkRow::ID);

  // Query the inserted row.
  service_->QueryHistoryAndBookmarks(projections, std::string(),
      std::vector<string16>(), std::string(), &cancelable_consumer_,
      Bind(&CallbackHelper::OnQueryResult, callback.get()));
  MessageLoop::current()->Run();
  ASSERT_TRUE(callback->success());

  // Move the cursor to the begining and verify whether we could get
  // the same result.
  AndroidStatement* statement = callback->statement();
  service_->MoveStatement(statement, 0, -1, &cancelable_consumer_,
      Bind(&CallbackHelper::OnStatementMoved, callback.get()));
  MessageLoop::current()->Run();
  EXPECT_EQ(-1, callback->cursor_position());
  EXPECT_TRUE(callback->statement()->statement()->Step());
  EXPECT_FALSE(callback->statement()->statement()->Step());
  service_->CloseStatement(statement);

  // Update the row.
  HistoryAndBookmarkRow update_row;
  update_row.set_visit_count(3);
  service_->UpdateHistoryAndBookmarks(update_row, std::string(),
      std::vector<string16>(), &cancelable_consumer_,
      Bind(&CallbackHelper::OnUpdated, callback.get()));
  MessageLoop::current()->Run();
  EXPECT_TRUE(callback->success());
  EXPECT_EQ(1, callback->count());

  // Delete the row.
  service_->DeleteHistoryAndBookmarks(std::string(), std::vector<string16>(),
      &cancelable_consumer_, Bind(&CallbackHelper::OnDeleted, callback.get()));
  MessageLoop::current()->Run();
  EXPECT_TRUE(callback->success());
  EXPECT_EQ(1, callback->count());
}

TEST_F(AndroidHistoryProviderServiceTest, TestSearchTerm) {
  SearchRow search_row;
  search_row.set_search_term(UTF8ToUTF16("google"));
  search_row.set_url(GURL("http://google.com"));
  search_row.set_template_url_id(1);
  search_row.set_search_time(Time::Now());

  scoped_refptr<CallbackHelper> callback(new CallbackHelper());

  // Insert a row and verify it succeeded.
  service_->InsertSearchTerm(search_row, &cancelable_consumer_,
      Bind(&CallbackHelper::OnInserted, callback.get()));

  MessageLoop::current()->Run();
  EXPECT_TRUE(callback->success());

  std::vector<SearchRow::ColumnID> projections;
  projections.push_back(SearchRow::ID);

  // Query the inserted row.
  service_->QuerySearchTerms(projections, std::string(),
      std::vector<string16>(), std::string(), &cancelable_consumer_,
      Bind(&CallbackHelper::OnQueryResult, callback.get()));
  MessageLoop::current()->Run();
  ASSERT_TRUE(callback->success());

  // Move the cursor to the begining and verify whether we could get
  // the same result.
  AndroidStatement* statement = callback->statement();
  service_->MoveStatement(statement, 0, -1, &cancelable_consumer_,
      Bind(&CallbackHelper::OnStatementMoved, callback.get()));
  MessageLoop::current()->Run();
  EXPECT_EQ(-1, callback->cursor_position());
  EXPECT_TRUE(callback->statement()->statement()->Step());
  EXPECT_FALSE(callback->statement()->statement()->Step());
  service_->CloseStatement(statement);

  // Update the row.
  SearchRow update_row;
  update_row.set_search_time(Time::Now());
  service_->UpdateSearchTerms(update_row, std::string(),
      std::vector<string16>(), &cancelable_consumer_,
      Bind(&CallbackHelper::OnUpdated, callback.get()));
  MessageLoop::current()->Run();
  EXPECT_TRUE(callback->success());
  EXPECT_EQ(1, callback->count());

  // Delete the row.
  service_->DeleteSearchTerms(std::string(), std::vector<string16>(),
      &cancelable_consumer_, Bind(&CallbackHelper::OnDeleted, callback.get()));
  MessageLoop::current()->Run();
  EXPECT_TRUE(callback->success());
  EXPECT_EQ(1, callback->count());
}

}  // namespace
