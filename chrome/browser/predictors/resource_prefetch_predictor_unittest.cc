// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor.h"

#include <iostream>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;
using testing::UnorderedElementsAre;

namespace predictors {

using PrefetchDataMap = std::map<std::string, PrefetchData>;
using RedirectDataMap = std::map<std::string, RedirectData>;
using OriginDataMap = std::map<std::string, OriginData>;

template <typename T>
class FakeGlowplugKeyValueTable : public GlowplugKeyValueTable<T> {
 public:
  FakeGlowplugKeyValueTable() : GlowplugKeyValueTable<T>("") {}
  void GetAllData(std::map<std::string, T>* data_map,
                  sql::Connection* db) const override {
    *data_map = data_;
  }
  void UpdateData(const std::string& key,
                  const T& data,
                  sql::Connection* db) override {
    data_[key] = data;
  }
  void DeleteData(const std::vector<std::string>& keys,
                  sql::Connection* db) override {
    for (const auto& key : keys)
      data_.erase(key);
  }
  void DeleteAllData(sql::Connection* db) override { data_.clear(); }

  std::map<std::string, T> data_;
};

class MockResourcePrefetchPredictorTables
    : public ResourcePrefetchPredictorTables {
 public:
  MockResourcePrefetchPredictorTables(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner)
      : ResourcePrefetchPredictorTables(std::move(db_task_runner)) {}

  void ScheduleDBTask(const base::Location& from_here, DBTask task) override {
    ExecuteDBTaskOnDBSequence(std::move(task));
  }

  void ExecuteDBTaskOnDBSequence(DBTask task) override {
    std::move(task).Run(nullptr);
  }

  GlowplugKeyValueTable<PrefetchData>* url_resource_table() override {
    return &url_resource_table_;
  }

  GlowplugKeyValueTable<RedirectData>* url_redirect_table() override {
    return &url_redirect_table_;
  }

  GlowplugKeyValueTable<PrefetchData>* host_resource_table() override {
    return &host_resource_table_;
  }

  GlowplugKeyValueTable<RedirectData>* host_redirect_table() override {
    return &host_redirect_table_;
  }

  GlowplugKeyValueTable<OriginData>* origin_table() override {
    return &origin_table_;
  }

  FakeGlowplugKeyValueTable<PrefetchData> url_resource_table_;
  FakeGlowplugKeyValueTable<RedirectData> url_redirect_table_;
  FakeGlowplugKeyValueTable<PrefetchData> host_resource_table_;
  FakeGlowplugKeyValueTable<RedirectData> host_redirect_table_;
  FakeGlowplugKeyValueTable<OriginData> origin_table_;

 protected:
  ~MockResourcePrefetchPredictorTables() override = default;
};

class MockResourcePrefetchPredictorObserver : public TestObserver {
 public:
  explicit MockResourcePrefetchPredictorObserver(
      ResourcePrefetchPredictor* predictor)
      : TestObserver(predictor) {}

  MOCK_METHOD2(OnNavigationLearned,
               void(size_t url_visit_count, const PageRequestSummary& summary));
};

class ResourcePrefetchPredictorTest : public testing::Test {
 public:
  ResourcePrefetchPredictorTest();
  ~ResourcePrefetchPredictorTest() override;
  void SetUp() override;
  void TearDown() override;

 protected:
  void AddUrlToHistory(const std::string& url, int visit_count) {
    HistoryServiceFactory::GetForProfile(profile_.get(),
                                         ServiceAccessType::EXPLICIT_ACCESS)->
        AddPageWithDetails(
            GURL(url),
            base::string16(),
            visit_count,
            0,
            base::Time::Now(),
            false,
            history::SOURCE_BROWSED);
    profile_->BlockUntilHistoryProcessesPendingRequests();
  }

  void InitializePredictor() {
    loading_predictor_->StartInitialization();
    db_task_runner_->RunUntilIdle();
    profile_->BlockUntilHistoryProcessesPendingRequests();
  }

  void ResetPredictor(bool small_db = true) {
    if (loading_predictor_)
      loading_predictor_->Shutdown();

    LoadingPredictorConfig config;
    PopulateTestConfig(&config, small_db);
    loading_predictor_ =
        base::MakeUnique<LoadingPredictor>(config, profile_.get());
    predictor_ = loading_predictor_->resource_prefetch_predictor();
    predictor_->set_mock_tables(mock_tables_);
  }

  void InitializeSampleData();

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  scoped_refptr<base::TestSimpleTaskRunner> db_task_runner_;
  net::TestURLRequestContext url_request_context_;

  std::unique_ptr<LoadingPredictor> loading_predictor_;
  ResourcePrefetchPredictor* predictor_;
  scoped_refptr<StrictMock<MockResourcePrefetchPredictorTables>> mock_tables_;

  PrefetchDataMap test_url_data_;
  PrefetchDataMap test_host_data_;
  RedirectDataMap test_url_redirect_data_;
  RedirectDataMap test_host_redirect_data_;
  OriginDataMap test_origin_data_;

