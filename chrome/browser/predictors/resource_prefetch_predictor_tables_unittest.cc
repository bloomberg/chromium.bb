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
  void TestGetAllUrlData();
  void TestUpdateDataForUrl();
  void TestDeleteDataForUrls();
  void TestDeleteAllUrlData();

  MessageLoop loop_;
  content::TestBrowserThread db_thread_;
  TestingProfile profile_;
  scoped_ptr<PredictorDatabase> db_;
  scoped_refptr<ResourcePrefetchPredictorTables> tables_;

 private:
  typedef ResourcePrefetchPredictorTables::UrlResourceRow UrlResourceRow;
  typedef std::vector<UrlResourceRow> UrlResourceRows;
  typedef ResourcePrefetchPredictorTables::UrlData UrlData;
  typedef std::vector<UrlData> UrlDataVector;

  // Adds data to the tables as well as test_url_data_.
  void InitializeSampleUrlData();

  // Checks that the input UrlData are the same, although the resources can be
  // in different order.
  void TestUrlDataAreEqual(const UrlDataVector& lhs,
                           const UrlDataVector& rhs) const;
  void TestUrlResourceRowsAreEqual(const UrlResourceRows& lhs,
                                   const UrlResourceRows& rhs) const;

  UrlDataVector test_url_data_;
};

class ResourcePrefetchPredictorTablesReopenTest
    : public ResourcePrefetchPredictorTablesTest {
 public:
  virtual void SetUp() OVERRIDE {
    // Write data to the table, and then reopen the db.
    ResourcePrefetchPredictorTablesTest::SetUp();
    tables_ = NULL;
    db_.reset(new PredictorDatabase(&profile_));
    loop_.RunUntilIdle();
    tables_ = db_->resource_prefetch_tables();
  }
};

ResourcePrefetchPredictorTablesTest::ResourcePrefetchPredictorTablesTest()
    : loop_(MessageLoop::TYPE_DEFAULT),
      db_thread_(content::BrowserThread::DB, &loop_),
      db_(new PredictorDatabase(&profile_)),
      tables_(db_->resource_prefetch_tables()) {
  loop_.RunUntilIdle();
}

ResourcePrefetchPredictorTablesTest::~ResourcePrefetchPredictorTablesTest() {
}

void ResourcePrefetchPredictorTablesTest::SetUp() {
  tables_->DeleteAllUrlData();
  InitializeSampleUrlData();
}

