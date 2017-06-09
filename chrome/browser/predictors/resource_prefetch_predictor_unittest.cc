// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor.h"

#include <iostream>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/browser/predictors/resource_prefetcher_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;
using testing::UnorderedElementsAre;

namespace predictors {

using URLRequestSummary = ResourcePrefetchPredictor::URLRequestSummary;
using PageRequestSummary = ResourcePrefetchPredictor::PageRequestSummary;
using PrefetchDataMap = std::map<std::string, PrefetchData>;
using RedirectDataMap = std::map<std::string, RedirectData>;
using OriginDataMap = std::map<std::string, OriginData>;
using ManifestDataMap = std::map<std::string, precache::PrecacheManifest>;

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
  MockResourcePrefetchPredictorTables() = default;

  void ScheduleDBTask(const tracked_objects::Location& from_here,
                      DBTask task) override {
    ExecuteDBTaskOnDBThread(std::move(task));
  }

  void ExecuteDBTaskOnDBThread(DBTask task) override {
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

  GlowplugKeyValueTable<precache::PrecacheManifest>* manifest_table() override {
    return &manifest_table_;
  }

  GlowplugKeyValueTable<OriginData>* origin_table() override {
    return &origin_table_;
  }

  FakeGlowplugKeyValueTable<PrefetchData> url_resource_table_;
  FakeGlowplugKeyValueTable<RedirectData> url_redirect_table_;
  FakeGlowplugKeyValueTable<PrefetchData> host_resource_table_;
  FakeGlowplugKeyValueTable<RedirectData> host_redirect_table_;
  FakeGlowplugKeyValueTable<precache::PrecacheManifest> manifest_table_;
  FakeGlowplugKeyValueTable<OriginData> origin_table_;

 protected:
  ~MockResourcePrefetchPredictorTables() override = default;
};

class MockResourcePrefetchPredictorObserver : public TestObserver {
 public:
  explicit MockResourcePrefetchPredictorObserver(
      ResourcePrefetchPredictor* predictor)
      : TestObserver(predictor) {}

  MOCK_METHOD2(
      OnNavigationLearned,
      void(size_t url_visit_count,
           const ResourcePrefetchPredictor::PageRequestSummary& summary));
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

  URLRequestSummary CreateRedirectRequestSummary(
      SessionID::id_type session_id,
      const std::string& main_frame_url,
      const std::string& redirect_url) {
    URLRequestSummary summary =
        CreateURLRequestSummary(session_id, main_frame_url);
    summary.redirect_url = GURL(redirect_url);
    return summary;
  }

  void InitializePredictor() {
    loading_predictor_->StartInitialization();
    base::RunLoop loop;
    loop.RunUntilIdle();  // Runs the DB lookup.
    profile_->BlockUntilHistoryProcessesPendingRequests();
  }

  void ResetPredictor(bool small_db = true) {
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
  net::TestURLRequestContext url_request_context_;

  std::unique_ptr<LoadingPredictor> loading_predictor_;
  ResourcePrefetchPredictor* predictor_;
  scoped_refptr<StrictMock<MockResourcePrefetchPredictorTables>> mock_tables_;

  PrefetchDataMap test_url_data_;
  PrefetchDataMap test_host_data_;
  RedirectDataMap test_url_redirect_data_;
  RedirectDataMap test_host_redirect_data_;
  ManifestDataMap test_manifest_data_;
  OriginDataMap test_origin_data_;

  MockURLRequestJobFactory url_request_job_factory_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

ResourcePrefetchPredictorTest::ResourcePrefetchPredictorTest()
    : profile_(new TestingProfile()),
      mock_tables_(new StrictMock<MockResourcePrefetchPredictorTables>()) {}

ResourcePrefetchPredictorTest::~ResourcePrefetchPredictorTest() {
  profile_.reset(NULL);
  base::RunLoop().RunUntilIdle();
}

void ResourcePrefetchPredictorTest::SetUp() {
  InitializeSampleData();

  ASSERT_TRUE(profile_->CreateHistoryService(true, false));
  profile_->BlockUntilHistoryProcessesPendingRequests();
  EXPECT_TRUE(HistoryServiceFactory::GetForProfile(
      profile_.get(), ServiceAccessType::EXPLICIT_ACCESS));
  // Initialize the predictor with empty data.
  ResetPredictor();
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::NOT_INITIALIZED);
  InitializePredictor();
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::INITIALIZED);