  MockURLRequestJobFactory url_request_job_factory_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

ResourcePrefetchPredictorTest::ResourcePrefetchPredictorTest()
    : profile_(base::MakeUnique<TestingProfile>()),
      db_task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
      mock_tables_(
          base::MakeRefCounted<StrictMock<MockResourcePrefetchPredictorTables>>(
              db_task_runner_)) {}

ResourcePrefetchPredictorTest::~ResourcePrefetchPredictorTest() = default;

void ResourcePrefetchPredictorTest::SetUp() {
  InitializeSampleData();

  CHECK(profile_->CreateHistoryService(true, false));
  profile_->BlockUntilHistoryProcessesPendingRequests();
  CHECK(HistoryServiceFactory::GetForProfile(
      profile_.get(), ServiceAccessType::EXPLICIT_ACCESS));
  // Initialize the predictor with empty data.
  ResetPredictor();
  // The first creation of the LoadingPredictor constructs the PredictorDatabase
  // for the |profile_|. The PredictorDatabase is initialized asynchronously and
  // we have to wait for the initialization completion even though the database
  // object is later replaced by a mock object.
  content::RunAllTasksUntilIdle();
  CHECK_EQ(predictor_->initialization_state_,
           ResourcePrefetchPredictor::NOT_INITIALIZED);
  InitializePredictor();
  CHECK_EQ(predictor_->initialization_state_,
           ResourcePrefetchPredictor::INITIALIZED);

  url_request_job_factory_.Reset();
  url_request_context_.set_job_factory(&url_request_job_factory_);

  histogram_tester_ = base::MakeUnique<base::HistogramTester>();
}

void ResourcePrefetchPredictorTest::TearDown() {
  EXPECT_EQ(*predictor_->url_resource_data_->data_cache_,
            mock_tables_->url_resource_table_.data_);
  EXPECT_EQ(*predictor_->url_redirect_data_->data_cache_,
            mock_tables_->url_redirect_table_.data_);
  EXPECT_EQ(*predictor_->host_resource_data_->data_cache_,
            mock_tables_->host_resource_table_.data_);
  EXPECT_EQ(*predictor_->host_redirect_data_->data_cache_,
            mock_tables_->host_redirect_table_.data_);
  EXPECT_EQ(*predictor_->origin_data_->data_cache_,
            mock_tables_->origin_table_.data_);
  loading_predictor_->Shutdown();
}

void ResourcePrefetchPredictorTest::InitializeSampleData() {
  {  // Url data.
    PrefetchData google = CreatePrefetchData("http://www.google.com/", 1);
    InitializeResourceData(google.add_resources(),
                           "http://google.com/style1.css",
                           content::RESOURCE_TYPE_STYLESHEET, 3, 2, 1, 1.0,
                           net::MEDIUM, false, false);
    InitializeResourceData(
        google.add_resources(), "http://google.com/script3.js",
        content::RESOURCE_TYPE_SCRIPT, 4, 0, 1, 2.1, net::MEDIUM, false, false);
    InitializeResourceData(google.add_resources(),
                           "http://google.com/script4.js",
                           content::RESOURCE_TYPE_SCRIPT, 11, 0, 0, 2.1,
                           net::MEDIUM, false, false);
    InitializeResourceData(
        google.add_resources(), "http://google.com/image1.png",
        content::RESOURCE_TYPE_IMAGE, 6, 3, 0, 2.2, net::MEDIUM, false, false);
    InitializeResourceData(google.add_resources(), "http://google.com/a.font",
                           content::RESOURCE_TYPE_LAST_TYPE, 2, 0, 0, 5.1,
                           net::MEDIUM, false, false);

    PrefetchData reddit = CreatePrefetchData("http://www.reddit.com/", 2);
    InitializeResourceData(
        reddit.add_resources(), "http://reddit-resource.com/script1.js",
        content::RESOURCE_TYPE_SCRIPT, 4, 0, 1, 1.0, net::MEDIUM, false, false);
    InitializeResourceData(
        reddit.add_resources(), "http://reddit-resource.com/script2.js",
        content::RESOURCE_TYPE_SCRIPT, 2, 0, 0, 2.1, net::MEDIUM, false, false);

    PrefetchData yahoo = CreatePrefetchData("http://www.yahoo.com/", 3);
    InitializeResourceData(yahoo.add_resources(), "http://google.com/image.png",
                           content::RESOURCE_TYPE_IMAGE, 20, 1, 0, 10.0,
                           net::MEDIUM, false, false);

    test_url_data_.clear();
    test_url_data_.insert(std::make_pair(google.primary_key(), google));
    test_url_data_.insert(std::make_pair(reddit.primary_key(), reddit));
    test_url_data_.insert(std::make_pair(yahoo.primary_key(), yahoo));
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
    InitializeResourceData(yahoo.add_resources(), "http://google.com/image.png",
                           content::RESOURCE_TYPE_IMAGE, 20, 1, 0, 10.0,
                           net::MEDIUM, false, false);

    test_host_data_.clear();
    test_host_data_.insert(std::make_pair(facebook.primary_key(), facebook));
    test_host_data_.insert(std::make_pair(yahoo.primary_key(), yahoo));
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
  }

  {  // Origin data.
    OriginData google = CreateOriginData("google.com", 12);
    InitializeOriginStat(google.add_origins(), "https://static.google.com", 12,
                         0, 0, 3., false, true);
    InitializeOriginStat(google.add_origins(), "https://cats.google.com", 12, 0,
                         0, 5., true, true);
    test_origin_data_.insert({"google.com", google});

    OriginData twitter = CreateOriginData("twitter.com", 42);
    InitializeOriginStat(twitter.add_origins(), "https://static.twitter.com",
                         12, 0, 0, 3., false, true);
    InitializeOriginStat(twitter.add_origins(), "https://random.140chars.com",
                         12, 0, 0, 3., false, true);
    test_origin_data_.insert({"twitter.com", twitter});
  }
}

// Confirm that there's been no shift in the
// ResourceData_Priority/net::RequestPriority equivalence.
static_assert(static_cast<int>(net::MINIMUM_PRIORITY) ==
              static_cast<int>(
                  ResourceData_Priority_REQUEST_PRIORITY_THROTTLED),
              "Database/Net priority mismatch: Minimum");
static_assert(static_cast<int>(net::MAXIMUM_PRIORITY) ==
              static_cast<int>(ResourceData_Priority_REQUEST_PRIORITY_HIGHEST),
              "Database/Net priority mismatch: Maximum");

// Tests that the predictor initializes correctly without any data.
TEST_F(ResourcePrefetchPredictorTest, LazilyInitializeEmpty) {
  EXPECT_TRUE(mock_tables_->url_resource_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->url_redirect_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->host_resource_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->host_redirect_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->origin_table_.data_.empty());
}

// Tests that the history and the db tables data are loaded correctly.
TEST_F(ResourcePrefetchPredictorTest, LazilyInitializeWithData) {
  mock_tables_->url_resource_table_.data_ = test_url_data_;
  mock_tables_->url_redirect_table_.data_ = test_url_redirect_data_;
  mock_tables_->host_resource_table_.data_ = test_host_data_;
  mock_tables_->host_redirect_table_.data_ = test_host_redirect_data_;
  mock_tables_->origin_table_.data_ = test_origin_data_;

  ResetPredictor();
  InitializePredictor();

  // Test that the internal variables correctly initialized.
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::INITIALIZED);

