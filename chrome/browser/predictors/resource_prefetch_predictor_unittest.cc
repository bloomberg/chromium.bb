// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ContainerEq;
using testing::Pointee;
using testing::SetArgPointee;

namespace predictors {

typedef ResourcePrefetchPredictor::URLRequestSummary URLRequestSummary;
typedef ResourcePrefetchPredictorTables::UrlTableRow UrlTableRow;
typedef ResourcePrefetchPredictorTables::UrlTableRows UrlTableRows;

class MockResourcePrefetchPredictorTables
    : public ResourcePrefetchPredictorTables {
 public:
  MockResourcePrefetchPredictorTables() { }

  MOCK_METHOD1(GetAllRows, void(std::vector<UrlTableRow>* url_row_buffer));
  MOCK_METHOD2(UpdateRowsForUrl, void(const GURL& main_page_url,
                                      const UrlTableRows& row_buffer));
  MOCK_METHOD1(DeleteRowsForUrls, void(const std::vector<GURL>& urls));
  MOCK_METHOD0(DeleteAllRows, void());

 private:
  ~MockResourcePrefetchPredictorTables() { }
};

class ResourcePrefetchPredictorTest : public testing::Test {
 public:
  ResourcePrefetchPredictorTest();
  ~ResourcePrefetchPredictorTest();
  void SetUp() OVERRIDE;
  void TearDown() OVERRIDE;

 protected:
  void AddUrlToHistory(const std::string& url,
                       int visit_count,
                       time_t last_visit) {
    history::URLRow row = history::URLRow(GURL(url));
    row.set_visit_count(visit_count);
    row.set_last_visit(base::Time::FromTimeT(last_visit));
    url_db_->AddURL(row);
  }

  NavigationID CreateNavigationID(int process_id,
                                  int render_view_id,
                                  const std::string& main_frame_url) {
    NavigationID navigation_id;
    navigation_id.render_process_id = process_id;
    navigation_id.render_view_id = render_view_id;
    navigation_id.main_frame_url = GURL(main_frame_url);
    navigation_id.creation_time = base::TimeTicks::Now();
    return navigation_id;
  }

  ResourcePrefetchPredictor::URLRequestSummary CreateURLRequestSummary(
      int process_id,
      int render_view_id,
      const std::string& main_frame_url,
      const std::string& resource_url,
      ResourceType::Type resource_type,
      const std::string& mime_type,
      bool was_cached) {
    ResourcePrefetchPredictor::URLRequestSummary summary;
    summary.navigation_id = CreateNavigationID(process_id, render_view_id,
                                               main_frame_url);
    summary.resource_url = GURL(resource_url);
    summary.resource_type = resource_type;
    summary.mime_type = mime_type;
    summary.was_cached = was_cached;
    return summary;
  }

  bool URLRequestSummaryAreEqual(const URLRequestSummary& lhs,
                                 const URLRequestSummary& rhs) {
    return lhs.navigation_id == rhs.navigation_id &&
        lhs.resource_url == rhs.resource_url &&
        lhs.resource_type == rhs.resource_type &&
        lhs.mime_type == rhs.mime_type &&
        lhs.was_cached == rhs.was_cached;
  }

  void ResetPredictor() {
    ResourcePrefetchPredictor::Config config;
    config.max_urls_to_track = 3;
    config.min_url_visit_count = 2;
    config.max_resources_per_entry = 4;
    config.max_consecutive_misses = 2;
    predictor_.reset(new ResourcePrefetchPredictor(config, &profile_));
    predictor_->SetTablesForTesting(mock_tables_);
  }

  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  TestingProfile profile_;