  url_request_job_factory_.Reset();
  url_request_context_.set_job_factory(&url_request_job_factory_);

  histogram_tester_.reset(new base::HistogramTester());
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
  EXPECT_EQ(*predictor_->manifest_data_->data_cache_,
            mock_tables_->manifest_table_.data_);
  EXPECT_EQ(*predictor_->origin_data_->data_cache_,
            mock_tables_->origin_table_.data_);
  loading_predictor_ = nullptr;
  predictor_ = nullptr;
  profile_->DestroyHistoryService();
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

  {  // Manifest data.
    precache::PrecacheManifest google = CreateManifestData(11);
    InitializePrecacheResource(
        google.add_resource(), "http://google.com/script.js", 0.5,
        precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);
    InitializePrecacheResource(
        google.add_resource(), "http://static.google.com/style.css", 0.333,
        precache::PrecacheResource::RESOURCE_TYPE_STYLESHEET);

    precache::PrecacheManifest facebook = CreateManifestData(12);
    InitializePrecacheResource(
        facebook.add_resource(), "http://fb.com/static.css", 0.99,
        precache::PrecacheResource::RESOURCE_TYPE_STYLESHEET);

    test_manifest_data_.insert(std::make_pair("google.com", google));
    test_manifest_data_.insert(std::make_pair("facebook.com", facebook));
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
  EXPECT_TRUE(mock_tables_->manifest_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->origin_table_.data_.empty());
}

// Tests that the history and the db tables data are loaded correctly.
TEST_F(ResourcePrefetchPredictorTest, LazilyInitializeWithData) {
  mock_tables_->url_resource_table_.data_ = test_url_data_;
  mock_tables_->url_redirect_table_.data_ = test_url_redirect_data_;
  mock_tables_->host_resource_table_.data_ = test_host_data_;
  mock_tables_->host_redirect_table_.data_ = test_host_redirect_data_;
  mock_tables_->manifest_table_.data_ = test_manifest_data_;
  mock_tables_->origin_table_.data_ = test_origin_data_;

  ResetPredictor();
  InitializePredictor();

  // Test that the internal variables correctly initialized.
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::INITIALIZED);
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());

  // Integrity of the cache and the backend storage is checked on TearDown.
}

// Single navigation but history count is low, so should not record url data.
TEST_F(ResourcePrefetchPredictorTest, NavigationLowHistoryCount) {
  const int kVisitCount = 1;
  AddUrlToHistory("https://www.google.com", kVisitCount);

  URLRequestSummary main_frame =
      CreateURLRequestSummary(1, "http://www.google.com");
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  URLRequestSummary main_frame_redirect = CreateRedirectRequestSummary(
      1, "http://www.google.com", "https://www.google.com");
  predictor_->RecordURLRedirect(main_frame_redirect);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());
  main_frame = CreateURLRequestSummary(1, "https://www.google.com");

  // Now add a few subresources.
  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, "https://www.google.com", "https://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false);
  predictor_->RecordURLResponse(resource1);
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, "https://www.google.com", "https://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  predictor_->RecordURLResponse(resource2);
  URLRequestSummary resource3 = CreateURLRequestSummary(
      1, "https://www.google.com", "https://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  predictor_->RecordURLResponse(resource3);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(
      mock_observer,
      OnNavigationLearned(kVisitCount,
                          CreatePageRequestSummary(
                              "https://www.google.com", "http://www.google.com",
                              {resource1, resource2, resource3})));

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
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
  InitializeOriginStat(origin_data.add_origins(), "https://google.com/", 1, 0,
                       0, 1., false, true);
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
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  std::vector<URLRequestSummary> resources;
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", true));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/image1.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/image2.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style2.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true));
  predictor_->RecordURLResponse(resources.back());

  auto no_store = CreateURLRequestSummary(
      1, "http://www.google.com",
      "http://static.google.com/style2-no-store.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true);
  no_store.is_no_store = true;
  predictor_->RecordURLResponse(no_store);

  auto redirected = CreateURLRequestSummary(
      1, "http://www.google.com", "http://reader.google.com/style.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true);
  redirected.redirect_url = GURL("http://dev.null.google.com/style.css");

  predictor_->RecordURLRedirect(redirected);
  redirected.is_no_store = true;
  redirected.request_url = redirected.redirect_url;
  redirected.redirect_url = GURL();

  predictor_->RecordURLResponse(redirected);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer,
              OnNavigationLearned(
                  kVisitCount, CreatePageRequestSummary("http://www.google.com",
                                                        "http://www.google.com",
                                                        resources)));

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
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
  InitializeOriginStat(origin_data.add_origins(), "http://static.google.com/",
                       1, 0, 0, 2., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://dev.null.google.com/",
                       1, 0, 0, 4., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://google.com/", 1, 0, 0,
                       1., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://reader.google.com/",
                       1, 0, 0, 3., false, true);
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
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  std::vector<URLRequestSummary> resources;
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", true));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/image1.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/image2.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style2.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true));
  predictor_->RecordURLResponse(resources.back());
  auto no_store = CreateURLRequestSummary(
      1, "http://www.google.com",
      "http://static.google.com/style2-no-store.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true);
  no_store.is_no_store = true;
  predictor_->RecordURLResponse(no_store);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer,
              OnNavigationLearned(
                  kVisitCount, CreatePageRequestSummary("http://www.google.com",
                                                        "http://www.google.com",
                                                        resources)));

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
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
  InitializeOriginStat(origin_data.add_origins(), "http://static.google.com/",
                       1, 0, 0, 2., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://google.com/", 1, 0, 0,
                       1., false, true);
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
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, "http://www.nike.com", "http://nike.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false);
  predictor_->RecordURLResponse(resource1);
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, "http://www.nike.com", "http://nike.com/image2.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false);
  predictor_->RecordURLResponse(resource2);

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(mock_observer,
              OnNavigationLearned(
                  kVisitCount, CreatePageRequestSummary(
                                   "http://www.nike.com", "http://www.nike.com",
                                   {resource1, resource2})));

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
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
  InitializeOriginStat(origin_data.add_origins(), "http://nike.com/", 1, 0, 0,
                       1., false, true);
  OriginDataMap expected_origin_data = test_origin_data_;
  expected_origin_data.erase("google.com");
  expected_origin_data["www.nike.com"] = origin_data;
  EXPECT_EQ(mock_tables_->origin_table_.data_, expected_origin_data);
}

