// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/base/request_priority.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace predictors {

class ResourcePrefetchPredictorTablesTest : public testing::Test {
 public:
  ResourcePrefetchPredictorTablesTest();
  ~ResourcePrefetchPredictorTablesTest() override;

  using PrefetchDataMap = std::map<std::string, PrefetchData>;
  using RedirectDataMap = std::map<std::string, RedirectData>;
  using OriginDataMap = std::map<std::string, OriginData>;

  void SetUp() override;
  void TearDown() override;

  void DeleteAllData();
  void GetAllData(PrefetchDataMap* url_resource_data,
                  PrefetchDataMap* host_resource_data,
                  RedirectDataMap* url_redirect_data,
                  RedirectDataMap* host_redirect_data,
                  OriginDataMap* origin_data) const;

 protected:
  void ReopenDatabase();
  void TestGetAllData();
  void TestUpdateData();
  void TestDeleteData();
  void TestDeleteAllData();

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  TestingProfile profile_;
  std::unique_ptr<PredictorDatabase> db_;
  scoped_refptr<ResourcePrefetchPredictorTables> tables_;

 private:
  // Initializes the tables, |test_url_data_|, |test_host_data_|,
  // |test_url_redirect_data_|, |test_host_redirect_data_| and
  // |test_origin_data|.
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

  void TestOriginDataAreEqual(const OriginDataMap& lhs,
                              const OriginDataMap& rhs) const;
  void TestOriginStatsAreEqual(const std::vector<OriginStat>& lhs,
                               const std::vector<OriginStat>& rhs) const;

  void AddKey(PrefetchDataMap* m, const std::string& key) const;
  void AddKey(RedirectDataMap* m, const std::string& key) const;
  void AddKey(OriginDataMap* m, const std::string& key) const;

