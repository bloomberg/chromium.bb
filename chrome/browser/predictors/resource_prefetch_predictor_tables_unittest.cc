// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/request_priority.h"
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
  void ReopenDatabase();
  void TestGetAllData();
  void TestUpdateData();
  void TestDeleteData();
  void TestDeleteSingleDataPoint();
  void TestDeleteAllData();

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<PredictorDatabase> db_;
  scoped_refptr<ResourcePrefetchPredictorTables> tables_;

  using PrefetchDataMap = ResourcePrefetchPredictorTables::PrefetchDataMap;
  using RedirectDataMap = ResourcePrefetchPredictorTables::RedirectDataMap;

 private:
  // Initializes the tables, |test_url_data_| and |test_host_data_|.
  void InitializeSampleData();

  // Checks that the input PrefetchData are the same, although the resources
  // can be in different order.
  void TestPrefetchDataAreEqual(const PrefetchDataMap& lhs,
                                const PrefetchDataMap& rhs) const;
  void TestResourcesAreEqual(const std::vector<ResourceData>& lhs,
                             const std::vector<ResourceData>& rhs) const;

  // Checks that the input RedirectData are the same, although the redirects
  // can be in different order.
  void TestRedirectDataAreEqual(const RedirectDataMap& lhs,
                                const RedirectDataMap& rhs) const;
  void TestRedirectsAreEqual(const std::vector<RedirectStat>& lhs,
                             const std::vector<RedirectStat>& rhs) const;

  void AddKey(PrefetchDataMap* m, const std::string& key) const;
  void AddKey(RedirectDataMap* m, const std::string& key) const;

  PrefetchDataMap test_url_data_;
  PrefetchDataMap test_host_data_;
  RedirectDataMap test_url_redirect_data_;
  RedirectDataMap test_host_redirect_data_;
};

class ResourcePrefetchPredictorTablesReopenTest
    : public ResourcePrefetchPredictorTablesTest {
 public:
  void SetUp() override {
    // Write data to the table, and then reopen the db.
    ResourcePrefetchPredictorTablesTest::SetUp();
    ResourcePrefetchPredictorTablesTest::TearDown();

    ReopenDatabase();
  }
};

ResourcePrefetchPredictorTablesTest::ResourcePrefetchPredictorTablesTest()
    : db_(new PredictorDatabase(&profile_)),
      tables_(db_->resource_prefetch_tables()) {
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
}