TEST_F(ResourcePrefetchPredictorTest, RedirectUrlNotInDB) {
  const int kVisitCount = 4;
  AddUrlToHistory("https://facebook.com/google", kVisitCount);

  URLRequestSummary fb1 = CreateURLRequestSummary(1, "http://fb.com/google");
  predictor_->RecordURLRequest(fb1);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  URLRequestSummary fb2 = CreateRedirectRequestSummary(
      1, "http://fb.com/google", "http://facebook.com/google");
  predictor_->RecordURLRedirect(fb2);
  URLRequestSummary fb3 = CreateRedirectRequestSummary(
      1, "http://facebook.com/google", "https://facebook.com/google");
  predictor_->RecordURLRedirect(fb3);
  NavigationID fb_end = CreateNavigationID(1, "https://facebook.com/google");

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(
      mock_observer,
      OnNavigationLearned(kVisitCount, CreatePageRequestSummary(
                                           "https://facebook.com/google",
                                           "http://fb.com/google",
                                           std::vector<URLRequestSummary>())));

  predictor_->RecordMainFrameLoadComplete(fb_end);
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

  URLRequestSummary fb1 = CreateURLRequestSummary(1, "http://fb.com/google");
  predictor_->RecordURLRequest(fb1);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  URLRequestSummary fb2 = CreateRedirectRequestSummary(
      1, "http://fb.com/google", "http://facebook.com/google");
  predictor_->RecordURLRedirect(fb2);
  URLRequestSummary fb3 = CreateRedirectRequestSummary(
      1, "http://facebook.com/google", "https://facebook.com/google");
  predictor_->RecordURLRedirect(fb3);
  NavigationID fb_end = CreateNavigationID(1, "https://facebook.com/google");

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(predictor_);
  EXPECT_CALL(
      mock_observer,
      OnNavigationLearned(kVisitCount, CreatePageRequestSummary(
                                           "https://facebook.com/google",
                                           "http://fb.com/google",
                                           std::vector<URLRequestSummary>())));

  predictor_->RecordMainFrameLoadComplete(fb_end);
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

TEST_F(ResourcePrefetchPredictorTest, ManifestHostNotInDB) {
  precache::PrecacheManifest manifest =
      CreateManifestData(base::Time::Now().ToDoubleT());
  InitializePrecacheResource(manifest.add_resource(),
                             "http://cdn.google.com/script.js", 0.9,
                             precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);
  InitializePrecacheResource(
      manifest.add_resource(), "http://cdn.google.com/style.css", 0.75,
      precache::PrecacheResource::RESOURCE_TYPE_STYLESHEET);

  predictor_->OnManifestFetched("google.com", manifest);

  EXPECT_EQ(mock_tables_->manifest_table_.data_,
            ManifestDataMap({{"google.com", manifest}}));
}

TEST_F(ResourcePrefetchPredictorTest, ManifestHostInDB) {
  mock_tables_->manifest_table_.data_ = test_manifest_data_;
  ResetPredictor();
  InitializePredictor();

  precache::PrecacheManifest manifest =
      CreateManifestData(base::Time::Now().ToDoubleT());
  InitializePrecacheResource(manifest.add_resource(),
                             "http://google.com/image.jpg", 0.1,
                             precache::PrecacheResource::RESOURCE_TYPE_IMAGE);

  predictor_->OnManifestFetched("google.com", manifest);

  ManifestDataMap expected_data = test_manifest_data_;
  expected_data["google.com"] = manifest;
  EXPECT_EQ(mock_tables_->manifest_table_.data_, expected_data);
}

TEST_F(ResourcePrefetchPredictorTest, ManifestHostNotInDBAndDBFull) {
  mock_tables_->manifest_table_.data_ = test_manifest_data_;
  ResetPredictor();
  InitializePredictor();

  precache::PrecacheManifest manifest =
      CreateManifestData(base::Time::Now().ToDoubleT());
  InitializePrecacheResource(manifest.add_resource(),
                             "http://en.wikipedia.org/logo.png", 1.0,
                             precache::PrecacheResource::RESOURCE_TYPE_IMAGE);

  predictor_->OnManifestFetched("en.wikipedia.org", manifest);

  ManifestDataMap expected_data = test_manifest_data_;
  expected_data.erase("google.com");
  expected_data["en.wikipedia.org"] = manifest;
  EXPECT_EQ(mock_tables_->manifest_table_.data_, expected_data);
}

TEST_F(ResourcePrefetchPredictorTest, ManifestUnknownFieldsRemoved) {
  precache::PrecacheManifest manifest =
      CreateManifestData(base::Time::Now().ToDoubleT());
  InitializePrecacheResource(manifest.add_resource(),
                             "http://cdn.google.com/script.js", 0.9,
                             precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);
  InitializePrecacheResource(
      manifest.add_resource(), "http://cdn.google.com/style.css", 0.75,
      precache::PrecacheResource::RESOURCE_TYPE_STYLESHEET);

  precache::PrecacheManifest manifest_with_unknown_fields(manifest);
  manifest_with_unknown_fields.mutable_id()->mutable_unknown_fields()->append(
      "DATA");
  manifest_with_unknown_fields.mutable_unknown_fields()->append("DATA");
  for (auto& resource : *manifest_with_unknown_fields.mutable_resource()) {
    resource.mutable_unknown_fields()->append("DATA");
  }

  predictor_->OnManifestFetched("google.com", manifest_with_unknown_fields);

  EXPECT_EQ(mock_tables_->manifest_table_.data_["google.com"].ByteSize(),
            manifest.ByteSize());
}

TEST_F(ResourcePrefetchPredictorTest, ManifestTooOld) {
  base::Time old_time = base::Time::Now() - base::TimeDelta::FromDays(7);
  precache::PrecacheManifest manifest =
      CreateManifestData(old_time.ToDoubleT());
  InitializePrecacheResource(manifest.add_resource(),
                             "http://cdn.google.com/script.js", 0.9,
                             precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);
  InitializePrecacheResource(
      manifest.add_resource(), "http://cdn.google.com/style.css", 0.75,
      precache::PrecacheResource::RESOURCE_TYPE_STYLESHEET);

  predictor_->OnManifestFetched("google.com", manifest);

  EXPECT_TRUE(mock_tables_->manifest_table_.data_.empty());
}

TEST_F(ResourcePrefetchPredictorTest, ManifestUnusedRemoved) {
  const std::string& script_url = "http://cdn.google.com/script.js";
  const std::string& style_url = "http://cdn.google.com/style.css";
  PrefetchData google = CreatePrefetchData("www.google.com");
  InitializeResourceData(google.add_resources(), script_url,
                         content::RESOURCE_TYPE_SCRIPT, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  InitializeResourceData(google.add_resources(), style_url,
                         content::RESOURCE_TYPE_SCRIPT, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->host_resource_data_->UpdateData(google.primary_key(), google);

  precache::PrecacheManifest manifest =
      CreateManifestData(base::Time::Now().ToDoubleT());
  InitializePrecacheResource(manifest.add_resource(), script_url, 0.9,
                             precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);
  InitializePrecacheResource(
      manifest.add_resource(), style_url, 0.75,
      precache::PrecacheResource::RESOURCE_TYPE_STYLESHEET);
  InitializeExperiment(&manifest, internal::kUnusedRemovedExperiment,
                       {true, false});

  predictor_->OnManifestFetched("google.com", manifest);

  // style_url should be removed.
  google.mutable_resources()->RemoveLast();
  EXPECT_EQ(mock_tables_->host_resource_table_.data_,
            PrefetchDataMap({{google.primary_key(), google}}));
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

  ManifestDataMap manifests;
  manifests.insert({"google.com", CreateManifestData()});
  manifests.insert({"apple.com", CreateManifestData()});
  manifests.insert({"en.wikipedia.org", CreateManifestData()});
  for (const auto& manifest : manifests)
    predictor_->manifest_data_->UpdateData(manifest.first, manifest.second);

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
  manifests.erase("google.com");
  manifests.erase("apple.com");

  predictor_->DeleteUrls(rows);
  EXPECT_EQ(mock_tables_->url_resource_table_.data_, url_resources);
  EXPECT_EQ(mock_tables_->host_resource_table_.data_, host_resources);
  EXPECT_EQ(mock_tables_->url_redirect_table_.data_, url_redirects);
  EXPECT_EQ(mock_tables_->host_redirect_table_.data_, host_redirects);
  EXPECT_EQ(mock_tables_->manifest_table_.data_, manifests);

  predictor_->DeleteAllUrls();
  EXPECT_TRUE(mock_tables_->url_resource_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->host_resource_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->url_redirect_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->host_redirect_table_.data_.empty());
  EXPECT_TRUE(mock_tables_->manifest_table_.data_.empty());
}

TEST_F(ResourcePrefetchPredictorTest, OnMainFrameRequest) {
  URLRequestSummary summary1 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  URLRequestSummary summary2 = CreateURLRequestSummary(
      2, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  URLRequestSummary summary3 = CreateURLRequestSummary(
      3, "http://www.yahoo.com", "http://www.yahoo.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);

  predictor_->OnMainFrameRequest(summary1);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());
  predictor_->OnMainFrameRequest(summary2);
  EXPECT_EQ(2U, predictor_->inflight_navigations_.size());
  predictor_->OnMainFrameRequest(summary3);
  EXPECT_EQ(3U, predictor_->inflight_navigations_.size());

  // Insert another with same navigation id. It should replace.
  URLRequestSummary summary4 = CreateURLRequestSummary(
      1, "http://www.nike.com", "http://www.nike.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  URLRequestSummary summary5 = CreateURLRequestSummary(
      2, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);

  predictor_->OnMainFrameRequest(summary4);
  EXPECT_EQ(3U, predictor_->inflight_navigations_.size());

  // Change this creation time so that it will go away on the next insert.
  summary5.navigation_id.creation_time = base::TimeTicks::Now() -
      base::TimeDelta::FromDays(1);
  predictor_->OnMainFrameRequest(summary5);
  EXPECT_EQ(3U, predictor_->inflight_navigations_.size());

  URLRequestSummary summary6 = CreateURLRequestSummary(
      4, "http://www.shoes.com", "http://www.shoes.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  predictor_->OnMainFrameRequest(summary6);
  EXPECT_EQ(3U, predictor_->inflight_navigations_.size());

  EXPECT_TRUE(predictor_->inflight_navigations_.find(summary3.navigation_id) !=
              predictor_->inflight_navigations_.end());
  EXPECT_TRUE(predictor_->inflight_navigations_.find(summary4.navigation_id) !=
              predictor_->inflight_navigations_.end());
  EXPECT_TRUE(predictor_->inflight_navigations_.find(summary6.navigation_id) !=
              predictor_->inflight_navigations_.end());
}

TEST_F(ResourcePrefetchPredictorTest, OnMainFrameRedirect) {
  URLRequestSummary yahoo = CreateURLRequestSummary(1, "http://yahoo.com");

  URLRequestSummary bbc1 = CreateURLRequestSummary(2, "http://bbc.com");
  URLRequestSummary bbc2 =
      CreateRedirectRequestSummary(2, "http://bbc.com", "https://www.bbc.com");
  NavigationID bbc_end = CreateNavigationID(2, "https://www.bbc.com");

  URLRequestSummary youtube1 = CreateURLRequestSummary(3, "http://youtube.com");
  URLRequestSummary youtube2 = CreateRedirectRequestSummary(
      3, "http://youtube.com", "https://youtube.com");
  NavigationID youtube_end = CreateNavigationID(3, "https://youtube.com");

  URLRequestSummary nyt1 = CreateURLRequestSummary(4, "http://nyt.com");
  URLRequestSummary nyt2 =
      CreateRedirectRequestSummary(4, "http://nyt.com", "http://nytimes.com");
  URLRequestSummary nyt3 = CreateRedirectRequestSummary(4, "http://nytimes.com",
                                                        "http://m.nytimes.com");
  NavigationID nyt_end = CreateNavigationID(4, "http://m.nytimes.com");

  URLRequestSummary fb1 = CreateURLRequestSummary(5, "http://fb.com");
  URLRequestSummary fb2 =
      CreateRedirectRequestSummary(5, "http://fb.com", "http://facebook.com");
  URLRequestSummary fb3 = CreateRedirectRequestSummary(5, "http://facebook.com",
                                                       "https://facebook.com");
  URLRequestSummary fb4 = CreateRedirectRequestSummary(
      5, "https://facebook.com",
      "https://m.facebook.com/?refsrc=https%3A%2F%2Fwww.facebook.com%2F&_rdr");
  NavigationID fb_end = CreateNavigationID(
      5,
      "https://m.facebook.com/?refsrc=https%3A%2F%2Fwww.facebook.com%2F&_rdr");

  // Redirect with empty redirect_url will be deleted.
  predictor_->OnMainFrameRequest(yahoo);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());
  predictor_->OnMainFrameRedirect(yahoo);
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());

  // Redirect without previous request works fine.
  // predictor_->OnMainFrameRequest(bbc1) missing.
  predictor_->OnMainFrameRedirect(bbc2);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());
  EXPECT_EQ(bbc1.navigation_id.main_frame_url,
            predictor_->inflight_navigations_[bbc_end]->initial_url);

  // http://youtube.com -> https://youtube.com.
  predictor_->OnMainFrameRequest(youtube1);
  EXPECT_EQ(2U, predictor_->inflight_navigations_.size());
  predictor_->OnMainFrameRedirect(youtube2);
  EXPECT_EQ(2U, predictor_->inflight_navigations_.size());
  EXPECT_EQ(youtube1.navigation_id.main_frame_url,
            predictor_->inflight_navigations_[youtube_end]->initial_url);

  // http://nyt.com -> http://nytimes.com -> http://m.nytimes.com.
  predictor_->OnMainFrameRequest(nyt1);
  EXPECT_EQ(3U, predictor_->inflight_navigations_.size());
  predictor_->OnMainFrameRedirect(nyt2);
  predictor_->OnMainFrameRedirect(nyt3);
  EXPECT_EQ(3U, predictor_->inflight_navigations_.size());
  EXPECT_EQ(nyt1.navigation_id.main_frame_url,
            predictor_->inflight_navigations_[nyt_end]->initial_url);

  // http://fb.com -> http://facebook.com -> https://facebook.com ->
  // https://m.facebook.com/?refsrc=https%3A%2F%2Fwww.facebook.com%2F&_rdr.
  predictor_->OnMainFrameRequest(fb1);
  EXPECT_EQ(4U, predictor_->inflight_navigations_.size());
  predictor_->OnMainFrameRedirect(fb2);
  predictor_->OnMainFrameRedirect(fb3);
  predictor_->OnMainFrameRedirect(fb4);
  EXPECT_EQ(4U, predictor_->inflight_navigations_.size());
  EXPECT_EQ(fb1.navigation_id.main_frame_url,
            predictor_->inflight_navigations_[fb_end]->initial_url);
}

TEST_F(ResourcePrefetchPredictorTest, OnSubresourceResponse) {
  // If there is no inflight navigation, nothing happens.
  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false);
  predictor_->OnSubresourceResponse(resource1);
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());

  // Add an inflight navigation.
  URLRequestSummary main_frame1 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  predictor_->OnMainFrameRequest(main_frame1);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  // Now add a few subresources.
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  URLRequestSummary resource3 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  predictor_->OnSubresourceResponse(resource1);
  predictor_->OnSubresourceResponse(resource2);
  predictor_->OnSubresourceResponse(resource3);

  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());
  EXPECT_EQ(3U, predictor_->inflight_navigations_[main_frame1.navigation_id]
                    ->subresource_requests.size());
  EXPECT_EQ(resource1,
            predictor_->inflight_navigations_[main_frame1.navigation_id]
                ->subresource_requests[0]);
  EXPECT_EQ(resource2,
            predictor_->inflight_navigations_[main_frame1.navigation_id]
                ->subresource_requests[1]);
  EXPECT_EQ(resource3,
            predictor_->inflight_navigations_[main_frame1.navigation_id]
                ->subresource_requests[2]);
}