  PrefetchDataMap test_url_data_;
  PrefetchDataMap test_host_data_;
  RedirectDataMap test_url_redirect_data_;
  RedirectDataMap test_host_redirect_data_;
  OriginDataMap test_origin_data_;
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
    : task_runner_(base::SequencedTaskRunnerHandle::Get()),
      db_(base::MakeUnique<PredictorDatabase>(&profile_, task_runner_)),
      tables_(db_->resource_prefetch_tables()) {
  content::RunAllTasksUntilIdle();
}

ResourcePrefetchPredictorTablesTest::~ResourcePrefetchPredictorTablesTest() {
}

void ResourcePrefetchPredictorTablesTest::SetUp() {
  DeleteAllData();
  InitializeSampleData();
  content::RunAllTasksUntilIdle();
}

void ResourcePrefetchPredictorTablesTest::TearDown() {
  tables_ = nullptr;
  db_ = nullptr;
  content::RunAllTasksUntilIdle();
}

void ResourcePrefetchPredictorTablesTest::TestGetAllData() {
  PrefetchDataMap actual_url_data, actual_host_data;
  RedirectDataMap actual_url_redirect_data, actual_host_redirect_data;
  OriginDataMap actual_origin_data;

  GetAllData(&actual_url_data, &actual_host_data, &actual_url_redirect_data,
             &actual_host_redirect_data, &actual_origin_data);

  TestPrefetchDataAreEqual(test_url_data_, actual_url_data);
  TestPrefetchDataAreEqual(test_host_data_, actual_host_data);
  TestRedirectDataAreEqual(test_url_redirect_data_, actual_url_redirect_data);
  TestRedirectDataAreEqual(test_host_redirect_data_, actual_host_redirect_data);
  TestOriginDataAreEqual(test_origin_data_, actual_origin_data);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteData() {
  std::vector<std::string> urls_to_delete = {"http://www.google.com",
                                             "http://www.yahoo.com"};
  std::vector<std::string> hosts_to_delete = {"www.yahoo.com"};
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &GlowplugKeyValueTable<PrefetchData>::DeleteData,
      base::Unretained(tables_->url_resource_table()), urls_to_delete));
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &GlowplugKeyValueTable<PrefetchData>::DeleteData,
      base::Unretained(tables_->host_resource_table()), hosts_to_delete));

  urls_to_delete = {"http://fb.com/google", "http://google.com"};
  hosts_to_delete = {"microsoft.com"};
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &GlowplugKeyValueTable<RedirectData>::DeleteData,
      base::Unretained(tables_->url_redirect_table()), urls_to_delete));
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &GlowplugKeyValueTable<RedirectData>::DeleteData,
      base::Unretained(tables_->host_redirect_table()), hosts_to_delete));

  hosts_to_delete = {"twitter.com"};
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &GlowplugKeyValueTable<OriginData>::DeleteData,
      base::Unretained(tables_->origin_table()), hosts_to_delete));

  PrefetchDataMap actual_url_data, actual_host_data;
  RedirectDataMap actual_url_redirect_data, actual_host_redirect_data;
  OriginDataMap actual_origin_data;

  GetAllData(&actual_url_data, &actual_host_data, &actual_url_redirect_data,
             &actual_host_redirect_data, &actual_origin_data);

  PrefetchDataMap expected_url_data, expected_host_data;
  RedirectDataMap expected_url_redirect_data, expected_host_redirect_data;
  OriginDataMap expected_origin_data;
  AddKey(&expected_url_data, "http://www.reddit.com");
  AddKey(&expected_host_data, "www.facebook.com");
  AddKey(&expected_url_redirect_data, "http://nyt.com");
  AddKey(&expected_host_redirect_data, "bbc.com");
  AddKey(&expected_origin_data, "abc.xyz");

  TestPrefetchDataAreEqual(expected_url_data, actual_url_data);
  TestPrefetchDataAreEqual(expected_host_data, actual_host_data);
  TestRedirectDataAreEqual(expected_url_redirect_data,
                           actual_url_redirect_data);
  TestRedirectDataAreEqual(expected_host_redirect_data,
                           actual_host_redirect_data);
  TestOriginDataAreEqual(expected_origin_data, actual_origin_data);
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

  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&GlowplugKeyValueTable<PrefetchData>::UpdateData,
                     base::Unretained(tables_->url_resource_table()),
                     google.primary_key(), google));

  PrefetchData yahoo = CreatePrefetchData("www.yahoo.com", 7);
  InitializeResourceData(
      yahoo.add_resources(), "http://www.yahoo.com/image.png",
      content::RESOURCE_TYPE_IMAGE, 120, 1, 1, 10.0, net::MEDIUM, true, false);

  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&GlowplugKeyValueTable<PrefetchData>::UpdateData,
                     base::Unretained(tables_->host_resource_table()),
                     yahoo.primary_key(), yahoo));

  RedirectData facebook = CreateRedirectData("http://fb.com/google", 20);
  InitializeRedirectStat(facebook.add_redirect_endpoints(),
                         "https://facebook.fr/google", 4, 2, 1);

  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&GlowplugKeyValueTable<RedirectData>::UpdateData,
                     base::Unretained(tables_->url_redirect_table()),
                     facebook.primary_key(), facebook));

  RedirectData microsoft = CreateRedirectData("microsoft.com", 21);
  InitializeRedirectStat(microsoft.add_redirect_endpoints(), "m.microsoft.com",
                         5, 7, 1);
  InitializeRedirectStat(microsoft.add_redirect_endpoints(), "microsoft.org", 7,
                         2, 0);

  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&GlowplugKeyValueTable<RedirectData>::UpdateData,
                     base::Unretained(tables_->host_redirect_table()),
                     microsoft.primary_key(), microsoft));

  OriginData twitter = CreateOriginData("twitter.com");
  InitializeOriginStat(twitter.add_origins(), "https://dogs.twitter.com", 10, 1,
                       0, 12., false, true);
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &GlowplugKeyValueTable<OriginData>::UpdateData,
      base::Unretained(tables_->origin_table()), twitter.host(), twitter));

  PrefetchDataMap actual_url_data, actual_host_data;
  RedirectDataMap actual_url_redirect_data, actual_host_redirect_data;
  OriginDataMap actual_origin_data;
  GetAllData(&actual_url_data, &actual_host_data, &actual_url_redirect_data,
             &actual_host_redirect_data, &actual_origin_data);

  PrefetchDataMap expected_url_data, expected_host_data;
  RedirectDataMap expected_url_redirect_data, expected_host_redirect_data;
  OriginDataMap expected_origin_data;
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

  AddKey(&expected_origin_data, "abc.xyz");
  expected_origin_data.insert({"twitter.com", twitter});

  TestPrefetchDataAreEqual(expected_url_data, actual_url_data);
  TestPrefetchDataAreEqual(expected_host_data, actual_host_data);
  TestRedirectDataAreEqual(expected_url_redirect_data,
                           actual_url_redirect_data);
  TestRedirectDataAreEqual(expected_host_redirect_data,
                           actual_host_redirect_data);
}