  scoped_ptr<ResourcePrefetchPredictor> predictor_;
  scoped_refptr<MockResourcePrefetchPredictorTables> mock_tables_;
  UrlTableRows test_url_rows_;
  history::URLDatabase* url_db_;
};

ResourcePrefetchPredictorTest::ResourcePrefetchPredictorTest()
    : loop_(MessageLoop::TYPE_DEFAULT),
      ui_thread_(content::BrowserThread::UI, &loop_),
      db_thread_(content::BrowserThread::DB, &loop_),
      predictor_(NULL),
      mock_tables_(new MockResourcePrefetchPredictorTables()) {
  test_url_rows_.push_back(UrlTableRow(
      "http://www.google.com",
      "http://google.com/style1.css",
      ResourceType::STYLESHEET,
      3, 2, 1, 1.0));
  test_url_rows_.push_back(UrlTableRow(
      "http://www.google.com",
      "http://google.com/script3.js",
      ResourceType::SCRIPT,
      4, 0, 1, 2.1));
  test_url_rows_.push_back(UrlTableRow(
      "http://www.google.com",
      "http://google.com/script4.js",
      ResourceType::SCRIPT,
      11, 0, 0, 2.1));
  test_url_rows_.push_back(UrlTableRow(
      "http://www.google.com",
      "http://google.com/image1.png",
      ResourceType::IMAGE,
      6, 3, 0, 2.2));
  test_url_rows_.push_back(UrlTableRow(
      "http://www.google.com",
      "http://google.com/a.font",
      ResourceType::LAST_TYPE,
      2, 0, 0, 5.1));

  test_url_rows_.push_back(UrlTableRow(
      "http://www.reddit.com",
      "http://reddit-resource.com/script1.js",
      ResourceType::SCRIPT,
      4, 0, 1, 1.0));
  test_url_rows_.push_back(UrlTableRow(
      "http://www.reddit.com",
      "http://reddit-resource.com/script2.js",
      ResourceType::SCRIPT,
      2, 0, 0, 2.1));

  test_url_rows_.push_back(UrlTableRow(
      "http://www.yahoo.com",
      "http://google.com/image.png",
      ResourceType::IMAGE,
      20, 1, 0, 10.0));
}

ResourcePrefetchPredictorTest::~ResourcePrefetchPredictorTest() {
}

void ResourcePrefetchPredictorTest::SetUp() {
  profile_.CreateHistoryService(true, false);
  profile_.BlockUntilHistoryProcessesPendingRequests();
  EXPECT_TRUE(profile_.GetHistoryService(Profile::EXPLICIT_ACCESS));
  url_db_ = profile_.GetHistoryService(
      Profile::EXPLICIT_ACCESS)->InMemoryDatabase();

  // Initialize the predictor with empty data.
  ResetPredictor();
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::NOT_INITIALIZED);
  EXPECT_CALL(*mock_tables_, GetAllRows(Pointee(ContainerEq(UrlTableRows()))));
  predictor_->LazilyInitialize();
  profile_.BlockUntilHistoryProcessesPendingRequests();
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::INITIALIZED);
}

void ResourcePrefetchPredictorTest::TearDown() {
  predictor_.reset(NULL);
  profile_.DestroyHistoryService();
}

TEST_F(ResourcePrefetchPredictorTest, LazilyInitializeEmpty) {
  // Tests that the predictor initializes correctly without any data.
  EXPECT_TRUE(predictor_->url_table_cache_.empty());
}

TEST_F(ResourcePrefetchPredictorTest, LazilyInitializeWithData) {
  // Tests that the history and the db tables data are loaded correctly.
  AddUrlToHistory("http://www.google.com", 4, 12345);
  AddUrlToHistory("http://www.yahoo.com", 2, 12346);

  EXPECT_CALL(*mock_tables_, GetAllRows(Pointee(ContainerEq(UrlTableRows()))))
      .WillOnce(SetArgPointee<0>(test_url_rows_));

  std::vector<GURL> urls_to_delete;
  urls_to_delete.push_back(GURL("http://www.reddit.com"));
  EXPECT_CALL(*mock_tables_, DeleteRowsForUrls(ContainerEq(urls_to_delete)));

  ResetPredictor();
  predictor_->LazilyInitialize();
  loop_.RunAllPending();

  // Test that the internal variables correctly initialized.
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::INITIALIZED);
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());
  EXPECT_EQ(2, static_cast<int>(predictor_->url_table_cache_.size()));

  UrlTableRows google_rows(test_url_rows_.begin(),
                           test_url_rows_.begin() + 5);
  std::sort(google_rows.begin(), google_rows.end(),
            ResourcePrefetchPredictorTables::UrlTableRowSorter());
  EXPECT_EQ(predictor_->url_table_cache_[GURL("http://www.google.com")].rows,
            google_rows);
  EXPECT_EQ(
      predictor_->url_table_cache_[GURL("http://www.google.com")].last_visit,
      base::Time::FromTimeT(12345));

  UrlTableRows yahoo_rows(test_url_rows_.begin() + 7,
                          test_url_rows_.begin() + 8);
  EXPECT_EQ(predictor_->url_table_cache_[GURL("http://www.yahoo.com")].rows,
            yahoo_rows);
  EXPECT_EQ(
      predictor_->url_table_cache_[GURL("http://www.yahoo.com")].last_visit,
      base::Time::FromTimeT(12346));
}