TEST_F(ResourcePrefetchPredictorTest, SummarizeResponse) {
  net::HttpResponseInfo response_info;
  response_info.headers =
      MakeResponseHeaders("HTTP/1.1 200 OK\n\nSome: Headers\n");
  response_info.was_cached = true;
  url_request_job_factory_.set_response_info(response_info);

  GURL url("http://www.google.com/cat.png");
  std::unique_ptr<net::URLRequest> request =
      CreateURLRequest(url_request_context_, url, net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, true);
  URLRequestSummary summary;
  EXPECT_TRUE(URLRequestSummary::SummarizeResponse(*request, &summary));
  EXPECT_EQ(url, summary.resource_url);
  EXPECT_EQ(content::RESOURCE_TYPE_IMAGE, summary.resource_type);
  EXPECT_TRUE(summary.was_cached);
  EXPECT_FALSE(summary.has_validators);
  EXPECT_FALSE(summary.always_revalidate);

  // Navigation_id elements should be unset by default.
  EXPECT_EQ(-1, summary.navigation_id.tab_id);
  EXPECT_EQ(GURL(), summary.navigation_id.main_frame_url);
}

TEST_F(ResourcePrefetchPredictorTest, SummarizeResponseContentType) {
  net::HttpResponseInfo response_info;
  response_info.headers = MakeResponseHeaders(
      "HTTP/1.1 200 OK\n\n"
      "Some: Headers\n"
      "Content-Type: image/whatever\n");
  url_request_job_factory_.set_response_info(response_info);
  url_request_job_factory_.set_mime_type("image/png");

  std::unique_ptr<net::URLRequest> request = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_PREFETCH, true);
  URLRequestSummary summary;
  EXPECT_TRUE(URLRequestSummary::SummarizeResponse(*request, &summary));
  EXPECT_EQ(content::RESOURCE_TYPE_IMAGE, summary.resource_type);
}