void ResourcePrefetchPredictorTablesTest::TestGetAllUrlData() {
  UrlDataVector actual_data;
  tables_->GetAllUrlData(&actual_data);

  TestUrlDataAreEqual(test_url_data_, actual_data);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteDataForUrls() {
  std::vector<GURL> urls_to_delete;
  urls_to_delete.push_back(GURL("http://www.google.com"));
  urls_to_delete.push_back(GURL("http://www.yahoo.com"));

  tables_->DeleteDataForUrls(urls_to_delete);

  UrlDataVector actual_data;
  tables_->GetAllUrlData(&actual_data);

  UrlDataVector expected_data;
  expected_data.push_back(test_url_data_[1]);

  TestUrlDataAreEqual(expected_data, actual_data);
}

void ResourcePrefetchPredictorTablesTest::TestUpdateDataForUrl() {
  UrlData new_data(GURL("http://www.google.com"));
  new_data.last_visit = base::Time::FromInternalValue(10);
  new_data.resources.push_back(UrlResourceRow(
      "http://www.google.com",
      "http://www.google.com/style.css",
      ResourceType::STYLESHEET,
      6, 2, 0, 1.0));
  new_data.resources.push_back(UrlResourceRow(
      "http://www.google.com",
      "http://www.google.com/image.png",
      ResourceType::IMAGE,
      6, 4, 1, 4.2));
  new_data.resources.push_back(UrlResourceRow(
      "http://www.google.com",
      "http://www.google.com/a.xml",
      ResourceType::LAST_TYPE,
      1, 0, 0, 6.1));
  new_data.resources.push_back(UrlResourceRow(
      "http://www.google.com",
      "http://www.resources.google.com/script.js",
      ResourceType::SCRIPT,
      12, 0, 0, 8.5));

  tables_->UpdateDataForUrl(new_data);

  UrlDataVector actual_data;
  tables_->GetAllUrlData(&actual_data);

  UrlDataVector expected_data;
  expected_data.push_back(new_data);
  expected_data.push_back(test_url_data_[1]);
  expected_data.push_back(test_url_data_[2]);

  TestUrlDataAreEqual(expected_data, actual_data);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteAllUrlData() {
  tables_->DeleteAllUrlData();

  UrlDataVector actual_data;
  tables_->GetAllUrlData(&actual_data);
  EXPECT_EQ(0, static_cast<int>(actual_data.size()));
}

void ResourcePrefetchPredictorTablesTest::TestUrlDataAreEqual(
    const UrlDataVector& lhs,
    const UrlDataVector& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  std::set<GURL> urls_seen;
  for (UrlDataVector::const_iterator rhs_it = rhs.begin(); rhs_it != rhs.end();
       ++rhs_it) {
    for (UrlDataVector::const_iterator lhs_it = lhs.begin();
         lhs_it != lhs.end(); ++lhs_it) {
      if (lhs_it->main_frame_url == rhs_it->main_frame_url) {
        EXPECT_TRUE(urls_seen.find(rhs_it->main_frame_url) == urls_seen.end());
        urls_seen.insert(rhs_it->main_frame_url);
        EXPECT_TRUE(lhs_it->last_visit == rhs_it->last_visit);
        TestUrlResourceRowsAreEqual(lhs_it->resources, rhs_it->resources);
        break;
      }
    }
  }

  EXPECT_EQ(lhs.size(), urls_seen.size());
}

void ResourcePrefetchPredictorTablesTest::TestUrlResourceRowsAreEqual(
    const UrlResourceRows& lhs,
    const UrlResourceRows& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  std::set<std::pair<GURL, GURL> > resources_seen;
  for (UrlResourceRows::const_iterator rhs_it = rhs.begin();
       rhs_it != rhs.end(); ++rhs_it) {
    std::pair<GURL, GURL> resource_id = std::make_pair(rhs_it->main_frame_url,
                                                       rhs_it->resource_url);
    EXPECT_TRUE(resources_seen.find(resource_id) == resources_seen.end());

    for (UrlResourceRows::const_iterator lhs_it = lhs.begin();
         lhs_it != lhs.end(); ++lhs_it) {
      if (*rhs_it == *lhs_it) {
        resources_seen.insert(resource_id);
        break;
      }
    }
    EXPECT_TRUE(resources_seen.find(resource_id) != resources_seen.end());
  }
  EXPECT_EQ(lhs.size(), resources_seen.size());
}

void ResourcePrefetchPredictorTablesTest::InitializeSampleUrlData() {
  UrlData google(GURL("http://www.google.com"));
  google.last_visit = base::Time::FromInternalValue(1);
  google.resources.push_back(UrlResourceRow(
      "http://www.google.com",
      "http://www.google.com/style.css",
      ResourceType::STYLESHEET,
      5, 2, 1, 1.1));
  google.resources.push_back(UrlResourceRow(
      "http://www.google.com",
      "http://www.google.com/script.js",
      ResourceType::SCRIPT,
      4, 0, 1, 2.1));
  google.resources.push_back(UrlResourceRow(
      "http://www.google.com",
      "http://www.google.com/image.png",
      ResourceType::IMAGE,
      6, 3, 0, 2.2));
  google.resources.push_back(UrlResourceRow(
      "http://www.google.com",
      "http://www.google.com/a.font",
      ResourceType::LAST_TYPE,
      2, 0, 0, 5.1));
  google.resources.push_back(UrlResourceRow(
      "http://www.google.com",
      "http://www.resources.google.com/script.js",
      ResourceType::SCRIPT,
      11, 0, 0, 8.5));

  UrlData reddit(GURL("http://www.reddit.com"));
  reddit.last_visit = base::Time::FromInternalValue(2);
  reddit.resources.push_back(UrlResourceRow(
      "http://www.reddit.com",
      "http://reddit-resource.com/script1.js",
      ResourceType::SCRIPT,
      4, 0, 1, 1.0));
  reddit.resources.push_back(UrlResourceRow(
      "http://www.reddit.com",
      "http://reddit-resource.com/script2.js",
      ResourceType::SCRIPT,
      2, 0, 0, 2.1));

  UrlData yahoo(GURL("http://www.yahoo.com"));
  yahoo.last_visit = base::Time::FromInternalValue(3);
  yahoo.resources.push_back(UrlResourceRow(
      "http://www.yahoo.com",
      "http://www.google.com/image.png",
      ResourceType::IMAGE,
      20, 1, 0, 10.0));

  test_url_data_.clear();
  test_url_data_.push_back(google);
  test_url_data_.push_back(reddit);
  test_url_data_.push_back(yahoo);

  // Add all the rows to the table.
  tables_->UpdateDataForUrl(google);
  tables_->UpdateDataForUrl(reddit);
  tables_->UpdateDataForUrl(yahoo);
}

TEST_F(ResourcePrefetchPredictorTablesTest, GetAllUrlData) {
  TestGetAllUrlData();
}

TEST_F(ResourcePrefetchPredictorTablesTest, UpdateDataForUrl) {
  TestUpdateDataForUrl();
}

TEST_F(ResourcePrefetchPredictorTablesTest, DeleteDataForUrls) {
  TestDeleteDataForUrls();
}

TEST_F(ResourcePrefetchPredictorTablesTest, DeleteAllUrlData) {
  TestDeleteAllUrlData();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, GetAllUrlData) {
  TestGetAllUrlData();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, UpdateDataForUrl) {
  TestUpdateDataForUrl();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, DeleteDataForUrls) {
  TestDeleteDataForUrls();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, DeleteAllUrlData) {
  TestDeleteAllUrlData();
}

}  // namespace predictors
