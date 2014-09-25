// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/profile_writer.h"

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/importer/importer_unittest_utils.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_match.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkMatch;
using content::BrowserThread;

class TestProfileWriter : public ProfileWriter {
 public:
  explicit TestProfileWriter(Profile* profile) : ProfileWriter(profile) {}
 protected:
  virtual ~TestProfileWriter() {}
};

class ProfileWriterTest : public testing::Test {
 public:
  ProfileWriterTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {
  }
  virtual ~ProfileWriterTest() {}

  // Create test bookmark entries to be added to ProfileWriter to
  // simulate bookmark importing.
  void CreateImportedBookmarksEntries() {
    AddImportedBookmarkEntry(GURL("http://www.google.com"),
                             base::ASCIIToUTF16("Google"));
    AddImportedBookmarkEntry(GURL("http://www.yahoo.com"),
                             base::ASCIIToUTF16("Yahoo"));
  }

  // Helper function to create history entries.
  history::URLRow MakeURLRow(const char* url,
                             base::string16 title,
                             int visit_count,
                             int days_since_last_visit,
                             int typed_count) {
    history::URLRow row(GURL(url), 0);
    row.set_title(title);
    row.set_visit_count(visit_count);
    row.set_typed_count(typed_count);
    row.set_last_visit(base::Time::NowFromSystemTime() -
                       base::TimeDelta::FromDays(days_since_last_visit));
    return row;
  }

  // Create test history entries to be added to ProfileWriter to
  // simulate history importing.
  void CreateHistoryPageEntries() {
    history::URLRow row1(
        MakeURLRow("http://www.google.com", base::ASCIIToUTF16("Google"),
        3, 10, 1));
    history::URLRow row2(
        MakeURLRow("http://www.yahoo.com", base::ASCIIToUTF16("Yahoo"),
        3, 30, 10));
    pages_.push_back(row1);
    pages_.push_back(row2);
  }

  void VerifyBookmarksCount(
      const std::vector<BookmarkModel::URLAndTitle>& bookmarks_record,
      BookmarkModel* bookmark_model,
      size_t expected) {
    std::vector<BookmarkMatch> matches;
    for (size_t i = 0; i < bookmarks_record.size(); ++i) {
      bookmark_model->GetBookmarksMatching(
          bookmarks_record[i].title, 10, &matches);
      EXPECT_EQ(expected, matches.size());
      matches.clear();
    }
  }

  void VerifyHistoryCount(Profile* profile) {
    HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(profile,
                                             Profile::EXPLICIT_ACCESS);
    history::QueryOptions options;
    base::CancelableTaskTracker history_task_tracker;
    history_service->QueryHistory(
        base::string16(),
        options,
        base::Bind(&ProfileWriterTest::HistoryQueryComplete,
                   base::Unretained(this)),
        &history_task_tracker);
    base::MessageLoop::current()->Run();
  }

  void HistoryQueryComplete(history::QueryResults* results) {
    base::MessageLoop::current()->Quit();
    history_count_ = results->size();
  }

 protected:
  std::vector<ImportedBookmarkEntry> bookmarks_;
  history::URLRows pages_;
  size_t history_count_;

 private:
  void AddImportedBookmarkEntry(const GURL& url, const base::string16& title) {
    base::Time date;
    ImportedBookmarkEntry entry;
    entry.creation_time = date;
    entry.url = url;
    entry.title = title;
    entry.in_toolbar = true;
    entry.is_folder = false;
    bookmarks_.push_back(entry);
  }

  base::MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  DISALLOW_COPY_AND_ASSIGN(ProfileWriterTest);
};

// Add bookmarks via ProfileWriter to profile1 when profile2 also exists.
TEST_F(ProfileWriterTest, CheckBookmarksWithMultiProfile) {
  TestingProfile profile2;
  profile2.CreateBookmarkModel(true);

  BookmarkModel* bookmark_model2 =
      BookmarkModelFactory::GetForProfile(&profile2);
  test::WaitForBookmarkModelToLoad(bookmark_model2);
  bookmarks::AddIfNotBookmarked(
      bookmark_model2, GURL("http://www.bing.com"), base::ASCIIToUTF16("Bing"));
  TestingProfile profile1;
  profile1.CreateBookmarkModel(true);

  CreateImportedBookmarksEntries();
  BookmarkModel* bookmark_model1 =
      BookmarkModelFactory::GetForProfile(&profile1);
  test::WaitForBookmarkModelToLoad(bookmark_model1);

  scoped_refptr<TestProfileWriter> profile_writer(
      new TestProfileWriter(&profile1));
  profile_writer->AddBookmarks(bookmarks_,
                               base::ASCIIToUTF16("Imported from Firefox"));

  std::vector<BookmarkModel::URLAndTitle> url_record1;
  bookmark_model1->GetBookmarks(&url_record1);
  EXPECT_EQ(2u, url_record1.size());

  std::vector<BookmarkModel::URLAndTitle> url_record2;
  bookmark_model2->GetBookmarks(&url_record2);
  EXPECT_EQ(1u, url_record2.size());
}

// Verify that bookmarks are duplicated when added twice.
TEST_F(ProfileWriterTest, CheckBookmarksAfterWritingDataTwice) {
  TestingProfile profile;
  profile.CreateBookmarkModel(true);

  CreateImportedBookmarksEntries();
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForProfile(&profile);
  test::WaitForBookmarkModelToLoad(bookmark_model);

  scoped_refptr<TestProfileWriter> profile_writer(
      new TestProfileWriter(&profile));
  profile_writer->AddBookmarks(bookmarks_,
                               base::ASCIIToUTF16("Imported from Firefox"));
  std::vector<BookmarkModel::URLAndTitle> bookmarks_record;
  bookmark_model->GetBookmarks(&bookmarks_record);
  EXPECT_EQ(2u, bookmarks_record.size());

  VerifyBookmarksCount(bookmarks_record, bookmark_model, 1);

  profile_writer->AddBookmarks(bookmarks_,
                               base::ASCIIToUTF16("Imported from Firefox"));
  // Verify that duplicate bookmarks exist.
  VerifyBookmarksCount(bookmarks_record, bookmark_model, 2);
}

// Verify that history entires are not duplicated when added twice.
TEST_F(ProfileWriterTest, CheckHistoryAfterWritingDataTwice) {
  TestingProfile profile;
  ASSERT_TRUE(profile.CreateHistoryService(true, false));
  profile.BlockUntilHistoryProcessesPendingRequests();

  CreateHistoryPageEntries();
  scoped_refptr<TestProfileWriter> profile_writer(
      new TestProfileWriter(&profile));
  profile_writer->AddHistoryPage(pages_, history::SOURCE_FIREFOX_IMPORTED);
  VerifyHistoryCount(&profile);
  size_t original_history_count = history_count_;
  history_count_ = 0;

  profile_writer->AddHistoryPage(pages_, history::SOURCE_FIREFOX_IMPORTED);
  VerifyHistoryCount(&profile);
  EXPECT_EQ(original_history_count, history_count_);
}