TEST_F(ResourcePrefetchPredictorTest, SummarizeResponseCachePolicy) {
  net::HttpResponseInfo response_info;
  response_info.headers = MakeResponseHeaders(
      "HTTP/1.1 200 OK\n"
      "Some: Headers\n");
  url_request_job_factory_.set_response_info(response_info);

  std::unique_ptr<net::URLRequest> request_no_validators = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_PREFETCH, true);

  URLRequestSummary summary;
  EXPECT_TRUE(
      URLRequestSummary::SummarizeResponse(*request_no_validators, &summary));
  EXPECT_FALSE(summary.has_validators);

  response_info.headers = MakeResponseHeaders(
      "HTTP/1.1 200 OK\n"
      "ETag: \"Cr66\"\n"
      "Cache-Control: no-cache\n");
  url_request_job_factory_.set_response_info(response_info);
  std::unique_ptr<net::URLRequest> request_etag = CreateURLRequest(
      url_request_context_, GURL("http://www.google.com/cat.png"), net::MEDIUM,
      content::RESOURCE_TYPE_PREFETCH, true);
  EXPECT_TRUE(URLRequestSummary::SummarizeResponse(*request_etag, &summary));
  EXPECT_TRUE(summary.has_validators);
  EXPECT_TRUE(summary.always_revalidate);
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