void ResourcePrefetchPredictorTablesTest::TestDeleteAllData() {
  DeleteAllData();

  PrefetchDataMap actual_url_data, actual_host_data;
  RedirectDataMap actual_url_redirect_data, actual_host_redirect_data;
  OriginDataMap actual_origin_data;
  GetAllData(&actual_url_data, &actual_host_data, &actual_url_redirect_data,
             &actual_host_redirect_data, &actual_origin_data);
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

void ResourcePrefetchPredictorTablesTest::TestOriginDataAreEqual(
    const OriginDataMap& lhs,
    const OriginDataMap& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  for (const auto& o : rhs) {
    const auto lhs_it = lhs.find(o.first);
    ASSERT_TRUE(lhs_it != lhs.end()) << o.first;
    std::vector<OriginStat> lhs_origins(lhs_it->second.origins().begin(),
                                        lhs_it->second.origins().end());
    std::vector<OriginStat> rhs_origins(o.second.origins().begin(),
                                        o.second.origins().end());

    TestOriginStatsAreEqual(lhs_origins, rhs_origins);
  }
}

void ResourcePrefetchPredictorTablesTest::TestOriginStatsAreEqual(
    const std::vector<OriginStat>& lhs,
    const std::vector<OriginStat>& rhs) const {
  EXPECT_EQ(lhs.size(), rhs.size());

  std::map<std::string, OriginStat> lhs_index;
  // Repeated origins are not allowed.
  for (const auto& o : lhs)
    EXPECT_TRUE(lhs_index.insert({o.origin(), o}).second);

  for (const auto& o : rhs) {
    auto lhs_it = lhs_index.find(o.origin());
    if (lhs_it != lhs_index.end()) {
      EXPECT_EQ(o, lhs_it->second);
      lhs_index.erase(lhs_it);
    } else {
      ADD_FAILURE() << o.origin();
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

void ResourcePrefetchPredictorTablesTest::AddKey(OriginDataMap* m,
                                                 const std::string& key) const {
  auto it = test_origin_data_.find(key);
  ASSERT_TRUE(it != test_origin_data_.end());
  m->insert(*it);
}

void ResourcePrefetchPredictorTablesTest::DeleteAllData() {
  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&GlowplugKeyValueTable<PrefetchData>::DeleteAllData,
                     base::Unretained(tables_->url_resource_table())));
  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&GlowplugKeyValueTable<RedirectData>::DeleteAllData,
                     base::Unretained(tables_->url_redirect_table())));
  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&GlowplugKeyValueTable<PrefetchData>::DeleteAllData,
                     base::Unretained(tables_->host_resource_table())));
  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&GlowplugKeyValueTable<RedirectData>::DeleteAllData,
                     base::Unretained(tables_->host_redirect_table())));
  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&GlowplugKeyValueTable<OriginData>::DeleteAllData,
                     base::Unretained(tables_->origin_table())));
}

void ResourcePrefetchPredictorTablesTest::GetAllData(
    PrefetchDataMap* url_resource_data,
    PrefetchDataMap* host_resource_data,
    RedirectDataMap* url_redirect_data,
    RedirectDataMap* host_redirect_data,
    OriginDataMap* origin_data) const {
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &GlowplugKeyValueTable<PrefetchData>::GetAllData,
      base::Unretained(tables_->url_resource_table()), url_resource_data));
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &GlowplugKeyValueTable<RedirectData>::GetAllData,
      base::Unretained(tables_->url_redirect_table()), url_redirect_data));
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &GlowplugKeyValueTable<PrefetchData>::GetAllData,
      base::Unretained(tables_->host_resource_table()), host_resource_data));
  tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
      &GlowplugKeyValueTable<RedirectData>::GetAllData,
      base::Unretained(tables_->host_redirect_table()), host_redirect_data));
  tables_->ExecuteDBTaskOnDBSequence(
      base::BindOnce(&GlowplugKeyValueTable<OriginData>::GetAllData,
                     base::Unretained(tables_->origin_table()), origin_data));
}

