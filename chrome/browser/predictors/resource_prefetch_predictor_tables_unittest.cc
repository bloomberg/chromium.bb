// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
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
  ~ResourcePrefetchPredictorTablesTest() override;
  void SetUp() override;
  void TearDown() override;

 protected:
  void TestGetAllData();
  void TestUpdateData();
  void TestDeleteData();
  void TestDeleteSingleDataPoint();
  void TestDeleteAllData();

  base::MessageLoop loop_;
  content::TestBrowserThread db_thread_;
  TestingProfile profile_;
  std::unique_ptr<PredictorDatabase> db_;
  scoped_refptr<ResourcePrefetchPredictorTables> tables_;

 private:
  typedef ResourcePrefetchPredictorTables::ResourceRow ResourceRow;
  typedef std::vector<ResourceRow> ResourceRows;
  typedef ResourcePrefetchPredictorTables::PrefetchData PrefetchData;
  typedef ResourcePrefetchPredictorTables::PrefetchDataMap PrefetchDataMap;

  // Initializes the tables, |test_url_data_| and |test_host_data_|.
  void InitializeSampleData();

  // Checks that the input PrefetchData are the same, although the resources
  // can be in different order.
  void TestPrefetchDataAreEqual(const PrefetchDataMap& lhs,
                                const PrefetchDataMap& rhs) const;
  void TestResourceRowsAreEqual(const ResourceRows& lhs,
                                const ResourceRows& rhs) const;

  void AddKey(PrefetchDataMap* m, const std::string& key) const;

  // Useful for debugging test.
  void PrintPrefetchData(const PrefetchData& data) const {
    LOG(ERROR) << "[" << data.key_type << "," << data.primary_key
               << "," << data.last_visit.ToInternalValue() << "]";
    for (ResourceRows::const_iterator it = data.resources.begin();
         it != data.resources.end(); ++it) {
      LOG(ERROR) << "\t\t" << it->resource_url << "\t" << it->resource_type
                 << "\t" << it->number_of_hits << "\t" << it->number_of_misses
                 << "\t" << it->consecutive_misses
                 << "\t" << it->average_position
                 << "\t" << it->score;
    }
  }

  PrefetchDataMap test_url_data_;
  PrefetchDataMap test_host_data_;
};

class ResourcePrefetchPredictorTablesReopenTest
    : public ResourcePrefetchPredictorTablesTest {
 public:
  void SetUp() override {
    // Write data to the table, and then reopen the db.
    ResourcePrefetchPredictorTablesTest::SetUp();
    ResourcePrefetchPredictorTablesTest::TearDown();

    db_.reset(new PredictorDatabase(&profile_));
    loop_.RunUntilIdle();
    tables_ = db_->resource_prefetch_tables();
  }
};

ResourcePrefetchPredictorTablesTest::ResourcePrefetchPredictorTablesTest()
    : loop_(base::MessageLoop::TYPE_DEFAULT),
      db_thread_(content::BrowserThread::DB, &loop_),
      db_(new PredictorDatabase(&profile_)),
      tables_(db_->resource_prefetch_tables()) {
  loop_.RunUntilIdle();
}

ResourcePrefetchPredictorTablesTest::~ResourcePrefetchPredictorTablesTest() {
}

void ResourcePrefetchPredictorTablesTest::SetUp() {
  tables_->DeleteAllData();
  InitializeSampleData();
}

void ResourcePrefetchPredictorTablesTest::TearDown() {
  tables_ = NULL;
  db_.reset();
  loop_.RunUntilIdle();
}