void ResourcePrefetchPredictorTablesTest::TestGetAllData() {
  PrefetchDataMap actual_url_data, actual_host_data;
  RedirectDataMap actual_url_redirect_data, actual_host_redirect_data;
  tables_->GetAllData(&actual_url_data, &actual_host_data,
                      &actual_url_redirect_data, &actual_host_redirect_data);

  TestPrefetchDataAreEqual(test_url_data_, actual_url_data);
  TestPrefetchDataAreEqual(test_host_data_, actual_host_data);
  TestRedirectDataAreEqual(test_url_redirect_data_, actual_url_redirect_data);
  TestRedirectDataAreEqual(test_host_redirect_data_, actual_host_redirect_data);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteData() {
  std::vector<std::string> urls_to_delete = {"http://www.google.com",
                                             "http://www.yahoo.com"};
  std::vector<std::string> hosts_to_delete = {"www.yahoo.com"};

  tables_->DeleteResourceData(urls_to_delete, hosts_to_delete);

  urls_to_delete = {"http://fb.com/google", "http://google.com"};
  hosts_to_delete = {"microsoft.com"};

  tables_->DeleteRedirectData(urls_to_delete, hosts_to_delete);

  PrefetchDataMap actual_url_data, actual_host_data;
  RedirectDataMap actual_url_redirect_data, actual_host_redirect_data;
  tables_->GetAllData(&actual_url_data, &actual_host_data,
                      &actual_url_redirect_data, &actual_host_redirect_data);

  PrefetchDataMap expected_url_data, expected_host_data;
  RedirectDataMap expected_url_redirect_data, expected_host_redirect_data;
  AddKey(&expected_url_data, "http://www.reddit.com");
  AddKey(&expected_host_data, "www.facebook.com");
  AddKey(&expected_url_redirect_data, "http://nyt.com");
  AddKey(&expected_host_redirect_data, "bbc.com");

  TestPrefetchDataAreEqual(expected_url_data, actual_url_data);
  TestPrefetchDataAreEqual(expected_host_data, actual_host_data);
  TestRedirectDataAreEqual(expected_url_redirect_data,
                           actual_url_redirect_data);
  TestRedirectDataAreEqual(expected_host_redirect_data,
                           actual_host_redirect_data);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteSingleDataPoint() {
  // Delete a URL.
  tables_->DeleteSingleResourceDataPoint("http://www.reddit.com",
                                         PREFETCH_KEY_TYPE_URL);

  PrefetchDataMap actual_url_data, actual_host_data;
  RedirectDataMap actual_url_redirect_data, actual_host_redirect_data;
  tables_->GetAllData(&actual_url_data, &actual_host_data,
                      &actual_url_redirect_data, &actual_host_redirect_data);

  PrefetchDataMap expected_url_data;
  AddKey(&expected_url_data, "http://www.google.com");
  AddKey(&expected_url_data, "http://www.yahoo.com");

  TestPrefetchDataAreEqual(expected_url_data, actual_url_data);
  TestPrefetchDataAreEqual(test_host_data_, actual_host_data);
  TestRedirectDataAreEqual(test_url_redirect_data_, actual_url_redirect_data);
  TestRedirectDataAreEqual(test_host_redirect_data_, actual_host_redirect_data);

  // Delete a host.
  tables_->DeleteSingleResourceDataPoint("www.facebook.com",
                                         PREFETCH_KEY_TYPE_HOST);
  actual_url_data.clear();
  actual_host_data.clear();
  actual_url_redirect_data.clear();
  actual_host_redirect_data.clear();
  tables_->GetAllData(&actual_url_data, &actual_host_data,
                      &actual_url_redirect_data, &actual_host_redirect_data);

  PrefetchDataMap expected_host_data;
  AddKey(&expected_host_data, "www.yahoo.com");

  TestPrefetchDataAreEqual(expected_url_data, actual_url_data);
  TestPrefetchDataAreEqual(expected_host_data, actual_host_data);
  TestRedirectDataAreEqual(test_url_redirect_data_, actual_url_redirect_data);
  TestRedirectDataAreEqual(test_host_redirect_data_, actual_host_redirect_data);

  // Delete a URL redirect.
  tables_->DeleteSingleRedirectDataPoint("http://nyt.com",
                                         PREFETCH_KEY_TYPE_URL);
  actual_url_data.clear();
  actual_host_data.clear();
  actual_url_redirect_data.clear();
  actual_host_redirect_data.clear();
  tables_->GetAllData(&actual_url_data, &actual_host_data,
                      &actual_url_redirect_data, &actual_host_redirect_data);

  RedirectDataMap expected_url_redirect_data;
  AddKey(&expected_url_redirect_data, "http://fb.com/google");
  AddKey(&expected_url_redirect_data, "http://google.com");

  TestPrefetchDataAreEqual(expected_url_data, actual_url_data);
  TestPrefetchDataAreEqual(expected_host_data, actual_host_data);
  TestRedirectDataAreEqual(expected_url_redirect_data,
                           actual_url_redirect_data);
  TestRedirectDataAreEqual(test_host_redirect_data_, actual_host_redirect_data);

  // Delete a host redirect.
  tables_->DeleteSingleRedirectDataPoint("bbc.com", PREFETCH_KEY_TYPE_HOST);
  actual_url_data.clear();
  actual_host_data.clear();
  actual_url_redirect_data.clear();
  actual_host_redirect_data.clear();
  tables_->GetAllData(&actual_url_data, &actual_host_data,
                      &actual_url_redirect_data, &actual_host_redirect_data);

  RedirectDataMap expected_host_redirect_data;
  AddKey(&expected_host_redirect_data, "microsoft.com");

  TestPrefetchDataAreEqual(expected_url_data, actual_url_data);
  TestPrefetchDataAreEqual(expected_host_data, actual_host_data);
  TestRedirectDataAreEqual(expected_url_redirect_data,
                           actual_url_redirect_data);
  TestRedirectDataAreEqual(expected_host_redirect_data,
                           actual_host_redirect_data);
}

void ResourcePrefetchPredictorTablesTest::TestUpdateData() {
  PrefetchData google = CreatePrefetchData("http://www.google.com", 10);
  InitializeResourceData(google.add_resources(),
                         "http://www.google.com/style.css",
                         content::RESOURCE_TYPE_STYLESHEET, 6, 2, 0, 1.0,
                         net::MEDIUM, true, false);
  InitializeResourceData(
      google.add_resources(), "http://www.google.com/image.png",
      content::RESOURCE_TYPE_IMAGE, 6, 4, 1, 4.2, net::MEDIUM, false, false);
  InitializeResourceData(google.add_resources(), "http://www.google.com/a.xml",
                         content::RESOURCE_TYPE_LAST_TYPE, 1, 0, 0, 6.1,
                         net::MEDIUM, false, false);
  InitializeResourceData(
      google.add_resources(), "http://www.resources.google.com/script.js",
      content::RESOURCE_TYPE_SCRIPT, 12, 0, 0, 8.5, net::MEDIUM, true, true);

  PrefetchData yahoo = CreatePrefetchData("www.yahoo.com", 7);
  InitializeResourceData(
      yahoo.add_resources(), "http://www.yahoo.com/image.png",
      content::RESOURCE_TYPE_IMAGE, 120, 1, 1, 10.0, net::MEDIUM, true, false);

  RedirectData facebook = CreateRedirectData("http://fb.com/google", 20);
  InitializeRedirectStat(facebook.add_redirect_endpoints(),
                         "https://facebook.fr/google", 4, 2, 1);

  RedirectData microsoft = CreateRedirectData("microsoft.com", 21);
  InitializeRedirectStat(microsoft.add_redirect_endpoints(), "m.microsoft.com",
                         5, 7, 1);
  InitializeRedirectStat(microsoft.add_redirect_endpoints(), "microsoft.org", 7,
                         2, 0);

  tables_->UpdateData(google, yahoo, facebook, microsoft);

  PrefetchDataMap actual_url_data, actual_host_data;
  RedirectDataMap actual_url_redirect_data, actual_host_redirect_data;
  tables_->GetAllData(&actual_url_data, &actual_host_data,
                      &actual_url_redirect_data, &actual_host_redirect_data);

  PrefetchDataMap expected_url_data, expected_host_data;
  RedirectDataMap expected_url_redirect_data, expected_host_redirect_data;
  AddKey(&expected_url_data, "http://www.reddit.com");
  AddKey(&expected_url_data, "http://www.yahoo.com");
  expected_url_data.insert(std::make_pair("http://www.google.com", google));

  AddKey(&expected_host_data, "www.facebook.com");
  expected_host_data.insert(std::make_pair("www.yahoo.com", yahoo));

  AddKey(&expected_url_redirect_data, "http://nyt.com");
  AddKey(&expected_url_redirect_data, "http://google.com");
  expected_url_redirect_data.insert(
      std::make_pair("http://fb.com/google", facebook));

  AddKey(&expected_host_redirect_data, "bbc.com");
  expected_host_redirect_data.insert(
      std::make_pair("microsoft.com", microsoft));

  TestPrefetchDataAreEqual(expected_url_data, actual_url_data);
  TestPrefetchDataAreEqual(expected_host_data, actual_host_data);
  TestRedirectDataAreEqual(expected_url_redirect_data,
                           actual_url_redirect_data);
  TestRedirectDataAreEqual(expected_host_redirect_data,
                           actual_host_redirect_data);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteAllData() {
  tables_->DeleteAllData();

  PrefetchDataMap actual_url_data, actual_host_data;
  RedirectDataMap actual_url_redirect_data, actual_host_redirect_data;
  tables_->GetAllData(&actual_url_data, &actual_host_data,
                      &actual_url_redirect_data, &actual_host_redirect_data);
  EXPECT_TRUE(actual_url_data.empty());
  EXPECT_TRUE(actual_host_data.empty());
  EXPECT_TRUE(actual_url_redirect_data.empty());
  EXPECT_TRUE(actual_host_redirect_data.empty());
}

void ResourcePrefetchPredictorTablesTest::TestPrefetchDataAreEqual(
    const PrefetchDataMap& lhs,
    const PrefetchDataMap& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  for (const auto& p : rhs) {
    const auto lhs_it = lhs.find(p.first);
    ASSERT_TRUE(lhs_it != lhs.end()) << p.first;
    EXPECT_TRUE(lhs_it->second.primary_key() == p.second.primary_key());
    EXPECT_TRUE(lhs_it->second.last_visit_time() == p.second.last_visit_time());

    std::vector<ResourceData> lhs_resources(lhs_it->second.resources().begin(),
                                            lhs_it->second.resources().end());
    std::vector<ResourceData> rhs_resources(p.second.resources().begin(),
                                            p.second.resources().end());

    TestResourcesAreEqual(lhs_resources, rhs_resources);
  }
}

void ResourcePrefetchPredictorTablesTest::TestResourcesAreEqual(
    const std::vector<ResourceData>& lhs,
    const std::vector<ResourceData>& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  std::map<std::string, ResourceData> lhs_index;
  // Repeated resources are not allowed.
  for (const auto& r : lhs)
    EXPECT_TRUE(lhs_index.insert(std::make_pair(r.resource_url(), r)).second);

  for (const auto& r : rhs) {
    auto lhs_it = lhs_index.find(r.resource_url());
    if (lhs_it != lhs_index.end()) {
      EXPECT_EQ(r, lhs_it->second);
      lhs_index.erase(lhs_it);
    } else {
      ADD_FAILURE() << r.resource_url();
    }
  }

  EXPECT_TRUE(lhs_index.empty());
}

void ResourcePrefetchPredictorTablesTest::TestRedirectDataAreEqual(
    const RedirectDataMap& lhs,
    const RedirectDataMap& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  for (const auto& p : rhs) {
    const auto lhs_it = lhs.find(p.first);
    ASSERT_TRUE(lhs_it != lhs.end()) << p.first;
    EXPECT_EQ(lhs_it->second.primary_key(), p.second.primary_key());
    EXPECT_EQ(lhs_it->second.last_visit_time(), p.second.last_visit_time());

    std::vector<RedirectStat> lhs_redirects(
        lhs_it->second.redirect_endpoints().begin(),
        lhs_it->second.redirect_endpoints().end());
    std::vector<RedirectStat> rhs_redirects(
        p.second.redirect_endpoints().begin(),
        p.second.redirect_endpoints().end());

    TestRedirectsAreEqual(lhs_redirects, rhs_redirects);
  }
}

void ResourcePrefetchPredictorTablesTest::TestRedirectsAreEqual(
    const std::vector<RedirectStat>& lhs,
    const std::vector<RedirectStat>& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  std::map<std::string, RedirectStat> lhs_index;
  // Repeated redirects are not allowed.
  for (const auto& r : lhs)
    EXPECT_TRUE(lhs_index.insert(std::make_pair(r.url(), r)).second);

  for (const auto& r : rhs) {
    auto lhs_it = lhs_index.find(r.url());
    if (lhs_it != lhs_index.end()) {
      EXPECT_EQ(r, lhs_it->second);
      lhs_index.erase(lhs_it);
    } else {
      ADD_FAILURE() << r.url();
    }
  }

  EXPECT_TRUE(lhs_index.empty());
}

void ResourcePrefetchPredictorTablesTest::AddKey(PrefetchDataMap* m,
                                                 const std::string& key) const {
  PrefetchDataMap::const_iterator it = test_url_data_.find(key);
  if (it != test_url_data_.end()) {
    m->insert(*it);
    return;
  }
  it = test_host_data_.find(key);
  ASSERT_TRUE(it != test_host_data_.end());
  m->insert(*it);
}

void ResourcePrefetchPredictorTablesTest::AddKey(RedirectDataMap* m,
                                                 const std::string& key) const {
  auto it = test_url_redirect_data_.find(key);
  if (it != test_url_redirect_data_.end()) {
    m->insert(*it);
    return;
  }
  it = test_host_redirect_data_.find(key);
  ASSERT_TRUE(it != test_host_redirect_data_.end());
  m->insert(*it);
}

void ResourcePrefetchPredictorTablesTest::InitializeSampleData() {
  PrefetchData empty_resource_data;
  RedirectData empty_redirect_data;

  {  // Url data.
    PrefetchData google = CreatePrefetchData("http://www.google.com", 1);
    InitializeResourceData(google.add_resources(),
                           "http://www.google.com/style.css",
                           content::RESOURCE_TYPE_STYLESHEET, 5, 2, 1, 1.1,
                           net::MEDIUM, false, false);
    InitializeResourceData(
        google.add_resources(), "http://www.google.com/script.js",
        content::RESOURCE_TYPE_SCRIPT, 4, 0, 1, 2.1, net::MEDIUM, false, false);
    InitializeResourceData(
        google.add_resources(), "http://www.google.com/image.png",
        content::RESOURCE_TYPE_IMAGE, 6, 3, 0, 2.2, net::MEDIUM, false, false);
    InitializeResourceData(google.add_resources(),
                           "http://www.google.com/a.font",
                           content::RESOURCE_TYPE_LAST_TYPE, 2, 0, 0, 5.1,
                           net::MEDIUM, false, false);
    InitializeResourceData(google.add_resources(),
                           "http://www.resources.google.com/script.js",
                           content::RESOURCE_TYPE_SCRIPT, 11, 0, 0, 8.5,
                           net::MEDIUM, false, false);

    PrefetchData reddit = CreatePrefetchData("http://www.reddit.com", 2);
    InitializeResourceData(
        reddit.add_resources(), "http://reddit-resource.com/script1.js",
        content::RESOURCE_TYPE_SCRIPT, 4, 0, 1, 1.0, net::MEDIUM, false, false);
    InitializeResourceData(
        reddit.add_resources(), "http://reddit-resource.com/script2.js",
        content::RESOURCE_TYPE_SCRIPT, 2, 0, 0, 2.1, net::MEDIUM, false, false);

    PrefetchData yahoo = CreatePrefetchData("http://www.yahoo.com", 3);
    InitializeResourceData(yahoo.add_resources(),
                           "http://www.google.com/image.png",
                           content::RESOURCE_TYPE_IMAGE, 20, 1, 0, 10.0,
                           net::MEDIUM, false, false);

    test_url_data_.clear();
    test_url_data_.insert(std::make_pair(google.primary_key(), google));
    test_url_data_.insert(std::make_pair(reddit.primary_key(), reddit));
    test_url_data_.insert(std::make_pair(yahoo.primary_key(), yahoo));

    tables_->UpdateData(google, empty_resource_data, empty_redirect_data,
                        empty_redirect_data);
    tables_->UpdateData(reddit, empty_resource_data, empty_redirect_data,
                        empty_redirect_data);
    tables_->UpdateData(yahoo, empty_resource_data, empty_redirect_data,
                        empty_redirect_data);
  }

  {  // Host data.
    PrefetchData facebook = CreatePrefetchData("www.facebook.com", 4);
    InitializeResourceData(facebook.add_resources(),
                           "http://www.facebook.com/style.css",
                           content::RESOURCE_TYPE_STYLESHEET, 5, 2, 1, 1.1,
                           net::MEDIUM, false, false);
    InitializeResourceData(
        facebook.add_resources(), "http://www.facebook.com/script.js",
        content::RESOURCE_TYPE_SCRIPT, 4, 0, 1, 2.1, net::MEDIUM, false, false);
    InitializeResourceData(
        facebook.add_resources(), "http://www.facebook.com/image.png",
        content::RESOURCE_TYPE_IMAGE, 6, 3, 0, 2.2, net::MEDIUM, false, false);
    InitializeResourceData(facebook.add_resources(),
                           "http://www.facebook.com/a.font",
                           content::RESOURCE_TYPE_LAST_TYPE, 2, 0, 0, 5.1,
                           net::MEDIUM, false, false);
    InitializeResourceData(facebook.add_resources(),
                           "http://www.resources.facebook.com/script.js",
                           content::RESOURCE_TYPE_SCRIPT, 11, 0, 0, 8.5,
                           net::MEDIUM, false, false);

    PrefetchData yahoo = CreatePrefetchData("www.yahoo.com", 5);
    InitializeResourceData(yahoo.add_resources(),
                           "http://www.google.com/image.png",
                           content::RESOURCE_TYPE_IMAGE, 20, 1, 0, 10.0,
                           net::MEDIUM, false, false);

    test_host_data_.clear();
    test_host_data_.insert(std::make_pair(facebook.primary_key(), facebook));
    test_host_data_.insert(std::make_pair(yahoo.primary_key(), yahoo));

    tables_->UpdateData(empty_resource_data, facebook, empty_redirect_data,
                        empty_redirect_data);
    tables_->UpdateData(empty_resource_data, yahoo, empty_redirect_data,
                        empty_redirect_data);
  }

  {  // Url redirect data.
    RedirectData facebook = CreateRedirectData("http://fb.com/google", 6);
    InitializeRedirectStat(facebook.add_redirect_endpoints(),
                           "https://facebook.com/google", 5, 1, 0);
    InitializeRedirectStat(facebook.add_redirect_endpoints(),
                           "https://facebook.com/login", 3, 5, 1);

    RedirectData nytimes = CreateRedirectData("http://nyt.com", 7);
    InitializeRedirectStat(nytimes.add_redirect_endpoints(),
                           "https://nytimes.com", 2, 0, 0);

    RedirectData google = CreateRedirectData("http://google.com", 8);
    InitializeRedirectStat(google.add_redirect_endpoints(),
                           "https://google.com", 3, 0, 0);

    test_url_redirect_data_.clear();
    test_url_redirect_data_.insert(
        std::make_pair(facebook.primary_key(), facebook));
    test_url_redirect_data_.insert(
        std::make_pair(nytimes.primary_key(), nytimes));
    test_url_redirect_data_.insert(
        std::make_pair(google.primary_key(), google));

    tables_->UpdateData(empty_resource_data, empty_resource_data, facebook,
                        empty_redirect_data);
    tables_->UpdateData(empty_resource_data, empty_resource_data, nytimes,
                        empty_redirect_data);
    tables_->UpdateData(empty_resource_data, empty_resource_data, google,
                        empty_redirect_data);
  }

  {  // Host redirect data.
    RedirectData bbc = CreateRedirectData("bbc.com", 9);
    InitializeRedirectStat(bbc.add_redirect_endpoints(), "www.bbc.com", 8, 4,
                           1);
    InitializeRedirectStat(bbc.add_redirect_endpoints(), "m.bbc.com", 5, 8, 0);
    InitializeRedirectStat(bbc.add_redirect_endpoints(), "bbc.co.uk", 1, 3, 0);

    RedirectData microsoft = CreateRedirectData("microsoft.com", 10);
    InitializeRedirectStat(microsoft.add_redirect_endpoints(),
                           "www.microsoft.com", 10, 0, 0);

    test_host_redirect_data_.clear();
    test_host_redirect_data_.insert(std::make_pair(bbc.primary_key(), bbc));
    test_host_redirect_data_.insert(
        std::make_pair(microsoft.primary_key(), microsoft));
    tables_->UpdateData(empty_resource_data, empty_resource_data,
                        empty_redirect_data, bbc);
    tables_->UpdateData(empty_resource_data, empty_resource_data,
                        empty_redirect_data, microsoft);
  }
}

void ResourcePrefetchPredictorTablesTest::ReopenDatabase() {
  db_.reset(new PredictorDatabase(&profile_));
  base::RunLoop().RunUntilIdle();
  tables_ = db_->resource_prefetch_tables();
}

// Test cases.

TEST_F(ResourcePrefetchPredictorTablesTest, ComputeResourceScore) {
  auto compute_score = [](net::RequestPriority priority,
                          content::ResourceType resource_type,
                          double average_position) {
    ResourceData resource;
    InitializeResourceData(&resource, "", resource_type, 0, 0, 0,
                           average_position, priority, false, false);
    return ResourcePrefetchPredictorTables::ComputeResourceScore(resource);
  };

  // Priority is more important than the rest.
  EXPECT_TRUE(compute_score(net::HIGHEST, content::RESOURCE_TYPE_SCRIPT, 1.) >
              compute_score(net::HIGHEST, content::RESOURCE_TYPE_IMAGE, 42.));

  EXPECT_TRUE(compute_score(net::HIGHEST, content::RESOURCE_TYPE_IMAGE, 42.) >
              compute_score(net::MEDIUM, content::RESOURCE_TYPE_SCRIPT, 1.));
  EXPECT_TRUE(compute_score(net::HIGHEST, content::RESOURCE_TYPE_IMAGE, 42.) >
              compute_score(net::LOW, content::RESOURCE_TYPE_SCRIPT, 1.));
  EXPECT_TRUE(compute_score(net::HIGHEST, content::RESOURCE_TYPE_IMAGE, 42.) >
              compute_score(net::LOWEST, content::RESOURCE_TYPE_SCRIPT, 1.));
  EXPECT_TRUE(compute_score(net::HIGHEST, content::RESOURCE_TYPE_IMAGE, 42.) >
              compute_score(net::IDLE, content::RESOURCE_TYPE_SCRIPT, 1.));

  // Scripts and stylesheets are equivalent.
  EXPECT_NEAR(
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_SCRIPT, 42.),
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_STYLESHEET, 42.),
      1e-4);

  // Scripts are more important than fonts and images, and the rest.
  EXPECT_TRUE(
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_SCRIPT, 42.) >
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_FONT_RESOURCE, 42.));
  EXPECT_TRUE(
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_FONT_RESOURCE, 42.) >
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_IMAGE, 42.));
  EXPECT_TRUE(
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_FONT_RESOURCE, 42.) >
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_FAVICON, 42.));

  // All else being equal, position matters.
  EXPECT_TRUE(compute_score(net::HIGHEST, content::RESOURCE_TYPE_SCRIPT, 12.) >
              compute_score(net::HIGHEST, content::RESOURCE_TYPE_SCRIPT, 42.));
}

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