TEST_F(ResourcePrefetchPredictorTest, NavigationNotRecorded) {
  // Single navigation but history count is low, so should not record.
  AddUrlToHistory("http://www.google.com", 1, 12345);

  URLRequestSummary main_frame = CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://www.google.com",
      ResourceType::MAIN_FRAME, "", false);
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1, static_cast<int>(predictor_->inflight_navigations_.size()));

  // Now add a few subresources.
  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/style1.css",
      ResourceType::STYLESHEET, "text/css", false);
  predictor_->RecordUrlResponse(resource1);
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/script1.js",
      ResourceType::SCRIPT, "text/javascript", false);
  predictor_->RecordUrlResponse(resource2);
  URLRequestSummary resource3 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/script2.js",
      ResourceType::SCRIPT, "text/javascript", false);
  predictor_->RecordUrlResponse(resource3);
  predictor_->OnNavigationComplete(main_frame.navigation_id);
}

TEST_F(ResourcePrefetchPredictorTest, NavigationUrlNotInDB) {
  // Single navigation that will be recorded. Will check for duplicate
  // resources and also for number of resources saved.
  AddUrlToHistory("http://www.google.com", 4, 12345);

  URLRequestSummary main_frame = CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://www.google.com",
      ResourceType::MAIN_FRAME, "", false);
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1, static_cast<int>(predictor_->inflight_navigations_.size()));

  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/style1.css",
      ResourceType::STYLESHEET, "text/css", false);
  predictor_->RecordUrlResponse(resource1);
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/script1.js",
      ResourceType::SCRIPT, "text/javascript", false);
  predictor_->RecordUrlResponse(resource2);
  URLRequestSummary resource3 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/script2.js",
      ResourceType::SCRIPT, "text/javascript", false);
  predictor_->RecordUrlResponse(resource3);
  URLRequestSummary resource4 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/script1.js",
      ResourceType::SCRIPT, "text/javascript", true);
  predictor_->RecordUrlResponse(resource4);
  URLRequestSummary resource5 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/image1.png",
      ResourceType::IMAGE, "image/png", false);
  predictor_->RecordUrlResponse(resource5);
  URLRequestSummary resource6 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/image2.png",
      ResourceType::IMAGE, "image/png", false);
  predictor_->RecordUrlResponse(resource6);
  URLRequestSummary resource7 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/style2.css",
      ResourceType::STYLESHEET, "text/css", false);
  predictor_->OnSubresourceLoadedFromMemory(
      resource7.navigation_id,
      resource7.resource_url,
      resource7.mime_type,
      resource7.resource_type);

  UrlTableRows db_rows;
  db_rows.push_back(UrlTableRow(
      "http://www.google.com", "http://google.com/style1.css",
      ResourceType::STYLESHEET, 1, 0, 0, 1.0));
  db_rows.push_back(UrlTableRow(
      "http://www.google.com", "http://google.com/script1.js",
      ResourceType::SCRIPT, 1, 0, 0, 2.0));
  db_rows.push_back(UrlTableRow(
      "http://www.google.com", "http://google.com/script2.js",
      ResourceType::SCRIPT, 1, 0, 0, 3.0));
  db_rows.push_back(UrlTableRow(
      "http://www.google.com", "http://google.com/style2.css",
      ResourceType::STYLESHEET, 1, 0, 0, 7.0));
  EXPECT_CALL(*mock_tables_,
              UpdateRowsForUrl(GURL("http://www.google.com"),
                               ContainerEq(db_rows)));

  predictor_->OnNavigationComplete(main_frame.navigation_id);
}