void ResourcePrefetchPredictorTablesTest::InitializeSampleData() {
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

    tables_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&GlowplugKeyValueTable<PrefetchData>::UpdateData,
                       base::Unretained(tables_->url_resource_table()),
                       google.primary_key(), google));
    tables_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&GlowplugKeyValueTable<PrefetchData>::UpdateData,
                       base::Unretained(tables_->url_resource_table()),
                       reddit.primary_key(), reddit));
    tables_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&GlowplugKeyValueTable<PrefetchData>::UpdateData,
                       base::Unretained(tables_->url_resource_table()),
                       yahoo.primary_key(), yahoo));
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

    tables_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&GlowplugKeyValueTable<PrefetchData>::UpdateData,
                       base::Unretained(tables_->host_resource_table()),
                       facebook.primary_key(), facebook));
    tables_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&GlowplugKeyValueTable<PrefetchData>::UpdateData,
                       base::Unretained(tables_->host_resource_table()),
                       yahoo.primary_key(), yahoo));
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

    tables_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&GlowplugKeyValueTable<RedirectData>::UpdateData,
                       base::Unretained(tables_->url_redirect_table()),
                       facebook.primary_key(), facebook));
    tables_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&GlowplugKeyValueTable<RedirectData>::UpdateData,
                       base::Unretained(tables_->url_redirect_table()),
                       nytimes.primary_key(), nytimes));
    tables_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&GlowplugKeyValueTable<RedirectData>::UpdateData,
                       base::Unretained(tables_->url_redirect_table()),
                       google.primary_key(), google));
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

    tables_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&GlowplugKeyValueTable<RedirectData>::UpdateData,
                       base::Unretained(tables_->host_redirect_table()),
                       bbc.primary_key(), bbc));
    tables_->ExecuteDBTaskOnDBSequence(
        base::BindOnce(&GlowplugKeyValueTable<RedirectData>::UpdateData,
                       base::Unretained(tables_->host_redirect_table()),
                       microsoft.primary_key(), microsoft));
  }

  {  // Origin data.
    OriginData twitter;
    twitter.set_host("twitter.com");
    InitializeOriginStat(twitter.add_origins(), "https://cdn.twitter.com", 10,
                         1, 0, 12., false, true);
    InitializeOriginStat(twitter.add_origins(), "https://cats.twitter.com", 10,
                         1, 0, 20., false, true);

    OriginData alphabet;
    alphabet.set_host("abc.xyz");
    InitializeOriginStat(alphabet.add_origins(), "https://abc.def.xyz", 10, 1,
                         0, 12., false, true);
    InitializeOriginStat(alphabet.add_origins(), "https://alpha.beta.com", 10,
                         1, 0, 20., false, true);

    test_origin_data_.clear();
    test_origin_data_.insert({"twitter.com", twitter});
    test_origin_data_.insert({"abc.xyz", alphabet});

    tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
        &GlowplugKeyValueTable<OriginData>::UpdateData,
        base::Unretained(tables_->origin_table()), twitter.host(), twitter));
    tables_->ExecuteDBTaskOnDBSequence(base::BindOnce(
        &GlowplugKeyValueTable<OriginData>::UpdateData,
        base::Unretained(tables_->origin_table()), alphabet.host(), alphabet));
  }
}