  // Integrity of the cache and the backend storage is checked on TearDown.
}

// Single navigation but history count is low, so should not record url data.
TEST_F(ResourcePrefetchPredictorTest, NavigationLowHistoryCount) {
  const int kVisitCount = 1;
  AddUrlToHistory("https://www.google.com", kVisitCount);

  URLRequestSummary main_frame =
      CreateURLRequestSummary(1, "http://www.google.com");

  URLRequestSummary main_frame_redirect = CreateRedirectRequestSummary(
      1, "http://www.google.com", "https://www.google.com");
  main_frame = CreateURLRequestSummary(1, "https://www.google.com");

  // Now add a few subresources.
  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, "https://www.google.com", "https://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false);
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, "https://www.google.com", "https://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  URLRequestSummary resource3 = CreateURLRequestSummary(
      1, "https://www.google.com", "https://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);

  auto page_summary = CreatePageRequestSummary(
      "https://www.google.com", "http://www.google.com",
      {resource1, resource2, resource3});

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(kVisitCount, page_summary));

  predictor_->RecordPageRequestSummary(
      base::MakeUnique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  PrefetchData host_data = CreatePrefetchData("www.google.com");
  InitializeResourceData(host_data.add_resources(),
                         "https://google.com/style1.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 1.0,
                         net::MEDIUM, false, false);
  InitializeResourceData(
      host_data.add_resources(), "https://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 2.0, net::MEDIUM, false, false);
  InitializeResourceData(
      host_data.add_resources(), "https://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 3.0, net::MEDIUM, false, false);
  EXPECT_EQ(mock_tables_->host_resource_table_.data_,
            PrefetchDataMap({{host_data.primary_key(), host_data}}));

  OriginData origin_data = CreateOriginData("www.google.com");
  InitializeOriginStat(origin_data.add_origins(), "https://www.google.com/", 1,
                       0, 0, 1., false, true);
  InitializeOriginStat(origin_data.add_origins(), "https://google.com/", 1, 0,
                       0, 2., false, true);
  EXPECT_EQ(mock_tables_->origin_table_.data_,
            OriginDataMap({{origin_data.host(), origin_data}}));

  RedirectData host_redirect_data = CreateRedirectData("www.google.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "www.google.com", 1, 0, 0);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
            RedirectDataMap(
                {{host_redirect_data.primary_key(), host_redirect_data}}));
}

// Single navigation that will be recorded. Will check for duplicate
// resources and also for number of resources saved.
TEST_F(ResourcePrefetchPredictorTest, NavigationUrlNotInDB) {
  const int kVisitCount = 4;
  AddUrlToHistory("http://www.google.com", kVisitCount);

  URLRequestSummary main_frame =
      CreateURLRequestSummary(1, "http://www.google.com");

  std::vector<URLRequestSummary> resources;
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false));
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", true));
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/image1.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/image2.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style2.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true));

  auto no_store = CreateURLRequestSummary(
      1, "http://www.google.com",
      "http://static.google.com/style2-no-store.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true);
  no_store.is_no_store = true;

  auto redirected = CreateURLRequestSummary(
      1, "http://www.google.com", "http://reader.google.com/style.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true);
  redirected.redirect_url = GURL("http://dev.null.google.com/style.css");

  auto page_summary = CreatePageRequestSummary(
      "http://www.google.com", "http://www.google.com", resources);
  page_summary.UpdateOrAddToOrigins(no_store);
  page_summary.UpdateOrAddToOrigins(redirected);

  redirected.is_no_store = true;
  redirected.request_url = redirected.redirect_url;
  redirected.redirect_url = GURL();
  page_summary.UpdateOrAddToOrigins(redirected);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(kVisitCount, page_summary));

  predictor_->RecordPageRequestSummary(
      base::MakeUnique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  PrefetchData url_data = CreatePrefetchData("http://www.google.com/");
  InitializeResourceData(url_data.add_resources(),
                         "http://google.com/style1.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 1.0,
                         net::MEDIUM, false, false);
  InitializeResourceData(url_data.add_resources(),
                         "http://google.com/style2.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 7.0,
                         net::MEDIUM, false, false);
  InitializeResourceData(
      url_data.add_resources(), "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 2.0, net::MEDIUM, false, false);
  InitializeResourceData(
      url_data.add_resources(), "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 3.0, net::MEDIUM, false, false);
  EXPECT_EQ(mock_tables_->url_resource_table_.data_,
            PrefetchDataMap({{url_data.primary_key(), url_data}}));

  PrefetchData host_data = CreatePrefetchData("www.google.com");
  host_data.mutable_resources()->CopyFrom(url_data.resources());
  EXPECT_EQ(mock_tables_->host_resource_table_.data_,
            PrefetchDataMap({{host_data.primary_key(), host_data}}));

  OriginData origin_data = CreateOriginData("www.google.com");
  InitializeOriginStat(origin_data.add_origins(), "http://www.google.com/", 1,
                       0, 0, 1., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://static.google.com/",
                       1, 0, 0, 3., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://dev.null.google.com/",
                       1, 0, 0, 5., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://google.com/", 1, 0, 0,
                       2., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://reader.google.com/",
                       1, 0, 0, 4., false, true);
  EXPECT_EQ(mock_tables_->origin_table_.data_,
            OriginDataMap({{origin_data.host(), origin_data}}));

  RedirectData url_redirect_data = CreateRedirectData("http://www.google.com/");
  InitializeRedirectStat(url_redirect_data.add_redirect_endpoints(),
                         "http://www.google.com/", 1, 0, 0);
  EXPECT_EQ(
      mock_tables_->url_redirect_table_.data_,
      RedirectDataMap({{url_redirect_data.primary_key(), url_redirect_data}}));

  RedirectData host_redirect_data = CreateRedirectData("www.google.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "www.google.com", 1, 0, 0);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
            RedirectDataMap(
                {{host_redirect_data.primary_key(), host_redirect_data}}));
}

// Tests that navigation is recorded correctly for URL already present in
// the database cache.
TEST_F(ResourcePrefetchPredictorTest, NavigationUrlInDB) {
  const int kVisitCount = 4;
  AddUrlToHistory("http://www.google.com", kVisitCount);

  mock_tables_->url_resource_table_.data_ = test_url_data_;
  mock_tables_->host_resource_table_.data_ = test_host_data_;

  ResetPredictor();
  InitializePredictor();

  URLRequestSummary main_frame = CreateURLRequestSummary(
      1, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);

  std::vector<URLRequestSummary> resources;
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false));
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", true));
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/image1.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/image2.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style2.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true));
  auto no_store = CreateURLRequestSummary(
      1, "http://www.google.com",
      "http://static.google.com/style2-no-store.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true);
  no_store.is_no_store = true;

  auto page_summary = CreatePageRequestSummary(
      "http://www.google.com", "http://www.google.com", resources);
  page_summary.UpdateOrAddToOrigins(no_store);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(kVisitCount, page_summary));

  predictor_->RecordPageRequestSummary(
      base::MakeUnique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  PrefetchData url_data = CreatePrefetchData("http://www.google.com/");
  InitializeResourceData(url_data.add_resources(),
                         "http://google.com/style1.css",
                         content::RESOURCE_TYPE_STYLESHEET, 4, 2, 0, 1.0,
                         net::MEDIUM, false, false);
  InitializeResourceData(url_data.add_resources(),
                         "http://google.com/style2.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 7.0,
                         net::MEDIUM, false, false);
  InitializeResourceData(
      url_data.add_resources(), "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 2.0, net::MEDIUM, false, false);
  InitializeResourceData(
      url_data.add_resources(), "http://google.com/script4.js",
      content::RESOURCE_TYPE_SCRIPT, 11, 1, 1, 2.1, net::MEDIUM, false, false);
  PrefetchDataMap expected_url_resource_data = test_url_data_;
  expected_url_resource_data.erase("www.facebook.com");
  expected_url_resource_data[url_data.primary_key()] = url_data;
  EXPECT_EQ(mock_tables_->url_resource_table_.data_,
            expected_url_resource_data);

  PrefetchData host_data = CreatePrefetchData("www.google.com");
  InitializeResourceData(host_data.add_resources(),
                         "http://google.com/style1.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 1.0,
                         net::MEDIUM, false, false);
  InitializeResourceData(host_data.add_resources(),
                         "http://google.com/style2.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 7.0,
                         net::MEDIUM, false, false);
  InitializeResourceData(
      host_data.add_resources(), "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 2.0, net::MEDIUM, false, false);
  InitializeResourceData(
      host_data.add_resources(), "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 3.0, net::MEDIUM, false, false);
  PrefetchDataMap expected_host_resource_data = test_host_data_;
  expected_host_resource_data.erase("www.facebook.com");
  expected_host_resource_data[host_data.primary_key()] = host_data;
  EXPECT_EQ(mock_tables_->host_resource_table_.data_,
            expected_host_resource_data);

  RedirectData url_redirect_data = CreateRedirectData("http://www.google.com/");
  InitializeRedirectStat(url_redirect_data.add_redirect_endpoints(),
                         "http://www.google.com/", 1, 0, 0);
  EXPECT_EQ(
      mock_tables_->url_redirect_table_.data_,
      RedirectDataMap({{url_redirect_data.primary_key(), url_redirect_data}}));

  RedirectData host_redirect_data = CreateRedirectData("www.google.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "www.google.com", 1, 0, 0);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
            RedirectDataMap(
                {{host_redirect_data.primary_key(), host_redirect_data}}));

  OriginData origin_data = CreateOriginData("www.google.com");
  InitializeOriginStat(origin_data.add_origins(), "http://www.google.com/", 1.,
                       0, 0, 1., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://static.google.com/",
                       1, 0, 0, 3., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://google.com/", 1, 0, 0,
                       2., false, true);
  EXPECT_EQ(mock_tables_->origin_table_.data_,
            OriginDataMap({{origin_data.host(), origin_data}}));
}

// Tests that a URL is deleted before another is added if the cache is full.
TEST_F(ResourcePrefetchPredictorTest, NavigationUrlNotInDBAndDBFull) {
  const int kVisitCount = 4;
  AddUrlToHistory("http://www.nike.com/", kVisitCount);

  mock_tables_->url_resource_table_.data_ = test_url_data_;
  mock_tables_->host_resource_table_.data_ = test_host_data_;
  mock_tables_->origin_table_.data_ = test_origin_data_;

  ResetPredictor();
  InitializePredictor();

  URLRequestSummary main_frame = CreateURLRequestSummary(
      1, "http://www.nike.com", "http://www.nike.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);

  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, "http://www.nike.com", "http://nike.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false);
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, "http://www.nike.com", "http://nike.com/image2.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false);

  auto page_summary = CreatePageRequestSummary(
      "http://www.nike.com", "http://www.nike.com", {resource1, resource2});

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(kVisitCount, page_summary));

  predictor_->RecordPageRequestSummary(
      base::MakeUnique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  PrefetchData url_data = CreatePrefetchData("http://www.nike.com/");
  InitializeResourceData(url_data.add_resources(), "http://nike.com/style1.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 1.0,
                         net::MEDIUM, false, false);
  InitializeResourceData(url_data.add_resources(), "http://nike.com/image2.png",
                         content::RESOURCE_TYPE_IMAGE, 1, 0, 0, 2.0,
                         net::MEDIUM, false, false);
  PrefetchDataMap expected_url_resource_data = test_url_data_;
  expected_url_resource_data.erase("http://www.google.com/");
  expected_url_resource_data[url_data.primary_key()] = url_data;
  EXPECT_EQ(mock_tables_->url_resource_table_.data_,
            expected_url_resource_data);

  PrefetchData host_data = CreatePrefetchData("www.nike.com");
  host_data.mutable_resources()->CopyFrom(url_data.resources());
  PrefetchDataMap expected_host_resource_data = test_host_data_;
  expected_host_resource_data.erase("www.facebook.com");
  expected_host_resource_data[host_data.primary_key()] = host_data;
  EXPECT_EQ(mock_tables_->host_resource_table_.data_,
            expected_host_resource_data);

  RedirectData url_redirect_data = CreateRedirectData("http://www.nike.com/");
  InitializeRedirectStat(url_redirect_data.add_redirect_endpoints(),
                         "http://www.nike.com/", 1, 0, 0);
  EXPECT_EQ(
      mock_tables_->url_redirect_table_.data_,
      RedirectDataMap({{url_redirect_data.primary_key(), url_redirect_data}}));

  RedirectData host_redirect_data = CreateRedirectData("www.nike.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "www.nike.com", 1, 0, 0);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
            RedirectDataMap(
                {{host_redirect_data.primary_key(), host_redirect_data}}));

  OriginData origin_data = CreateOriginData("www.nike.com");
  InitializeOriginStat(origin_data.add_origins(), "http://www.nike.com/", 1, 0,
                       0, 1., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://nike.com/", 1, 0, 0,
                       2., false, true);
  OriginDataMap expected_origin_data = test_origin_data_;
  expected_origin_data.erase("google.com");
  expected_origin_data["www.nike.com"] = origin_data;
  EXPECT_EQ(mock_tables_->origin_table_.data_, expected_origin_data);
}

TEST_F(ResourcePrefetchPredictorTest, RedirectUrlNotInDB) {
  const int kVisitCount = 4;
  AddUrlToHistory("https://facebook.com/google", kVisitCount);

  auto page_summary = CreatePageRequestSummary(
      "https://facebook.com/google", "http://fb.com/google",
      std::vector<URLRequestSummary>());

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(kVisitCount, page_summary));

  predictor_->RecordPageRequestSummary(
      base::MakeUnique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  RedirectData url_redirect_data = CreateRedirectData("http://fb.com/google");
  InitializeRedirectStat(url_redirect_data.add_redirect_endpoints(),
                         "https://facebook.com/google", 1, 0, 0);
  EXPECT_EQ(
      mock_tables_->url_redirect_table_.data_,
      RedirectDataMap({{url_redirect_data.primary_key(), url_redirect_data}}));

  RedirectData host_redirect_data = CreateRedirectData("fb.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "facebook.com", 1, 0, 0);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
            RedirectDataMap(
                {{host_redirect_data.primary_key(), host_redirect_data}}));
}

// Tests that redirect is recorded correctly for URL already present in
// the database cache.
TEST_F(ResourcePrefetchPredictorTest, RedirectUrlInDB) {
  const int kVisitCount = 7;
  AddUrlToHistory("https://facebook.com/google", kVisitCount);

  mock_tables_->url_redirect_table_.data_ = test_url_redirect_data_;
  mock_tables_->host_redirect_table_.data_ = test_host_redirect_data_;

  ResetPredictor();
  InitializePredictor();

  auto page_summary = CreatePageRequestSummary(
      "https://facebook.com/google", "http://fb.com/google",
      std::vector<URLRequestSummary>());

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer, OnNavigationLearned(kVisitCount, page_summary));

  predictor_->RecordPageRequestSummary(
      base::MakeUnique<PageRequestSummary>(page_summary));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  RedirectData url_redirect_data = CreateRedirectData("http://fb.com/google");
  InitializeRedirectStat(url_redirect_data.add_redirect_endpoints(),
                         "https://facebook.com/google", 6, 1, 0);
  // Existing redirect to https://facebook.com/login will be deleted because of
  // too many consecutive misses.
  RedirectDataMap expected_url_redirect_data = test_url_redirect_data_;
  expected_url_redirect_data[url_redirect_data.primary_key()] =
      url_redirect_data;
  EXPECT_EQ(mock_tables_->url_redirect_table_.data_,
            expected_url_redirect_data);

  RedirectData host_redirect_data = CreateRedirectData("fb.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "facebook.com", 1, 0, 0);
  RedirectDataMap expected_host_redirect_data = test_host_redirect_data_;
  expected_host_redirect_data.erase("bbc.com");
  expected_host_redirect_data[host_redirect_data.primary_key()] =
      host_redirect_data;
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_,
            expected_host_redirect_data);
}

TEST_F(ResourcePrefetchPredictorTest, DeleteUrls) {
  ResetPredictor(false);
  InitializePredictor();

  // Add some dummy entries to cache.

  PrefetchDataMap url_resources;
  url_resources.insert(
      {"http://www.google.com/page1.html",
       CreatePrefetchData("http://www.google.com/page1.html")});
  url_resources.insert(
      {"http://www.google.com/page2.html",
       CreatePrefetchData("http://www.google.com/page2.html")});
  url_resources.insert(
      {"http://www.yahoo.com/", CreatePrefetchData("http://www.yahoo.com/")});
  url_resources.insert(
      {"http://www.apple.com/", CreatePrefetchData("http://www.apple.com/")});
  url_resources.insert(
      {"http://www.nike.com/", CreatePrefetchData("http://www.nike.com/")});
  for (const auto& resource : url_resources)
    predictor_->url_resource_data_->UpdateData(resource.first, resource.second);

  PrefetchDataMap host_resources;
  host_resources.insert(
      {"www.google.com", CreatePrefetchData("www.google.com")});
  host_resources.insert({"www.yahoo.com", CreatePrefetchData("www.yahoo.com")});
  host_resources.insert({"www.apple.com", CreatePrefetchData("www.apple.com")});
  for (const auto& resource : host_resources) {
    predictor_->host_resource_data_->UpdateData(resource.first,
                                                resource.second);
  }

  RedirectDataMap url_redirects;
  url_redirects.insert(
      {"http://www.google.com/page1.html",
       CreateRedirectData("http://www.google.com/page1.html")});
  url_redirects.insert(
      {"http://www.google.com/page2.html",
       CreateRedirectData("http://www.google.com/page2.html")});
  url_redirects.insert(
      {"http://www.apple.com/", CreateRedirectData("http://www.apple.com/")});
  url_redirects.insert(
      {"http://nyt.com/", CreateRedirectData("http://nyt.com/")});
  for (const auto& redirect : url_redirects)
    predictor_->url_redirect_data_->UpdateData(redirect.first, redirect.second);

  RedirectDataMap host_redirects;
  host_redirects.insert(
      {"www.google.com", CreateRedirectData("www.google.com")});
  host_redirects.insert({"www.nike.com", CreateRedirectData("www.nike.com")});
  host_redirects.insert(
      {"www.wikipedia.org", CreateRedirectData("www.wikipedia.org")});
  for (const auto& redirect : host_redirects) {
    predictor_->host_redirect_data_->UpdateData(redirect.first,
                                                redirect.second);
  }

  // TODO(alexilin): Add origin data.

  history::URLRows rows;
  rows.push_back(history::URLRow(GURL("http://www.google.com/page2.html")));
  rows.push_back(history::URLRow(GURL("http://www.apple.com")));
  rows.push_back(history::URLRow(GURL("http://www.nike.com")));

  url_resources.erase("http://www.google.com/page2.html");
  url_resources.erase("http://www.apple.com/");
  url_resources.erase("http://www.nike.com/");
  host_resources.erase("www.google.com");
  host_resources.erase("www.apple.com");
  url_redirects.erase("http://www.google.com/page2.html");
  url_redirects.erase("http://www.apple.com/");
  host_redirects.erase("www.google.com");
  host_redirects.erase("www.nike.com");

  predictor_->DeleteUrls(rows);
  EXPECT_EQ(mock_tables_->url_resource_table_.data_, url_resources);
  EXPECT_EQ(mock_tables_->host_resource_table_.data_, host_resources);
  EXPECT_EQ(mock_tables_->url_redirect_table_.data_, url_redirects);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_, host_redirects);

  predictor_->DeleteAllUrls();
  EXPECT_TRUE(mock_tables_->url_resource_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->host_resource_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->url_redirect_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->host_redirect_table_.data_.empty());
}

TEST_F(ResourcePrefetchPredictorTest, PopulatePrefetcherRequest) {
  // The data that will be used in populating.
  PrefetchData google = CreatePrefetchData("http://www.google.com/", 1);
  InitializeResourceData(google.add_resources(), "http://google.com/image1.png",
                         content::RESOURCE_TYPE_IMAGE, 10, 0, 0, 2.2,
                         net::MEDIUM, false, false);  // good
  InitializeResourceData(google.add_resources(), "http://google.com/style1.css",
                         content::RESOURCE_TYPE_STYLESHEET, 2, 2, 1, 1.0,
                         net::MEDIUM, false, false);  // still good
  InitializeResourceData(google.add_resources(), "http://google.com/script3.js",
                         content::RESOURCE_TYPE_SCRIPT, 1, 0, 1, 2.1,
                         net::MEDIUM, false, false);  // bad - not enough hits
  InitializeResourceData(
      google.add_resources(), "http://google.com/script4.js",
      content::RESOURCE_TYPE_SCRIPT, 4, 5, 0, 2.1, net::MEDIUM, false,
      false);  // bad - more misses than hits (min_confidence = 0.5)

  // The data to be sure that other PrefetchData won't be affected.
  PrefetchData twitter = CreatePrefetchData("http://twitter.com", 2);
  InitializeResourceData(
      twitter.add_resources(), "http://twitter.com/image.jpg",
      content::RESOURCE_TYPE_IMAGE, 10, 0, 0, 1.0, net::MEDIUM, false, false);

  // The data to check negative result.
  PrefetchData nyt = CreatePrefetchData("http://nyt.com", 3);
  InitializeResourceData(nyt.add_resources(), "http://nyt.com/old_script.js",
                         content::RESOURCE_TYPE_SCRIPT, 5, 7, 7, 1.0,
                         net::MEDIUM, false, false);

  auto& resource_data = *predictor_->url_resource_data_;
  resource_data.UpdateData(google.primary_key(), google);
  resource_data.UpdateData(twitter.primary_key(), twitter);
  resource_data.UpdateData(nyt.primary_key(), nyt);

  std::vector<GURL> urls;
  EXPECT_TRUE(predictor_->PopulatePrefetcherRequest(google.primary_key(),
                                                    resource_data, &urls));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL("http://google.com/image1.png"),
                                         GURL("http://google.com/style1.css")));

  urls.clear();
  EXPECT_FALSE(predictor_->PopulatePrefetcherRequest(nyt.primary_key(),
                                                     resource_data, &urls));
  EXPECT_TRUE(urls.empty());

  urls.clear();
  EXPECT_FALSE(predictor_->PopulatePrefetcherRequest("http://404.com",
                                                     resource_data, &urls));
  EXPECT_TRUE(urls.empty());
}