TEST_F(ResourcePrefetchPredictorTest, NavigationUrlInDB) {
  // Tests that navigation is recoreded correctly for URL already present in
  // the database cache.
  AddUrlToHistory("http://www.google.com", 4, 12345);

  EXPECT_CALL(*mock_tables_, GetAllRows(Pointee(ContainerEq(UrlTableRows()))))
      .WillOnce(SetArgPointee<0>(test_url_rows_));

  std::vector<GURL> urls_to_delete;
  urls_to_delete.push_back(GURL("http://www.reddit.com"));
  urls_to_delete.push_back(GURL("http://www.yahoo.com"));
  EXPECT_CALL(*mock_tables_, DeleteRowsForUrls(ContainerEq(urls_to_delete)));

  ResetPredictor();
  predictor_->LazilyInitialize();
  loop_.RunAllPending();

  URLRequestSummary main_frame = CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://www.google.com",
      ResourceType::MAIN_FRAME, "", false);
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1, static_cast<int>(predictor_->inflight_navigations_.size()));

  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/style1.css",
      ResourceType::STYLESHEET, "text/css", false);
  predictor_->RecordUrlResponse(resource1);
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/script1.js",
      ResourceType::SCRIPT, "text/javascript", false);
  predictor_->RecordUrlResponse(resource2);
  URLRequestSummary resource3 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/script2.js",
      ResourceType::SCRIPT, "text/javascript", false);
  predictor_->RecordUrlResponse(resource3);
  URLRequestSummary resource4 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/script1.js",
      ResourceType::SCRIPT, "text/javascript", true);
  predictor_->RecordUrlResponse(resource4);
  URLRequestSummary resource5 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/image1.png",
      ResourceType::IMAGE, "image/png", false);
  predictor_->RecordUrlResponse(resource5);
  URLRequestSummary resource6 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/image2.png",
      ResourceType::IMAGE, "image/png", false);
  predictor_->RecordUrlResponse(resource6);
  URLRequestSummary resource7 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/style2.css",
      ResourceType::STYLESHEET, "text/css", false);
  predictor_->OnSubresourceLoadedFromMemory(
      resource7.navigation_id,
      resource7.resource_url,
      resource7.mime_type,
      resource7.resource_type);

  UrlTableRows db_rows;
  db_rows.push_back(UrlTableRow(
      "http://www.google.com", "http://google.com/style1.css",
      ResourceType::STYLESHEET, 4, 2, 0, 1.0));
  db_rows.push_back(UrlTableRow(
      "http://www.google.com", "http://google.com/script1.js",
      ResourceType::SCRIPT, 1, 0, 0, 2.0));
  db_rows.push_back(UrlTableRow(
      "http://www.google.com", "http://google.com/script4.js",
      ResourceType::SCRIPT, 11, 1, 1, 2.1));
  db_rows.push_back(UrlTableRow(
      "http://www.google.com", "http://google.com/script2.js",
      ResourceType::SCRIPT, 1, 0, 0, 3.0));
  EXPECT_CALL(*mock_tables_,
              UpdateRowsForUrl(GURL("http://www.google.com"),
                               ContainerEq(db_rows)));

  predictor_->OnNavigationComplete(main_frame.navigation_id);
}