void ResourcePrefetchPredictorTablesTest::ReopenDatabase() {
  db_ = base::MakeUnique<PredictorDatabase>(&profile_, task_runner_);
  content::RunAllTasksUntilIdle();
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
  EXPECT_GT(compute_score(net::HIGHEST, content::RESOURCE_TYPE_SCRIPT, 1.),
            compute_score(net::HIGHEST, content::RESOURCE_TYPE_IMAGE, 42.));

  EXPECT_GT(compute_score(net::HIGHEST, content::RESOURCE_TYPE_IMAGE, 42.),
            compute_score(net::MEDIUM, content::RESOURCE_TYPE_SCRIPT, 1.));
  EXPECT_GT(compute_score(net::HIGHEST, content::RESOURCE_TYPE_IMAGE, 42.),
            compute_score(net::LOW, content::RESOURCE_TYPE_SCRIPT, 1.));
  EXPECT_GT(compute_score(net::HIGHEST, content::RESOURCE_TYPE_IMAGE, 42.),
            compute_score(net::LOWEST, content::RESOURCE_TYPE_SCRIPT, 1.));
  EXPECT_GT(compute_score(net::HIGHEST, content::RESOURCE_TYPE_IMAGE, 42.),
            compute_score(net::IDLE, content::RESOURCE_TYPE_SCRIPT, 1.));

  // Stylesheets are are more important than scripts, fonts and images, and the
  // rest.
  EXPECT_GT(compute_score(net::HIGHEST, content::RESOURCE_TYPE_STYLESHEET, 42.),
            compute_score(net::HIGHEST, content::RESOURCE_TYPE_SCRIPT, 42.));
  EXPECT_GT(
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_SCRIPT, 42.),
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_FONT_RESOURCE, 42.));
  EXPECT_GT(
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_FONT_RESOURCE, 42.),
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_IMAGE, 42.));
  EXPECT_GT(
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_FONT_RESOURCE, 42.),
      compute_score(net::HIGHEST, content::RESOURCE_TYPE_FAVICON, 42.));

  // All else being equal, position matters.
  EXPECT_GT(compute_score(net::HIGHEST, content::RESOURCE_TYPE_SCRIPT, 12.),
            compute_score(net::HIGHEST, content::RESOURCE_TYPE_SCRIPT, 42.));
}

TEST_F(ResourcePrefetchPredictorTablesTest, ComputeOriginScore) {
  auto compute_score = [](int hits, int misses, double average_position,
                          bool always_access_network, bool accessed_network) {
    OriginStat origin;
    InitializeOriginStat(&origin, "", hits, misses, 0, average_position,
                         always_access_network, accessed_network);
    return ResourcePrefetchPredictorTables::ComputeOriginScore(origin);
  };

  // High-confidence is more important than the rest.
  EXPECT_GT(compute_score(20, 2, 12., false, false),
            compute_score(2, 0, 12., false, false));
  EXPECT_GT(compute_score(20, 2, 12., false, false),
            compute_score(2, 0, 2., true, true));

  // Don't care about the confidence as long as it's high.
  EXPECT_NEAR(compute_score(20, 2, 12., false, false),
              compute_score(50, 6, 12., false, false), 1e-4);

  // Mandatory network access.
  EXPECT_GT(compute_score(1, 1, 12., true, false),
            compute_score(1, 1, 12., false, false));
  EXPECT_GT(compute_score(1, 1, 12., true, false),
            compute_score(1, 1, 12., false, true));
  EXPECT_GT(compute_score(1, 1, 12., true, false),
            compute_score(1, 1, 2., false, true));

  // Accessed network.
  EXPECT_GT(compute_score(1, 1, 12., false, true),
            compute_score(1, 1, 12., false, false));
  EXPECT_GT(compute_score(1, 1, 12., false, true),
            compute_score(1, 1, 2., false, false));

  // All else being equal, position matters.
  EXPECT_GT(compute_score(1, 1, 2., false, false),
            compute_score(1, 1, 12., false, false));
  EXPECT_GT(compute_score(1, 1, 2., true, true),
            compute_score(1, 1, 12., true, true));
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
  OriginDataMap origin_data;
  GetAllData(&url_data, &host_data, &url_redirect_data, &host_redirect_data,
             &origin_data);
  EXPECT_TRUE(url_data.empty());
  EXPECT_TRUE(host_data.empty());
  EXPECT_TRUE(url_redirect_data.empty());
  EXPECT_TRUE(host_redirect_data.empty());
  EXPECT_TRUE(origin_data.empty());
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

TEST_F(ResourcePrefetchPredictorTablesReopenTest, DeleteAllData) {
  TestDeleteAllData();
}

}  // namespace predictors