TEST_F(ResourcePrefetchPredictorTest, GetRedirectEndpoint) {
  // The data to be requested for the confident endpoint.
  RedirectData nyt = CreateRedirectData("http://nyt.com", 1);
  InitializeRedirectStat(nyt.add_redirect_endpoints(),
                         "https://mobile.nytimes.com", 10, 0, 0);

  // The data to check negative result due not enough confidence.
  RedirectData facebook = CreateRedirectData("http://fb.com", 3);
  InitializeRedirectStat(facebook.add_redirect_endpoints(),
                         "http://facebook.com", 5, 5, 0);

  // The data to check negative result due ambiguity.
  RedirectData google = CreateRedirectData("http://google.com", 4);
  InitializeRedirectStat(google.add_redirect_endpoints(), "https://google.com",
                         10, 0, 0);
  InitializeRedirectStat(google.add_redirect_endpoints(), "https://google.fr",
                         10, 1, 0);
  InitializeRedirectStat(google.add_redirect_endpoints(), "https://google.ws",
                         20, 20, 0);

  auto& redirect_data = *predictor_->url_redirect_data_;
  redirect_data.UpdateData(nyt.primary_key(), nyt);
  redirect_data.UpdateData(facebook.primary_key(), facebook);
  redirect_data.UpdateData(google.primary_key(), google);

  std::string redirect_endpoint;
  EXPECT_TRUE(predictor_->GetRedirectEndpoint("http://nyt.com", redirect_data,
                                              &redirect_endpoint));
  EXPECT_EQ(redirect_endpoint, "https://mobile.nytimes.com");

  // Returns the initial url if data_map doesn't contain an entry for the url.
  EXPECT_TRUE(predictor_->GetRedirectEndpoint("http://bbc.com", redirect_data,
                                              &redirect_endpoint));
  EXPECT_EQ(redirect_endpoint, "http://bbc.com");

  EXPECT_FALSE(predictor_->GetRedirectEndpoint("http://fb.com", redirect_data,
                                               &redirect_endpoint));
  EXPECT_FALSE(predictor_->GetRedirectEndpoint(
      "http://google.com", redirect_data, &redirect_endpoint));
}