TEST_F(ResourcePrefetchPredictorTest, NavigationUrlNotInDBAndDBFull) {
  // Tests that a URL is deleted before another is added if the cache is full.
  AddUrlToHistory("http://www.google.com", 4, 12345);
  AddUrlToHistory("http://www.reddit.com", 4, 12340);  // Should be deleted.
  AddUrlToHistory("http://www.yahoo.com", 4, 12350);
  AddUrlToHistory("http://www.nike.com", 4, 10000);

  EXPECT_CALL(*mock_tables_, GetAllRows(Pointee(ContainerEq(UrlTableRows()))))
      .WillOnce(SetArgPointee<0>(test_url_rows_));

  ResetPredictor();
  predictor_->LazilyInitialize();
  loop_.RunAllPending();
  EXPECT_EQ(3, static_cast<int>(predictor_->url_table_cache_.size()));

  URLRequestSummary main_frame = CreateURLRequestSummary(
      1, 1, "http://www.nike.com", "http://www.nike.com",
      ResourceType::MAIN_FRAME, "", false);
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1, static_cast<int>(predictor_->inflight_navigations_.size()));

  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, 1, "http://www.nike.com",  "http://nike.com/style1.css",
      ResourceType::STYLESHEET, "text/css", false);
  predictor_->RecordUrlResponse(resource1);
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, 1, "http://www.nike.com",  "http://nike.com/image2.png",
      ResourceType::IMAGE, "image/png", false);
  predictor_->RecordUrlResponse(resource2);

  std::vector<GURL> urls_to_delete;
  urls_to_delete.push_back(GURL("http://www.reddit.com"));
  EXPECT_CALL(*mock_tables_, DeleteRowsForUrls(ContainerEq(urls_to_delete)));

  UrlTableRows db_rows;
  db_rows.push_back(UrlTableRow(
      "http://www.nike.com", "http://nike.com/style1.css",
      ResourceType::STYLESHEET, 1, 0, 0, 1.0));
  db_rows.push_back(UrlTableRow(
      "http://www.nike.com", "http://nike.com/image2.png",
      ResourceType::IMAGE, 1, 0, 0, 2.0));
  EXPECT_CALL(*mock_tables_,
              UpdateRowsForUrl(GURL("http://www.nike.com"),
                               ContainerEq(db_rows)));

  predictor_->OnNavigationComplete(main_frame.navigation_id);
}

TEST_F(ResourcePrefetchPredictorTest, DeleteUrls) {
  // Add some dummy entries to cache.
  predictor_->url_table_cache_[GURL("http://www.google.com")];
  predictor_->url_table_cache_[GURL("http://www.yahoo.com")];
  predictor_->url_table_cache_[GURL("http://www.apple.com")];
  predictor_->url_table_cache_[GURL("http://www.nike.com")];
  predictor_->url_table_cache_[GURL("http://www.reddit.com")];

  history::URLRows rows;
  rows.push_back(history::URLRow(GURL("http://www.apple.com")));
  rows.push_back(history::URLRow(GURL("http://www.yahoo.com")));

  std::vector<GURL> urls_to_delete;
  urls_to_delete.push_back(GURL("http://www.apple.com"));
  urls_to_delete.push_back(GURL("http://www.yahoo.com"));
  EXPECT_CALL(*mock_tables_, DeleteRowsForUrls(ContainerEq(urls_to_delete)));
  predictor_->DeleteUrls(rows);
  EXPECT_EQ(3, static_cast<int>(predictor_->url_table_cache_.size()));

  EXPECT_CALL(*mock_tables_, DeleteAllRows());

  predictor_->DeleteAllUrls();
  EXPECT_TRUE(predictor_->url_table_cache_.empty());
}

TEST_F(ResourcePrefetchPredictorTest, OnMainFrameRequest) {
  URLRequestSummary summary1 = CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://www.google.com",
      ResourceType::MAIN_FRAME, "", false);
  URLRequestSummary summary2 = CreateURLRequestSummary(
      1, 2, "http://www.google.com", "http://www.google.com",
      ResourceType::MAIN_FRAME, "", false);
  URLRequestSummary summary3 = CreateURLRequestSummary(
      2, 1, "http://www.yahoo.com", "http://www.yahoo.com",
      ResourceType::MAIN_FRAME, "", false);

  predictor_->OnMainFrameRequest(summary1);
  EXPECT_EQ(1, static_cast<int>(predictor_->inflight_navigations_.size()));
  predictor_->OnMainFrameRequest(summary2);
  EXPECT_EQ(2, static_cast<int>(predictor_->inflight_navigations_.size()));
  predictor_->OnMainFrameRequest(summary3);
  EXPECT_EQ(3, static_cast<int>(predictor_->inflight_navigations_.size()));

  // Insert anther with same navigation id. It should replace.
  URLRequestSummary summary4 = CreateURLRequestSummary(
      1, 1, "http://www.nike.com", "http://www.nike.com",
      ResourceType::MAIN_FRAME, "", false);
  URLRequestSummary summary5 = CreateURLRequestSummary(
      1, 2, "http://www.google.com", "http://www.google.com",
      ResourceType::MAIN_FRAME, "", false);

  predictor_->OnMainFrameRequest(summary4);
  EXPECT_EQ(3, static_cast<int>(predictor_->inflight_navigations_.size()));

  // Change this creation time so that it will go away on the next insert.
  summary5.navigation_id.creation_time = base::TimeTicks::Now() -
      base::TimeDelta::FromDays(1);
  predictor_->OnMainFrameRequest(summary5);
  EXPECT_EQ(3, static_cast<int>(predictor_->inflight_navigations_.size()));

  URLRequestSummary summary6 = CreateURLRequestSummary(
      3, 1, "http://www.shoes.com", "http://www.shoes.com",
      ResourceType::MAIN_FRAME, "", false);
  predictor_->OnMainFrameRequest(summary6);
  EXPECT_EQ(3, static_cast<int>(predictor_->inflight_navigations_.size()));

  EXPECT_TRUE(predictor_->inflight_navigations_.find(summary3.navigation_id) !=
              predictor_->inflight_navigations_.end());
  EXPECT_TRUE(predictor_->inflight_navigations_.find(summary4.navigation_id) !=
              predictor_->inflight_navigations_.end());
  EXPECT_TRUE(predictor_->inflight_navigations_.find(summary6.navigation_id) !=
              predictor_->inflight_navigations_.end());
}