void ResourcePrefetchPredictorTablesTest::TestGetAllData() {
  PrefetchDataMap actual_url_data, actual_host_data;
  tables_->GetAllData(&actual_url_data, &actual_host_data);

  TestPrefetchDataAreEqual(test_url_data_, actual_url_data);
  TestPrefetchDataAreEqual(test_host_data_, actual_host_data);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteData() {
  std::vector<std::string> urls_to_delete, hosts_to_delete;
  urls_to_delete.push_back("http://www.google.com");
  urls_to_delete.push_back("http://www.yahoo.com");
  hosts_to_delete.push_back("www.yahoo.com");

  tables_->DeleteData(urls_to_delete, hosts_to_delete);

  PrefetchDataMap actual_url_data, actual_host_data;
  tables_->GetAllData(&actual_url_data, &actual_host_data);

  PrefetchDataMap expected_url_data, expected_host_data;
  AddKey(&expected_url_data, "http://www.reddit.com");
  AddKey(&expected_host_data, "www.facebook.com");

  TestPrefetchDataAreEqual(expected_url_data, actual_url_data);
  TestPrefetchDataAreEqual(expected_host_data, actual_host_data);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteSingleDataPoint() {
  // Delete a URL.
  tables_->DeleteSingleDataPoint("http://www.reddit.com",
                                 PREFETCH_KEY_TYPE_URL);

  PrefetchDataMap actual_url_data, actual_host_data;
  tables_->GetAllData(&actual_url_data, &actual_host_data);

  PrefetchDataMap expected_url_data;
  AddKey(&expected_url_data, "http://www.google.com");
  AddKey(&expected_url_data, "http://www.yahoo.com");

  TestPrefetchDataAreEqual(expected_url_data, actual_url_data);
  TestPrefetchDataAreEqual(test_host_data_, actual_host_data);

  // Delete a host.
  tables_->DeleteSingleDataPoint("www.facebook.com", PREFETCH_KEY_TYPE_HOST);
  actual_url_data.clear();
  actual_host_data.clear();
  tables_->GetAllData(&actual_url_data, &actual_host_data);

  PrefetchDataMap expected_host_data;
  AddKey(&expected_host_data, "www.yahoo.com");

  TestPrefetchDataAreEqual(expected_url_data, actual_url_data);
  TestPrefetchDataAreEqual(expected_host_data, actual_host_data);
}

void ResourcePrefetchPredictorTablesTest::TestUpdateData() {
  PrefetchData google(PREFETCH_KEY_TYPE_URL, "http://www.google.com");
  google.last_visit = base::Time::FromInternalValue(10);
  google.resources.push_back(ResourceRow(std::string(),
                                         "http://www.google.com/style.css",
                                         content::RESOURCE_TYPE_STYLESHEET,
                                         6,
                                         2,
                                         0,
                                         1.0));
  google.resources.push_back(ResourceRow(std::string(),
                                         "http://www.google.com/image.png",
                                         content::RESOURCE_TYPE_IMAGE,
                                         6,
                                         4,
                                         1,
                                         4.2));
  google.resources.push_back(ResourceRow(std::string(),
                                         "http://www.google.com/a.xml",
                                         content::RESOURCE_TYPE_LAST_TYPE,
                                         1,
                                         0,
                                         0,
                                         6.1));
  google.resources
      .push_back(ResourceRow(std::string(),
                             "http://www.resources.google.com/script.js",
                             content::RESOURCE_TYPE_SCRIPT,
                             12,
                             0,
                             0,
                             8.5));

  PrefetchData yahoo(PREFETCH_KEY_TYPE_HOST, "www.yahoo.com");
  yahoo.last_visit = base::Time::FromInternalValue(7);
  yahoo.resources.push_back(ResourceRow(std::string(),
                                        "http://www.yahoo.com/image.png",
                                        content::RESOURCE_TYPE_IMAGE,
                                        120,
                                        1,
                                        1,
                                        10.0));

  tables_->UpdateData(google, yahoo);

  PrefetchDataMap actual_url_data, actual_host_data;
  tables_->GetAllData(&actual_url_data, &actual_host_data);

  PrefetchDataMap expected_url_data, expected_host_data;
  AddKey(&expected_url_data, "http://www.reddit.com");
  AddKey(&expected_url_data, "http://www.yahoo.com");
  expected_url_data.insert(std::make_pair("http://www.google.com", google));

  AddKey(&expected_host_data, "www.facebook.com");
  expected_host_data.insert(std::make_pair("www.yahoo.com", yahoo));

  TestPrefetchDataAreEqual(expected_url_data, actual_url_data);
  TestPrefetchDataAreEqual(expected_host_data, actual_host_data);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteAllData() {
  tables_->DeleteAllData();

  PrefetchDataMap actual_url_data, actual_host_data;
  tables_->GetAllData(&actual_url_data, &actual_host_data);
  EXPECT_TRUE(actual_url_data.empty());
  EXPECT_TRUE(actual_host_data.empty());
}

void ResourcePrefetchPredictorTablesTest::TestPrefetchDataAreEqual(
    const PrefetchDataMap& lhs,
    const PrefetchDataMap& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  for (PrefetchDataMap::const_iterator rhs_it = rhs.begin();
       rhs_it != rhs.end(); ++rhs_it) {
    PrefetchDataMap::const_iterator lhs_it = lhs.find(rhs_it->first);
    ASSERT_TRUE(lhs_it != lhs.end()) << rhs_it->first;

    TestResourceRowsAreEqual(lhs_it->second.resources,
                             rhs_it->second.resources);
  }
}

void ResourcePrefetchPredictorTablesTest::TestResourceRowsAreEqual(
    const ResourceRows& lhs,
    const ResourceRows& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  std::set<GURL> resources_seen;
  for (ResourceRows::const_iterator rhs_it = rhs.begin();
       rhs_it != rhs.end(); ++rhs_it) {
    const GURL& resource = rhs_it->resource_url;
    EXPECT_FALSE(ContainsKey(resources_seen, resource));

    for (ResourceRows::const_iterator lhs_it = lhs.begin();
         lhs_it != lhs.end(); ++lhs_it) {
      if (*rhs_it == *lhs_it) {
        resources_seen.insert(resource);
        break;
      }
    }
    EXPECT_TRUE(ContainsKey(resources_seen, resource));
  }
  EXPECT_EQ(lhs.size(), resources_seen.size());
}

void ResourcePrefetchPredictorTablesTest::AddKey(PrefetchDataMap* m,
                                                 const std::string& key) const {
  PrefetchDataMap::const_iterator it = test_url_data_.find(key);
  if (it != test_url_data_.end()) {
    m->insert(std::make_pair(it->first, it->second));
    return;
  }
  it = test_host_data_.find(key);
  ASSERT_TRUE(it != test_host_data_.end());
  m->insert(std::make_pair(it->first, it->second));
}

void ResourcePrefetchPredictorTablesTest::InitializeSampleData() {
  {  // Url data.
    PrefetchData google(PREFETCH_KEY_TYPE_URL, "http://www.google.com");
    google.last_visit = base::Time::FromInternalValue(1);
    google.resources.push_back(ResourceRow(std::string(),
                                           "http://www.google.com/style.css",
                                           content::RESOURCE_TYPE_STYLESHEET,
                                           5,
                                           2,
                                           1,
                                           1.1));
    google.resources.push_back(ResourceRow(std::string(),
                                           "http://www.google.com/script.js",
                                           content::RESOURCE_TYPE_SCRIPT,
                                           4,
                                           0,
                                           1,
                                           2.1));
    google.resources.push_back(ResourceRow(std::string(),
                                           "http://www.google.com/image.png",
                                           content::RESOURCE_TYPE_IMAGE,
                                           6,
                                           3,
                                           0,
                                           2.2));
    google.resources.push_back(ResourceRow(std::string(),
                                           "http://www.google.com/a.font",
                                           content::RESOURCE_TYPE_LAST_TYPE,
                                           2,
                                           0,
                                           0,
                                           5.1));
    google.resources
        .push_back(ResourceRow(std::string(),
                               "http://www.resources.google.com/script.js",
                               content::RESOURCE_TYPE_SCRIPT,
                               11,
                               0,
                               0,
                               8.5));

    PrefetchData reddit(PREFETCH_KEY_TYPE_URL, "http://www.reddit.com");
    reddit.last_visit = base::Time::FromInternalValue(2);
    reddit.resources
        .push_back(ResourceRow(std::string(),
                               "http://reddit-resource.com/script1.js",
                               content::RESOURCE_TYPE_SCRIPT,
                               4,
                               0,
                               1,
                               1.0));
    reddit.resources
        .push_back(ResourceRow(std::string(),
                               "http://reddit-resource.com/script2.js",
                               content::RESOURCE_TYPE_SCRIPT,
                               2,
                               0,
                               0,
                               2.1));

    PrefetchData yahoo(PREFETCH_KEY_TYPE_URL, "http://www.yahoo.com");
    yahoo.last_visit = base::Time::FromInternalValue(3);
    yahoo.resources.push_back(ResourceRow(std::string(),
                                          "http://www.google.com/image.png",
                                          content::RESOURCE_TYPE_IMAGE,
                                          20,
                                          1,
                                          0,
                                          10.0));

    test_url_data_.clear();
    test_url_data_.insert(std::make_pair("http://www.google.com", google));
    test_url_data_.insert(std::make_pair("http://www.reddit.com", reddit));
    test_url_data_.insert(std::make_pair("http://www.yahoo.com", yahoo));

    PrefetchData empty_host_data(PREFETCH_KEY_TYPE_HOST, std::string());
    tables_->UpdateData(google, empty_host_data);
    tables_->UpdateData(reddit, empty_host_data);
    tables_->UpdateData(yahoo, empty_host_data);
  }

  {  // Host data.
    PrefetchData facebook(PREFETCH_KEY_TYPE_HOST, "www.facebook.com");
    facebook.last_visit = base::Time::FromInternalValue(4);
    facebook.resources
        .push_back(ResourceRow(std::string(),
                               "http://www.facebook.com/style.css",
                               content::RESOURCE_TYPE_STYLESHEET,
                               5,
                               2,
                               1,
                               1.1));
    facebook.resources
        .push_back(ResourceRow(std::string(),
                               "http://www.facebook.com/script.js",
                               content::RESOURCE_TYPE_SCRIPT,
                               4,
                               0,
                               1,
                               2.1));
    facebook.resources
        .push_back(ResourceRow(std::string(),
                               "http://www.facebook.com/image.png",
                               content::RESOURCE_TYPE_IMAGE,
                               6,
                               3,
                               0,
                               2.2));
    facebook.resources.push_back(ResourceRow(std::string(),
                                             "http://www.facebook.com/a.font",
                                             content::RESOURCE_TYPE_LAST_TYPE,
                                             2,
                                             0,
                                             0,
                                             5.1));
    facebook.resources
        .push_back(ResourceRow(std::string(),
                               "http://www.resources.facebook.com/script.js",
                               content::RESOURCE_TYPE_SCRIPT,
                               11,
                               0,
                               0,
                               8.5));

    PrefetchData yahoo(PREFETCH_KEY_TYPE_HOST, "www.yahoo.com");
    yahoo.last_visit = base::Time::FromInternalValue(5);
    yahoo.resources.push_back(ResourceRow(std::string(),
                                          "http://www.google.com/image.png",
                                          content::RESOURCE_TYPE_IMAGE,
                                          20,
                                          1,
                                          0,
                                          10.0));

    test_host_data_.clear();
    test_host_data_.insert(std::make_pair("www.facebook.com", facebook));
    test_host_data_.insert(std::make_pair("www.yahoo.com", yahoo));

    PrefetchData empty_url_data(PREFETCH_KEY_TYPE_URL, std::string());
    tables_->UpdateData(empty_url_data, facebook);
    tables_->UpdateData(empty_url_data, yahoo);
  }
}

// Test cases.

TEST_F(ResourcePrefetchPredictorTablesTest, GetAllData) {
  TestGetAllData();
}

TEST_F(ResourcePrefetchPredictorTablesTest, UpdateData) {
  TestUpdateData();
}

TEST_F(ResourcePrefetchPredictorTablesTest, DeleteData) {
  TestDeleteData();
}

TEST_F(ResourcePrefetchPredictorTablesTest, DeleteSingleDataPoint) {
  TestDeleteSingleDataPoint();
}

TEST_F(ResourcePrefetchPredictorTablesTest, DeleteAllData) {
  TestDeleteAllData();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, GetAllData) {
  TestGetAllData();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, UpdateData) {
  TestUpdateData();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, DeleteData) {
  TestDeleteData();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, DeleteSingleDataPoint) {
  TestDeleteSingleDataPoint();
}

TEST_F(ResourcePrefetchPredictorTablesReopenTest, DeleteAllData) {
  TestDeleteAllData();
}

}  // namespace predictors