TEST_F(ResourcePrefetchPredictorTest, GetPrefetchData) {
  const GURL main_frame_url("http://google.com/?query=cats");
  ResourcePrefetchPredictor::Prediction prediction;
  std::vector<GURL>& urls = prediction.subresource_urls;
  // No prefetch data.
  EXPECT_FALSE(predictor_->IsUrlPrefetchable(main_frame_url));
  EXPECT_FALSE(predictor_->GetPrefetchData(main_frame_url, &prediction));

  // Add a resource associated with the main frame host.
  PrefetchData google_host = CreatePrefetchData("google.com", 2);
  const std::string script_url = "https://cdn.google.com/script.js";
  InitializeResourceData(google_host.add_resources(), script_url,
                         content::RESOURCE_TYPE_SCRIPT, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->host_resource_data_->UpdateData(google_host.primary_key(),
                                              google_host);

  urls.clear();
  EXPECT_TRUE(predictor_->IsUrlPrefetchable(main_frame_url));
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &prediction));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(script_url)));

  // Add host-based redirect.
  RedirectData host_redirect = CreateRedirectData("google.com", 3);
  InitializeRedirectStat(host_redirect.add_redirect_endpoints(),
                         "www.google.fr", 10, 0, 0);
  predictor_->host_redirect_data_->UpdateData(host_redirect.primary_key(),
                                              host_redirect);

  // Prediction failed: no data associated with the host redirect endpoint.
  urls.clear();
  EXPECT_FALSE(predictor_->IsUrlPrefetchable(main_frame_url));
  EXPECT_FALSE(predictor_->GetPrefetchData(main_frame_url, &prediction));

  // Add a resource associated with host redirect endpoint.
  PrefetchData www_google_host = CreatePrefetchData("www.google.fr", 4);
  const std::string style_url = "https://cdn.google.com/style.css";
  InitializeResourceData(www_google_host.add_resources(), style_url,
                         content::RESOURCE_TYPE_STYLESHEET, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->host_resource_data_->UpdateData(www_google_host.primary_key(),
                                              www_google_host);

  urls.clear();
  EXPECT_TRUE(predictor_->IsUrlPrefetchable(main_frame_url));
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &prediction));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(style_url)));

  // Add a resource associated with the main frame url.
  PrefetchData google_url =
      CreatePrefetchData("http://google.com/?query=cats", 5);
  const std::string image_url = "https://cdn.google.com/image.png";
  InitializeResourceData(google_url.add_resources(), image_url,
                         content::RESOURCE_TYPE_IMAGE, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->url_resource_data_->UpdateData(google_url.primary_key(),
                                             google_url);

  urls.clear();
  EXPECT_TRUE(predictor_->IsUrlPrefetchable(main_frame_url));
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &prediction));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(image_url)));

  // Add url-based redirect.
  RedirectData url_redirect =
      CreateRedirectData("http://google.com/?query=cats", 6);
  InitializeRedirectStat(url_redirect.add_redirect_endpoints(),
                         "https://www.google.com/?query=cats", 10, 0, 0);
  predictor_->url_redirect_data_->UpdateData(url_redirect.primary_key(),
                                             url_redirect);

  // Url redirect endpoint doesn't have associated resources so we get
  // host-based data.
  urls.clear();
  EXPECT_TRUE(predictor_->IsUrlPrefetchable(main_frame_url));
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &prediction));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(style_url)));

  // Add a resource associated with url redirect endpoint.
  PrefetchData www_google_url =
      CreatePrefetchData("https://www.google.com/?query=cats", 7);
  const std::string font_url = "https://cdn.google.com/comic-sans-ms.woff";
  InitializeResourceData(www_google_url.add_resources(), font_url,
                         content::RESOURCE_TYPE_FONT_RESOURCE, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->url_resource_data_->UpdateData(www_google_url.primary_key(),
                                             www_google_url);

  urls.clear();
  EXPECT_TRUE(predictor_->IsUrlPrefetchable(main_frame_url));
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &prediction));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(font_url)));
}