TEST_F(ResourcePrefetchPredictorTest, OnMainFrameRedirect) {
  URLRequestSummary summary1 = CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://www.google.com",
      ResourceType::MAIN_FRAME, "", false);
  URLRequestSummary summary2 = CreateURLRequestSummary(
      1, 2, "http://www.google.com", "http://www.google.com",
      ResourceType::MAIN_FRAME, "", false);
  URLRequestSummary summary3 = CreateURLRequestSummary(
      2, 1, "http://www.yahoo.com", "http://www.yahoo.com",
      ResourceType::MAIN_FRAME, "", false);

  predictor_->OnMainFrameRedirect(summary1);
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());

  predictor_->OnMainFrameRequest(summary1);
  EXPECT_EQ(1, static_cast<int>(predictor_->inflight_navigations_.size()));
  predictor_->OnMainFrameRequest(summary2);
  EXPECT_EQ(2, static_cast<int>(predictor_->inflight_navigations_.size()));

  predictor_->OnMainFrameRedirect(summary3);
  EXPECT_EQ(2, static_cast<int>(predictor_->inflight_navigations_.size()));
  predictor_->OnMainFrameRedirect(summary1);
  EXPECT_EQ(1, static_cast<int>(predictor_->inflight_navigations_.size()));
  predictor_->OnMainFrameRedirect(summary2);
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());
}

TEST_F(ResourcePrefetchPredictorTest, OnSubresourceResponse) {
  // If there is no inflight navigation, nothing happens.
  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/style1.css",
      ResourceType::STYLESHEET, "text/css", false);
  predictor_->OnSubresourceResponse(resource1);
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());

  // Add an inflight navigation.
  URLRequestSummary main_frame1 = CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://www.google.com",
      ResourceType::MAIN_FRAME, "", false);
  predictor_->OnMainFrameRequest(main_frame1);
  EXPECT_EQ(1, static_cast<int>(predictor_->inflight_navigations_.size()));

  // Now add a few subresources.
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/script1.js",
      ResourceType::SCRIPT, "text/javascript", false);
  URLRequestSummary resource3 = CreateURLRequestSummary(
      1, 1, "http://www.google.com",  "http://google.com/script2.js",
      ResourceType::SCRIPT, "text/javascript", false);
  predictor_->OnSubresourceResponse(resource1);
  predictor_->OnSubresourceResponse(resource2);
  predictor_->OnSubresourceResponse(resource3);

  EXPECT_EQ(1, static_cast<int>(predictor_->inflight_navigations_.size()));
  EXPECT_EQ(3, static_cast<int>(
      predictor_->inflight_navigations_[main_frame1.navigation_id].size()));
  EXPECT_TRUE(URLRequestSummaryAreEqual(
      resource1,
      predictor_->inflight_navigations_[main_frame1.navigation_id][0]));
  EXPECT_TRUE(URLRequestSummaryAreEqual(
      resource2,
      predictor_->inflight_navigations_[main_frame1.navigation_id][1]));
  EXPECT_TRUE(URLRequestSummaryAreEqual(
      resource3,
      predictor_->inflight_navigations_[main_frame1.navigation_id][2]));
}

}  // namespace predictors