TEST_F(ResourcePrefetchPredictorTablesTest, DatabaseVersionIsSet) {
  sql::Connection* db = tables_->DB();
  const int version = ResourcePrefetchPredictorTables::kDatabaseVersion;
  EXPECT_EQ(version, ResourcePrefetchPredictorTables::GetDatabaseVersion(db));
}

TEST_F(ResourcePrefetchPredictorTablesTest, DatabaseIsResetWhenIncompatible) {
  const int version = ResourcePrefetchPredictorTables::kDatabaseVersion;
  sql::Connection* db = tables_->DB();
  ASSERT_TRUE(
      ResourcePrefetchPredictorTables::SetDatabaseVersion(db, version + 1));
  EXPECT_EQ(version + 1,
            ResourcePrefetchPredictorTables::GetDatabaseVersion(db));

  ReopenDatabase();

  db = tables_->DB();
  ASSERT_EQ(version, ResourcePrefetchPredictorTables::GetDatabaseVersion(db));

  PrefetchDataMap url_data, host_data;
  RedirectDataMap url_redirect_data, host_redirect_data;
  tables_->GetAllData(&url_data, &host_data, &url_redirect_data,
                      &host_redirect_data);
  EXPECT_TRUE(url_data.empty());
  EXPECT_TRUE(host_data.empty());
  EXPECT_TRUE(url_redirect_data.empty());
  EXPECT_TRUE(host_redirect_data.empty());
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