TEST_F(ResourcePrefetchPredictorTest, TestPredictPreconnectOrigins) {
  const GURL main_frame_url("http://google.com/?query=cats");
  auto prediction = base::MakeUnique<PreconnectPrediction>();
  // No prefetch data.
  EXPECT_FALSE(predictor_->IsUrlPreconnectable(main_frame_url));
  EXPECT_FALSE(
      predictor_->PredictPreconnectOrigins(main_frame_url, prediction.get()));

  const char* cdn_origin = "https://cdn%d.google.com";
  auto gen_origin = [cdn_origin](int n) {
    return base::StringPrintf(cdn_origin, n);
  };

  // Add origins associated with the main frame host.
  OriginData google = CreateOriginData("google.com");
  InitializeOriginStat(google.add_origins(), gen_origin(1), 10, 0, 0, 1.0, true,
                       true);  // High confidence - preconnect.
  InitializeOriginStat(google.add_origins(), gen_origin(2), 10, 5, 0, 2.0, true,
                       true);  // Medium confidence - preresolve.
  InitializeOriginStat(google.add_origins(), gen_origin(3), 1, 10, 10, 3.0,
                       true, true);  // Low confidence - ignore.
  predictor_->origin_data_->UpdateData(google.host(), google);

  prediction = base::MakeUnique<PreconnectPrediction>();
  EXPECT_TRUE(predictor_->IsUrlPreconnectable(main_frame_url));
  EXPECT_TRUE(
      predictor_->PredictPreconnectOrigins(main_frame_url, prediction.get()));
  EXPECT_EQ(*prediction, CreatePreconnectPrediction("google.com", false,
                                                    {GURL(gen_origin(1))},
                                                    {GURL(gen_origin(2))}));

  // Add a redirect.
  RedirectData redirect = CreateRedirectData("google.com", 3);
  InitializeRedirectStat(redirect.add_redirect_endpoints(), "www.google.com",
                         10, 0, 0);
  predictor_->host_redirect_data_->UpdateData(redirect.primary_key(), redirect);

  // Prediction failed: no data associated with the redirect endpoint.
  prediction = base::MakeUnique<PreconnectPrediction>();
  EXPECT_FALSE(predictor_->IsUrlPreconnectable(main_frame_url));
  EXPECT_FALSE(
      predictor_->PredictPreconnectOrigins(main_frame_url, prediction.get()));

  // Add a resource associated with the redirect endpoint.
  OriginData www_google = CreateOriginData("www.google.com", 4);
  InitializeOriginStat(www_google.add_origins(), gen_origin(4), 10, 0, 0, 1.0,
                       true,
                       true);  // High confidence - preconnect.
  predictor_->origin_data_->UpdateData(www_google.host(), www_google);

  prediction = base::MakeUnique<PreconnectPrediction>();
  EXPECT_TRUE(predictor_->IsUrlPreconnectable(main_frame_url));
  EXPECT_TRUE(
      predictor_->PredictPreconnectOrigins(main_frame_url, prediction.get()));
  EXPECT_EQ(*prediction, CreatePreconnectPrediction("www.google.com", true,
                                                    {GURL(gen_origin(4))},
                                                    std::vector<GURL>()));
}

}  // namespace predictors
