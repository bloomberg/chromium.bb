// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <utility>
#include <vector>

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace predictors {

class ResourcePrefetchPredictorTablesTest : public testing::Test {
 public:
  ResourcePrefetchPredictorTablesTest();
  virtual ~ResourcePrefetchPredictorTablesTest();
  virtual void SetUp() OVERRIDE;

 protected:
  void TestGetAllRows();
  void TestUpdateRowsForUrl();
  void TestDeleteRowsForUrls();
  void TestDeleteAllRows();

  MessageLoop loop_;
  content::TestBrowserThread db_thread_;
  TestingProfile profile_;
  scoped_ptr<PredictorDatabase> db_;
  scoped_refptr<ResourcePrefetchPredictorTables> tables_;

 private:
  // Adds data to the URL table as well as test_url_rows_.
  void InitializeSampleUrlData();

  // Checks that the input vectors have the same elements, although they may
  // be in different orders.
  void TestUrlRowVectorsAreEqual(
      const ResourcePrefetchPredictorTables::UrlTableRows& lhs,
      const ResourcePrefetchPredictorTables::UrlTableRows& rhs) const;

  ResourcePrefetchPredictorTables::UrlTableRows test_url_rows_;
};

class ResourcePrefetchPredictorTablesReopenTest
    : public ResourcePrefetchPredictorTablesTest {
 public:
  virtual void SetUp() OVERRIDE {
    // Write data to the table, and then reopen the db.
    ResourcePrefetchPredictorTablesTest::SetUp();
    tables_ = NULL;
    db_.reset(new PredictorDatabase(&profile_));
    loop_.RunAllPending();
    tables_ = db_->resource_prefetch_tables();
  }
};

ResourcePrefetchPredictorTablesTest::ResourcePrefetchPredictorTablesTest()
    : loop_(MessageLoop::TYPE_DEFAULT),
      db_thread_(content::BrowserThread::DB, &loop_),
      db_(new PredictorDatabase(&profile_)),
      tables_(db_->resource_prefetch_tables()) {
  loop_.RunAllPending();
}

ResourcePrefetchPredictorTablesTest::~ResourcePrefetchPredictorTablesTest() {
}

void ResourcePrefetchPredictorTablesTest::SetUp() {
  tables_->DeleteAllRows();
  InitializeSampleUrlData();
}

void ResourcePrefetchPredictorTablesTest::TestGetAllRows() {
  ResourcePrefetchPredictorTables::UrlTableRows actual_rows;
  tables_->GetAllRows(&actual_rows);

  TestUrlRowVectorsAreEqual(test_url_rows_, actual_rows);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteRowsForUrls() {
  std::vector<GURL> urls_to_delete;
  urls_to_delete.push_back(GURL("http://www.google.com"));
  urls_to_delete.push_back(GURL("http://www.yahoo.com"));

  tables_->DeleteRowsForUrls(urls_to_delete);

  ResourcePrefetchPredictorTables::UrlTableRows actual_rows;
  tables_->GetAllRows(&actual_rows);

  ResourcePrefetchPredictorTables::UrlTableRows expected_rows;
  expected_rows.push_back(test_url_rows_[5]);
  expected_rows.push_back(test_url_rows_[6]);

  TestUrlRowVectorsAreEqual(expected_rows, actual_rows);
}

void ResourcePrefetchPredictorTablesTest::TestUpdateRowsForUrl() {
  ResourcePrefetchPredictorTables::UrlTableRows new_rows;
  new_rows.push_back(ResourcePrefetchPredictorTables::UrlTableRow(
      "http://www.google.com",
      "http://www.google.com/style.css",
      ResourceType::STYLESHEET,
      6, 2, 0, 1.0));
  new_rows.push_back(ResourcePrefetchPredictorTables::UrlTableRow(
      "http://www.google.com",
      "http://www.google.com/image.png",
      ResourceType::IMAGE,
      6, 4, 1, 4.2));
  new_rows.push_back(ResourcePrefetchPredictorTables::UrlTableRow(
      "http://www.google.com",
      "http://www.google.com/a.xml",
      ResourceType::LAST_TYPE,
      1, 0, 0, 6.1));
  new_rows.push_back(ResourcePrefetchPredictorTables::UrlTableRow(
      "http://www.google.com",
      "http://www.resources.google.com/script.js",
      ResourceType::SCRIPT,
      12, 0, 0, 8.5));

  tables_->UpdateRowsForUrl(GURL("http://www.google.com"), new_rows);

  ResourcePrefetchPredictorTables::UrlTableRows actual_rows;
  tables_->GetAllRows(&actual_rows);

  ResourcePrefetchPredictorTables::UrlTableRows expected_rows(new_rows);
  expected_rows.push_back(test_url_rows_[5]);
  expected_rows.push_back(test_url_rows_[6]);
  expected_rows.push_back(test_url_rows_[7]);

  TestUrlRowVectorsAreEqual(expected_rows, actual_rows);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteAllRows() {
  tables_->DeleteAllRows();

  ResourcePrefetchPredictorTables::UrlTableRows actual_rows;
  tables_->GetAllRows(&actual_rows);
  EXPECT_EQ(0, static_cast<int>(actual_rows.size()));
}

void ResourcePrefetchPredictorTablesTest::TestUrlRowVectorsAreEqual(
    const ResourcePrefetchPredictorTables::UrlTableRows& lhs,
    const ResourcePrefetchPredictorTables::UrlTableRows& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  std::set<std::pair<GURL, GURL> > resources_seen;
  for (int i = 0; i < static_cast<int>(rhs.size()); ++i) {
    std::pair<GURL, GURL> resource_id = std::make_pair(rhs[i].main_frame_url,
                                                       rhs[i].resource_url);
    EXPECT_TRUE(resources_seen.find(resource_id) == resources_seen.end());

    for (int j = 0; j < static_cast<int>(lhs.size()); ++j) {
      if (rhs[i] == lhs[j]) {
        resources_seen.insert(resource_id);
        break;
      }
    }
    EXPECT_TRUE(resources_seen.find(resource_id) != resources_seen.end());
  }
  EXPECT_EQ(lhs.size(), resources_seen.size());
}

void ResourcePrefetchPredictorTablesTest::InitializeSampleUrlData() {
  test_url_rows_.clear();
  test_url_rows_.push_back(ResourcePrefetchPredictorTables::UrlTableRow(
      "http://www.google.com",
      "http://www.google.com/style.css",
      ResourceType::STYLESHEET,
      5, 2, 1, 1.1));
  test_url_rows_.push_back(ResourcePrefetchPredictorTables::UrlTableRow(
      "http://www.google.com",
      "http://www.google.com/script.js",
      ResourceType::SCRIPT,
      4, 0, 1, 2.1));
  test_url_rows_.push_back(ResourcePrefetchPredictorTables::UrlTableRow(
      "http://www.google.com",
      "http://www.google.com/image.png",
      ResourceType::IMAGE,
      6, 3, 0, 2.2));
  test_url_rows_.push_back(ResourcePrefetchPredictorTables::UrlTableRow(
      "http://www.google.com",
      "http://www.google.com/a.font",
      ResourceType::LAST_TYPE,
      2, 0, 0, 5.1));
  test_url_rows_.push_back(ResourcePrefetchPredictorTables::UrlTableRow(
      "http://www.google.com",
      "http://www.resources.google.com/script.js",
      ResourceType::SCRIPT,
      11, 0, 0, 8.5));

  test_url_rows_.push_back(ResourcePrefetchPredictorTables::UrlTableRow(
      "http://www.reddit.com",
      "http://reddit-resource.com/script1.js",
      ResourceType::SCRIPT,
      4, 0, 1, 1.0));
  test_url_rows_.push_back(ResourcePrefetchPredictorTables::UrlTableRow(
      "http://www.reddit.com",
      "http://reddit-resource.com/script2.js",
      ResourceType::SCRIPT,
      2, 0, 0, 2.1));

  test_url_rows_.push_back(ResourcePrefetchPredictorTables::UrlTableRow(
      "http://www.yahoo.com",
      "http://www.google.com/image.png",
      ResourceType::IMAGE,
      20, 1, 0, 10.0));

  // Add all the rows to the table.
  tables_->UpdateRowsForUrl(
      GURL("http://www.google.com"),
      ResourcePrefetchPredictorTables::UrlTableRows(
          test_url_rows_.begin(), test_url_rows_.begin() + 5));
  tables_->UpdateRowsForUrl(
      GURL("http://www.reddit.com"),
      ResourcePrefetchPredictorTables::UrlTableRows(
          test_url_rows_.begin() + 5, test_url_rows_.begin() + 7));
  tables_->UpdateRowsForUrl(
      GURL("http://www.yahoo.com"),
      ResourcePrefetchPredictorTables::UrlTableRows(
          test_url_rows_.begin() + 7, test_url_rows_.begin() + 8));
}

TEST_F(ResourcePrefetchPredictorTablesTest, GetAllRows) {
  TestGetAllRows();
}

TEST_F(ResourcePrefetchPredictorTablesTest, UpdateRowsForUrl) {
  TestUpdateRowsForUrl();
}

TEST_F(ResourcePrefetchPredictorTablesTest, DeleteRowsForUrls) {
  TestDeleteRowsForUrls();
}

TEST_F(ResourcePrefetchPredictorTablesTest, DeleteAllRows) {
  TestDeleteAllRows();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, GetAllRows) {
  TestGetAllRows();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, UpdateRowsForUrl) {
  TestUpdateRowsForUrl();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, DeleteRowsForUrls) {
  TestDeleteRowsForUrls();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, DeleteAllRows) {
  TestDeleteAllRows();
}

}  // namespace predictors