TEST_F(ResourcePrefetchPredictorTest, PopulateFromManifest) {
  // The data that will be used in populating.
  precache::PrecacheManifest google =
      CreateManifestData(base::Time::Now().ToDoubleT());
  InitializePrecacheResource(google.add_resource(),
                             "https://static.google.com/good.js", 0.9,
                             precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);
  InitializePrecacheResource(google.add_resource(),
                             "https://static.google.com/versioned_removed", 0.8,
                             precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);
  InitializePrecacheResource(google.add_resource(),
                             "https://static.google.com/unused_removed", 0.8,
                             precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);
  InitializePrecacheResource(google.add_resource(),
                             "https://static.google.com/no_store", 0.8,
                             precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);
  InitializePrecacheResource(
      google.add_resource(), "https://static.google.com/good.css", 0.75,
      precache::PrecacheResource::RESOURCE_TYPE_STYLESHEET);
  InitializePrecacheResource(google.add_resource(),
                             "https://static.google.com/low_confidence", 0.6,
                             precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);
  InitializeExperiment(&google, internal::kVersionedRemovedExperiment,
                       {true, false, true, true, true});
  InitializeExperiment(&google, internal::kUnusedRemovedExperiment,
                       {true, true, false, true, true});
  InitializeExperiment(&google, internal::kNoStoreRemovedExperiment,
                       {true, true, true, false, true});

  // The data that's too old.
  base::Time old_time = base::Time::Now() - base::TimeDelta::FromDays(7);
  precache::PrecacheManifest facebook =
      CreateManifestData(old_time.ToDoubleT());
  InitializePrecacheResource(facebook.add_resource(),
                             "https://static.facebook.com/good", 0.9,
                             precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);

  predictor_->manifest_data_->UpdateData("google.com", google);
  predictor_->manifest_data_->UpdateData("facebook.com", facebook);

  std::vector<GURL> urls;
  EXPECT_TRUE(predictor_->PopulateFromManifest("google.com", &urls));
  EXPECT_EQ(urls,
            std::vector<GURL>({GURL("https://static.google.com/good.css"),
                               GURL("https://static.google.com/good.js")}));

  urls.clear();
  EXPECT_FALSE(predictor_->PopulateFromManifest("facebook.com", &urls));
  EXPECT_TRUE(urls.empty());

  urls.clear();
  EXPECT_FALSE(predictor_->PopulateFromManifest("404.com", &urls));
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
  EXPECT_FALSE(predictor_->GetPrefetchData(main_frame_url, &prediction));

  // Add a manifest associated with the main frame host.
  const std::string& resource_url = "https://static.google.com/resource";
  precache::PrecacheManifest manifest =
      CreateManifestData(base::Time::Now().ToDoubleT());
  InitializePrecacheResource(manifest.add_resource(), resource_url, 0.9,
                             precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);
  predictor_->manifest_data_->UpdateData("google.com", manifest);

  urls.clear();
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &prediction));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(resource_url)));

  // Add a resource associated with the main frame host.
  PrefetchData google_host = CreatePrefetchData("google.com", 2);
  const std::string script_url = "https://cdn.google.com/script.js";
  InitializeResourceData(google_host.add_resources(), script_url,
                         content::RESOURCE_TYPE_SCRIPT, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->host_resource_data_->UpdateData(google_host.primary_key(),
                                              google_host);

  urls.clear();
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
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &prediction));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(font_url)));
}

TEST_F(ResourcePrefetchPredictorTest, TestRecordFirstContentfulPaint) {
  auto res1_time = base::TimeTicks::FromInternalValue(1);
  auto res2_time = base::TimeTicks::FromInternalValue(2);
  auto fcp_time = base::TimeTicks::FromInternalValue(3);
  auto res3_time = base::TimeTicks::FromInternalValue(4);

  URLRequestSummary main_frame =
      CreateURLRequestSummary(1, "http://www.google.com");
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false);
  resource1.response_time = res1_time;
  predictor_->RecordURLResponse(resource1);
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  resource2.response_time = res2_time;
  predictor_->RecordURLResponse(resource2);
  URLRequestSummary resource3 = CreateURLRequestSummary(
      1, "http://www.google.com", "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  resource3.response_time = res3_time;
  predictor_->RecordURLResponse(resource3);

  predictor_->RecordFirstContentfulPaint(main_frame.navigation_id, fcp_time);

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();

  PrefetchData host_data = CreatePrefetchData("www.google.com");
  InitializeResourceData(host_data.add_resources(),
                         "http://google.com/style1.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 1.0,
                         net::MEDIUM, false, false);
  InitializeResourceData(
      host_data.add_resources(), "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 2.0, net::MEDIUM, false, false);
  ResourceData* resource3_rd = host_data.add_resources();
  InitializeResourceData(resource3_rd, "http://google.com/script2.js",
                         content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 3.0,
                         net::MEDIUM, false, false);
  resource3_rd->set_before_first_contentful_paint(false);
  EXPECT_EQ(mock_tables_->host_resource_table_.data_,
            PrefetchDataMap({{host_data.primary_key(), host_data}}));
}

}  // namespace predictors
