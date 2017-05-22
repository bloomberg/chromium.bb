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
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ContainerEq;
using testing::Pointee;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::UnorderedElementsAre;

namespace predictors {

using URLRequestSummary = ResourcePrefetchPredictor::URLRequestSummary;
using PageRequestSummary = ResourcePrefetchPredictor::PageRequestSummary;
using PrefetchDataMap = ResourcePrefetchPredictorTables::PrefetchDataMap;
using RedirectDataMap = ResourcePrefetchPredictorTables::RedirectDataMap;
using ManifestDataMap = ResourcePrefetchPredictorTables::ManifestDataMap;
using OriginDataMap = ResourcePrefetchPredictorTables::OriginDataMap;

scoped_refptr<net::HttpResponseHeaders> MakeResponseHeaders(
    const char* headers) {
  return make_scoped_refptr(new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(headers, strlen(headers))));
}

class EmptyURLRequestDelegate : public net::URLRequest::Delegate {
  void OnResponseStarted(net::URLRequest* request, int net_error) override {}
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override {}
};

class MockURLRequestJob : public net::URLRequestJob {
 public:
  MockURLRequestJob(net::URLRequest* request,
                    const net::HttpResponseInfo& response_info,
                    const std::string& mime_type)
      : net::URLRequestJob(request, nullptr),
        response_info_(response_info),
        mime_type_(mime_type) {}

  bool GetMimeType(std::string* mime_type) const override {
    *mime_type = mime_type_;
    return true;
  }

 protected:
  void Start() override { NotifyHeadersComplete(); }
  void GetResponseInfo(net::HttpResponseInfo* info) override {
    *info = response_info_;
  }

 private:
  net::HttpResponseInfo response_info_;
  std::string mime_type_;
};

class MockURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  MockURLRequestJobFactory() {}
  ~MockURLRequestJobFactory() override {}

  net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new MockURLRequestJob(request, response_info_, mime_type_);
  }

  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const override {
    return nullptr;
  }

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return nullptr;
  }

  bool IsHandledProtocol(const std::string& scheme) const override {
    return true;
  }

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return true;
  }

  void set_response_info(const net::HttpResponseInfo& response_info) {
    response_info_ = response_info;
  }

  void set_mime_type(const std::string& mime_type) { mime_type_ = mime_type; }

 private:
  net::HttpResponseInfo response_info_;
  std::string mime_type_;
};

class MockResourcePrefetchPredictorTables
    : public ResourcePrefetchPredictorTables {
 public:
  MockResourcePrefetchPredictorTables() { }

  MOCK_METHOD6(GetAllData,
               void(PrefetchDataMap* url_data_map,
                    PrefetchDataMap* host_data_map,
                    RedirectDataMap* url_redirect_data_map,
                    RedirectDataMap* host_redirect_data_map,
                    ManifestDataMap* manifest_data_map,
                    OriginDataMap* origin_data_map));
  MOCK_METHOD2(UpdateResourceData,
               void(const PrefetchData& data, PrefetchKeyType key_type));
  MOCK_METHOD2(UpdateRedirectData,
               void(const RedirectData& data, PrefetchKeyType key_type));
  MOCK_METHOD2(UpdateManifestData,
               void(const std::string& host,
                    const precache::PrecacheManifest& manifest_data));
  MOCK_METHOD1(UpdateOriginData, void(const OriginData& origin_data));
  MOCK_METHOD2(DeleteResourceData,
               void(const std::vector<std::string>& urls,
                    const std::vector<std::string>& hosts));
  MOCK_METHOD1(DeleteOriginData, void(const std::vector<std::string>& hosts));
  MOCK_METHOD2(DeleteSingleResourceDataPoint,
               void(const std::string& key, PrefetchKeyType key_type));
  MOCK_METHOD2(DeleteRedirectData,
               void(const std::vector<std::string>& urls,
                    const std::vector<std::string>& hosts));
  MOCK_METHOD2(DeleteSingleRedirectDataPoint,
               void(const std::string& key, PrefetchKeyType key_type));
  MOCK_METHOD1(DeleteManifestData, void(const std::vector<std::string>& hosts));
  MOCK_METHOD0(DeleteAllData, void());

 protected:
  ~MockResourcePrefetchPredictorTables() { }
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

  std::unique_ptr<net::URLRequest> CreateURLRequest(
      const GURL& url,
      net::RequestPriority priority,
      content::ResourceType resource_type,
      bool is_main_frame) {
    std::unique_ptr<net::URLRequest> request =
        url_request_context_.CreateRequest(url, priority,
                                           &url_request_delegate_,
                                           TRAFFIC_ANNOTATION_FOR_TESTS);
    request->set_first_party_for_cookies(url);
    content::ResourceRequestInfo::AllocateForTesting(
        request.get(), resource_type, nullptr, -1, -1, -1, is_main_frame, false,
        false, true, content::PREVIEWS_OFF);
    request->Start();
    return request;
  }

  void InitializePredictor() {
    predictor_->StartInitialization();
    base::RunLoop loop;
    loop.RunUntilIdle();  // Runs the DB lookup.
    profile_->BlockUntilHistoryProcessesPendingRequests();
  }

  void ResetPredictor() {
    LoadingPredictorConfig config;
    config.max_urls_to_track = 3;
    config.max_hosts_to_track = 2;
    config.min_url_visit_count = 2;
    config.max_resources_per_entry = 4;
    config.max_consecutive_misses = 2;
    config.max_redirect_consecutive_misses = 2;
    config.min_resource_confidence_to_trigger_prefetch = 0.5;
    config.is_url_learning_enabled = true;
    config.is_manifests_enabled = true;
    config.is_origin_learning_enabled = true;

    config.mode |= LoadingPredictorConfig::LEARNING;
    predictor_.reset(new ResourcePrefetchPredictor(config, profile_.get()));
    predictor_->set_mock_tables(mock_tables_);
  }

  void InitializeSampleData();
  void TestRedirectStatusHistogram(
      const std::string& predictor_initial_key,
      const std::string& predictor_key,
      const std::string& navigation_initial_url,
      const std::string& navigation_url,
      ResourcePrefetchPredictor::RedirectStatus expected_status);

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  net::TestURLRequestContext url_request_context_;

  std::unique_ptr<ResourcePrefetchPredictor> predictor_;
  scoped_refptr<StrictMock<MockResourcePrefetchPredictorTables>> mock_tables_;

  PrefetchDataMap test_url_data_;
  PrefetchDataMap test_host_data_;
  RedirectDataMap test_url_redirect_data_;
  RedirectDataMap test_host_redirect_data_;
  ManifestDataMap test_manifest_data_;
  OriginDataMap test_origin_data_;

  MockURLRequestJobFactory url_request_job_factory_;
  EmptyURLRequestDelegate url_request_delegate_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

ResourcePrefetchPredictorTest::ResourcePrefetchPredictorTest()
    : thread_bundle_(),
      profile_(new TestingProfile()),
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
  EXPECT_CALL(*mock_tables_.get(),
              GetAllData(Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(ManifestDataMap())),
                         Pointee(ContainerEq(OriginDataMap()))));
  InitializePredictor();
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::INITIALIZED);

  url_request_context_.set_job_factory(&url_request_job_factory_);

  histogram_tester_.reset(new base::HistogramTester());
}

void ResourcePrefetchPredictorTest::TearDown() {
  predictor_.reset(NULL);
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

void ResourcePrefetchPredictorTest::TestRedirectStatusHistogram(
    const std::string& predictor_initial_key,
    const std::string& predictor_key,
    const std::string& navigation_initial_url,
    const std::string& navigation_url,
    ResourcePrefetchPredictor::RedirectStatus expected_status) {
  // Database initialization.
  const std::string& script_url = "https://cdn.google.com/script.js";
  PrefetchData google = CreatePrefetchData(predictor_key, 1);
  // We need at least one resource for prediction.
  InitializeResourceData(google.add_resources(), script_url,
                         content::RESOURCE_TYPE_SCRIPT, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->host_table_cache_->insert(
      std::make_pair(google.primary_key(), google));

  if (predictor_initial_key != predictor_key) {
    RedirectData redirect = CreateRedirectData(predictor_initial_key, 1);
    InitializeRedirectStat(redirect.add_redirect_endpoints(), predictor_key, 10,
                           0, 0);
    predictor_->host_redirect_table_cache_->insert(
        std::make_pair(redirect.primary_key(), redirect));
  }

  // Navigation simulation.
  using testing::_;
  EXPECT_CALL(*mock_tables_.get(), UpdateResourceData(_, _));
  EXPECT_CALL(*mock_tables_.get(), UpdateRedirectData(_, _));
  EXPECT_CALL(*mock_tables_.get(), UpdateOriginData(_));
  URLRequestSummary initial =
      CreateURLRequestSummary(1, navigation_initial_url);
  predictor_->RecordURLRequest(initial);

  if (navigation_initial_url != navigation_url) {
    URLRequestSummary redirect =
        CreateRedirectRequestSummary(1, navigation_initial_url, navigation_url);
    predictor_->RecordURLRedirect(redirect);
  }
  NavigationID navigation_id = CreateNavigationID(1, navigation_url);

  URLRequestSummary script = CreateURLRequestSummary(
      1, navigation_url, script_url, content::RESOURCE_TYPE_SCRIPT);
  predictor_->RecordURLResponse(script);

  predictor_->RecordMainFrameLoadComplete(navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();

  // Histogram check.
  histogram_tester_->ExpectBucketCount(
      internal::kResourcePrefetchPredictorRedirectStatusHistogram,
      static_cast<int>(expected_status), 1);
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
  EXPECT_TRUE(predictor_->url_table_cache_->empty());
  EXPECT_TRUE(predictor_->host_table_cache_->empty());
  EXPECT_TRUE(predictor_->url_redirect_table_cache_->empty());
  EXPECT_TRUE(predictor_->host_redirect_table_cache_->empty());
  EXPECT_TRUE(predictor_->manifest_table_cache_->empty());
  EXPECT_TRUE(predictor_->origin_table_cache_->empty());
}

// Tests that the history and the db tables data are loaded correctly.
TEST_F(ResourcePrefetchPredictorTest, LazilyInitializeWithData) {
  AddUrlToHistory("http://www.google.com/", 4);
  AddUrlToHistory("http://www.yahoo.com/", 2);

  EXPECT_CALL(*mock_tables_.get(),
              GetAllData(Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(ManifestDataMap())),
                         Pointee(ContainerEq(OriginDataMap()))))
      .WillOnce(DoAll(SetArgPointee<0>(test_url_data_),
                      SetArgPointee<1>(test_host_data_),
                      SetArgPointee<2>(test_url_redirect_data_),
                      SetArgPointee<3>(test_host_redirect_data_),
                      SetArgPointee<4>(test_manifest_data_),
                      SetArgPointee<5>(test_origin_data_)));

  ResetPredictor();
  InitializePredictor();

  // Test that the internal variables correctly initialized.
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::INITIALIZED);
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());

  EXPECT_EQ(test_url_data_, *predictor_->url_table_cache_);
  EXPECT_EQ(test_host_data_, *predictor_->host_table_cache_);
  EXPECT_EQ(test_url_redirect_data_, *predictor_->url_redirect_table_cache_);
  EXPECT_EQ(test_host_redirect_data_, *predictor_->host_redirect_table_cache_);
  EXPECT_EQ(test_manifest_data_, *predictor_->manifest_table_cache_);
  EXPECT_EQ(test_origin_data_, *predictor_->origin_table_cache_);
}

// Single navigation but history count is low, so should not record.
TEST_F(ResourcePrefetchPredictorTest, NavigationNotRecorded) {
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

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(
      predictor_.get());
  EXPECT_CALL(
      mock_observer,
      OnNavigationLearned(kVisitCount,
                          CreatePageRequestSummary(
                              "https://www.google.com", "http://www.google.com",
                              {resource1, resource2, resource3})));

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
  EXPECT_CALL(*mock_tables_.get(),
              UpdateResourceData(host_data, PREFETCH_KEY_TYPE_HOST));

  OriginData origin_data = CreateOriginData("www.google.com");
  InitializeOriginStat(origin_data.add_origins(), "https://google.com/", 1, 0,
                       0, 1., false, true);
  EXPECT_CALL(*mock_tables_.get(), UpdateOriginData(origin_data));

  RedirectData host_redirect_data = CreateRedirectData("www.google.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "www.google.com", 1, 0, 0);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateRedirectData(host_redirect_data, PREFETCH_KEY_TYPE_HOST));

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();
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

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(
      predictor_.get());
  EXPECT_CALL(mock_observer,
              OnNavigationLearned(
                  kVisitCount, CreatePageRequestSummary("http://www.google.com",
                                                        "http://www.google.com",
                                                        resources)));

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
  EXPECT_CALL(*mock_tables_.get(),
              UpdateResourceData(url_data, PREFETCH_KEY_TYPE_URL));

  OriginData origin_data = CreateOriginData("www.google.com");
  InitializeOriginStat(origin_data.add_origins(), "http://static.google.com/",
                       1, 0, 0, 2., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://dev.null.google.com/",
                       1, 0, 0, 4., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://google.com/", 1, 0, 0,
                       1., false, true);
  InitializeOriginStat(origin_data.add_origins(), "http://reader.google.com/",
                       1, 0, 0, 3., false, true);
  EXPECT_CALL(*mock_tables_.get(), UpdateOriginData(origin_data));

  PrefetchData host_data = CreatePrefetchData("www.google.com");
  host_data.mutable_resources()->CopyFrom(url_data.resources());
  EXPECT_CALL(*mock_tables_.get(),
              UpdateResourceData(host_data, PREFETCH_KEY_TYPE_HOST));

  RedirectData url_redirect_data = CreateRedirectData("http://www.google.com/");
  InitializeRedirectStat(url_redirect_data.add_redirect_endpoints(),
                         "http://www.google.com/", 1, 0, 0);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateRedirectData(url_redirect_data, PREFETCH_KEY_TYPE_URL));

  RedirectData host_redirect_data = CreateRedirectData("www.google.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "www.google.com", 1, 0, 0);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateRedirectData(host_redirect_data, PREFETCH_KEY_TYPE_HOST));

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();
}

// Tests that navigation is recorded correctly for URL already present in
// the database cache.
TEST_F(ResourcePrefetchPredictorTest, NavigationUrlInDB) {
  const int kVisitCount = 4;
  AddUrlToHistory("http://www.google.com", kVisitCount);

  EXPECT_CALL(*mock_tables_.get(),
              GetAllData(Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(ManifestDataMap())),
                         Pointee(ContainerEq(OriginDataMap()))))
      .WillOnce(DoAll(SetArgPointee<0>(test_url_data_),
                      SetArgPointee<1>(test_host_data_)));
  ResetPredictor();
  InitializePredictor();
  EXPECT_EQ(3U, predictor_->url_table_cache_->size());
  EXPECT_EQ(2U, predictor_->host_table_cache_->size());

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

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(
      predictor_.get());
  EXPECT_CALL(mock_observer,
              OnNavigationLearned(
                  kVisitCount, CreatePageRequestSummary("http://www.google.com",
                                                        "http://www.google.com",
                                                        resources)));

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
  EXPECT_CALL(*mock_tables_.get(),
              UpdateResourceData(url_data, PREFETCH_KEY_TYPE_URL));
  EXPECT_CALL(*mock_tables_.get(),
              DeleteSingleResourceDataPoint("www.facebook.com",
                                            PREFETCH_KEY_TYPE_HOST));

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
  EXPECT_CALL(*mock_tables_.get(),
              UpdateResourceData(host_data, PREFETCH_KEY_TYPE_HOST));

  RedirectData url_redirect_data = CreateRedirectData("http://www.google.com/");
  InitializeRedirectStat(url_redirect_data.add_redirect_endpoints(),
                         "http://www.google.com/", 1, 0, 0);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateRedirectData(url_redirect_data, PREFETCH_KEY_TYPE_URL));

  RedirectData host_redirect_data = CreateRedirectData("www.google.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "www.google.com", 1, 0, 0);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateRedirectData(host_redirect_data, PREFETCH_KEY_TYPE_HOST));

  OriginData origin_data = CreateOriginData("www.google.com");
  InitializeOriginStat(origin_data.add_origins(), "http://static.google.com/",
                       1, 0, 0, 2., true, true);
  InitializeOriginStat(origin_data.add_origins(), "http://google.com/", 1, 0, 0,
                       1., false, true);
  EXPECT_CALL(*mock_tables_.get(), UpdateOriginData(origin_data));

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();
}

// Tests that a URL is deleted before another is added if the cache is full.
TEST_F(ResourcePrefetchPredictorTest, NavigationUrlNotInDBAndDBFull) {
  const int kVisitCount = 4;
  AddUrlToHistory("http://www.nike.com/", kVisitCount);

  EXPECT_CALL(*mock_tables_.get(),
              GetAllData(Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(ManifestDataMap())),
                         Pointee(ContainerEq(OriginDataMap()))))
      .WillOnce(DoAll(SetArgPointee<0>(test_url_data_),
                      SetArgPointee<1>(test_host_data_),
                      SetArgPointee<5>(test_origin_data_)));
  ResetPredictor();
  InitializePredictor();
  EXPECT_EQ(3U, predictor_->url_table_cache_->size());
  EXPECT_EQ(2U, predictor_->host_table_cache_->size());
  EXPECT_EQ(2U, predictor_->origin_table_cache_->size());

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

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(
      predictor_.get());
  EXPECT_CALL(mock_observer,
              OnNavigationLearned(
                  kVisitCount, CreatePageRequestSummary(
                                   "http://www.nike.com", "http://www.nike.com",
                                   {resource1, resource2})));

  EXPECT_CALL(*mock_tables_.get(),
              DeleteSingleResourceDataPoint("http://www.google.com/",
                                            PREFETCH_KEY_TYPE_URL));
  EXPECT_CALL(*mock_tables_.get(),
              DeleteSingleResourceDataPoint("www.facebook.com",
                                            PREFETCH_KEY_TYPE_HOST));
  EXPECT_CALL(*mock_tables_.get(),
              DeleteOriginData(std::vector<std::string>({"google.com"})));

  PrefetchData url_data = CreatePrefetchData("http://www.nike.com/");
  InitializeResourceData(url_data.add_resources(), "http://nike.com/style1.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 1.0,
                         net::MEDIUM, false, false);
  InitializeResourceData(url_data.add_resources(), "http://nike.com/image2.png",
                         content::RESOURCE_TYPE_IMAGE, 1, 0, 0, 2.0,
                         net::MEDIUM, false, false);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateResourceData(url_data, PREFETCH_KEY_TYPE_URL));

  PrefetchData host_data = CreatePrefetchData("www.nike.com");
  host_data.mutable_resources()->CopyFrom(url_data.resources());
  EXPECT_CALL(*mock_tables_.get(),
              UpdateResourceData(host_data, PREFETCH_KEY_TYPE_HOST));

  RedirectData url_redirect_data = CreateRedirectData("http://www.nike.com/");
  InitializeRedirectStat(url_redirect_data.add_redirect_endpoints(),
                         "http://www.nike.com/", 1, 0, 0);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateRedirectData(url_redirect_data, PREFETCH_KEY_TYPE_URL));

  RedirectData host_redirect_data = CreateRedirectData("www.nike.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "www.nike.com", 1, 0, 0);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateRedirectData(host_redirect_data, PREFETCH_KEY_TYPE_HOST));

  EXPECT_CALL(*mock_tables_.get(), UpdateOriginData(testing::_));

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();
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

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(
      predictor_.get());
  EXPECT_CALL(
      mock_observer,
      OnNavigationLearned(kVisitCount, CreatePageRequestSummary(
                                           "https://facebook.com/google",
                                           "http://fb.com/google",
                                           std::vector<URLRequestSummary>())));

  // Since the navigation hasn't resources, corresponding entry
  // in resource table will be deleted.
  EXPECT_CALL(*mock_tables_.get(),
              DeleteSingleResourceDataPoint("https://facebook.com/google",
                                            PREFETCH_KEY_TYPE_URL));
  EXPECT_CALL(*mock_tables_.get(), DeleteSingleResourceDataPoint(
                                       "facebook.com", PREFETCH_KEY_TYPE_HOST));

  RedirectData url_redirect_data = CreateRedirectData("http://fb.com/google");
  InitializeRedirectStat(url_redirect_data.add_redirect_endpoints(),
                         "https://facebook.com/google", 1, 0, 0);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateRedirectData(url_redirect_data, PREFETCH_KEY_TYPE_URL));

  RedirectData host_redirect_data = CreateRedirectData("fb.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "facebook.com", 1, 0, 0);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateRedirectData(host_redirect_data, PREFETCH_KEY_TYPE_HOST));

  predictor_->RecordMainFrameLoadComplete(fb_end);
  profile_->BlockUntilHistoryProcessesPendingRequests();
}

// Tests that redirect is recorded correctly for URL already present in
// the database cache.
TEST_F(ResourcePrefetchPredictorTest, RedirectUrlInDB) {
  const int kVisitCount = 7;
  AddUrlToHistory("https://facebook.com/google", kVisitCount);

  EXPECT_CALL(*mock_tables_.get(),
              GetAllData(Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(ManifestDataMap())),
                         Pointee(ContainerEq(OriginDataMap()))))
      .WillOnce(DoAll(SetArgPointee<2>(test_url_redirect_data_),
                      SetArgPointee<3>(test_host_redirect_data_)));
  ResetPredictor();
  InitializePredictor();
  EXPECT_EQ(3U, predictor_->url_redirect_table_cache_->size());
  EXPECT_EQ(2U, predictor_->host_redirect_table_cache_->size());

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

  StrictMock<MockResourcePrefetchPredictorObserver> mock_observer(
      predictor_.get());
  EXPECT_CALL(
      mock_observer,
      OnNavigationLearned(kVisitCount, CreatePageRequestSummary(
                                           "https://facebook.com/google",
                                           "http://fb.com/google",
                                           std::vector<URLRequestSummary>())));

  // Oldest entries in tables will be superseded and deleted.
  EXPECT_CALL(*mock_tables_.get(),
              DeleteSingleRedirectDataPoint("bbc.com", PREFETCH_KEY_TYPE_HOST));

  // Since the navigation hasn't resources, corresponding entry
  // in resource table will be deleted.
  EXPECT_CALL(*mock_tables_.get(),
              DeleteSingleResourceDataPoint("https://facebook.com/google",
                                            PREFETCH_KEY_TYPE_URL));
  EXPECT_CALL(*mock_tables_.get(), DeleteSingleResourceDataPoint(
                                       "facebook.com", PREFETCH_KEY_TYPE_HOST));

  RedirectData url_redirect_data = CreateRedirectData("http://fb.com/google");
  InitializeRedirectStat(url_redirect_data.add_redirect_endpoints(),
                         "https://facebook.com/google", 6, 1, 0);
  // Existing redirect to https://facebook.com/login will be deleted because of
  // too many consecutive misses.
  EXPECT_CALL(*mock_tables_.get(),
              UpdateRedirectData(url_redirect_data, PREFETCH_KEY_TYPE_URL));

  RedirectData host_redirect_data = CreateRedirectData("fb.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "facebook.com", 1, 0, 0);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateRedirectData(host_redirect_data, PREFETCH_KEY_TYPE_HOST));

  predictor_->RecordMainFrameLoadComplete(fb_end);
  profile_->BlockUntilHistoryProcessesPendingRequests();
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

  EXPECT_CALL(*mock_tables_.get(), UpdateManifestData("google.com", manifest));

  predictor_->OnManifestFetched("google.com", manifest);
}

TEST_F(ResourcePrefetchPredictorTest, ManifestHostInDB) {
  EXPECT_CALL(*mock_tables_.get(),
              GetAllData(Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(ManifestDataMap())),
                         Pointee(ContainerEq(OriginDataMap()))))
      .WillOnce(SetArgPointee<4>(test_manifest_data_));
  ResetPredictor();
  InitializePredictor();
  EXPECT_EQ(2U, predictor_->manifest_table_cache_->size());

  precache::PrecacheManifest manifest =
      CreateManifestData(base::Time::Now().ToDoubleT());
  InitializePrecacheResource(manifest.add_resource(),
                             "http://google.com/image.jpg", 0.1,
                             precache::PrecacheResource::RESOURCE_TYPE_IMAGE);

  EXPECT_CALL(*mock_tables_.get(), UpdateManifestData("google.com", manifest));

  predictor_->OnManifestFetched("google.com", manifest);
}

TEST_F(ResourcePrefetchPredictorTest, ManifestHostNotInDBAndDBFull) {
  EXPECT_CALL(*mock_tables_.get(),
              GetAllData(Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(ManifestDataMap())),
                         Pointee(ContainerEq(OriginDataMap()))))
      .WillOnce(SetArgPointee<4>(test_manifest_data_));
  ResetPredictor();
  InitializePredictor();
  EXPECT_EQ(2U, predictor_->manifest_table_cache_->size());

  precache::PrecacheManifest manifest =
      CreateManifestData(base::Time::Now().ToDoubleT());
  InitializePrecacheResource(manifest.add_resource(),
                             "http://en.wikipedia.org/logo.png", 1.0,
                             precache::PrecacheResource::RESOURCE_TYPE_IMAGE);

  EXPECT_CALL(*mock_tables_.get(),
              DeleteManifestData(std::vector<std::string>({"google.com"})));

  EXPECT_CALL(*mock_tables_.get(),
              UpdateManifestData("en.wikipedia.org", manifest));

  predictor_->OnManifestFetched("en.wikipedia.org", manifest);
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

  int manifest_size = manifest.ByteSize();
  auto match_size = [manifest_size](const precache::PrecacheManifest& m) {
    return m.ByteSize() == manifest_size;
  };
  EXPECT_CALL(
      *mock_tables_.get(),
      UpdateManifestData("google.com",
                         testing::AllOf(manifest, testing::Truly(match_size))));

  predictor_->OnManifestFetched("google.com", manifest_with_unknown_fields);
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

  // No calls to DB should happen.
  predictor_->OnManifestFetched("google.com", manifest);
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
  predictor_->host_table_cache_->insert({google.primary_key(), google});

  precache::PrecacheManifest manifest =
      CreateManifestData(base::Time::Now().ToDoubleT());
  InitializePrecacheResource(manifest.add_resource(), script_url, 0.9,
                             precache::PrecacheResource::RESOURCE_TYPE_SCRIPT);
  InitializePrecacheResource(
      manifest.add_resource(), style_url, 0.75,
      precache::PrecacheResource::RESOURCE_TYPE_STYLESHEET);
  InitializeExperiment(&manifest, internal::kUnusedRemovedExperiment,
                       {true, false});

  // style_url should be removed.
  google.mutable_resources()->RemoveLast();
  EXPECT_CALL(*mock_tables_.get(),
              UpdateResourceData(google, PREFETCH_KEY_TYPE_HOST));
  EXPECT_CALL(*mock_tables_.get(), UpdateManifestData("google.com", manifest));

  predictor_->OnManifestFetched("google.com", manifest);
}

TEST_F(ResourcePrefetchPredictorTest, DeleteUrls) {
  // Add some dummy entries to cache.
  predictor_->url_table_cache_->insert(
      std::make_pair("http://www.google.com/page1.html",
                     CreatePrefetchData("http://www.google.com/page1.html")));
  predictor_->url_table_cache_->insert(
      std::make_pair("http://www.google.com/page2.html",
                     CreatePrefetchData("http://www.google.com/page2.html")));
  predictor_->url_table_cache_->insert(std::make_pair(
      "http://www.yahoo.com/", CreatePrefetchData("http://www.yahoo.com/")));
  predictor_->url_table_cache_->insert(std::make_pair(
      "http://www.apple.com/", CreatePrefetchData("http://www.apple.com/")));
  predictor_->url_table_cache_->insert(std::make_pair(
      "http://www.nike.com/", CreatePrefetchData("http://www.nike.com/")));

  predictor_->host_table_cache_->insert(
      std::make_pair("www.google.com", CreatePrefetchData("www.google.com")));
  predictor_->host_table_cache_->insert(
      std::make_pair("www.yahoo.com", CreatePrefetchData("www.yahoo.com")));
  predictor_->host_table_cache_->insert(
      std::make_pair("www.apple.com", CreatePrefetchData("www.apple.com")));

  predictor_->url_redirect_table_cache_->insert(
      std::make_pair("http://www.google.com/page1.html",
                     CreateRedirectData("http://www.google.com/page1.html")));
  predictor_->url_redirect_table_cache_->insert(
      std::make_pair("http://www.google.com/page2.html",
                     CreateRedirectData("http://www.google.com/page2.html")));
  predictor_->url_redirect_table_cache_->insert(std::make_pair(
      "http://www.apple.com/", CreateRedirectData("http://www.apple.com/")));
  predictor_->url_redirect_table_cache_->insert(
      std::make_pair("http://nyt.com/", CreateRedirectData("http://nyt.com/")));

  predictor_->host_redirect_table_cache_->insert(
      std::make_pair("www.google.com", CreateRedirectData("www.google.com")));
  predictor_->host_redirect_table_cache_->insert(
      std::make_pair("www.nike.com", CreateRedirectData("www.nike.com")));
  predictor_->host_redirect_table_cache_->insert(std::make_pair(
      "www.wikipedia.org", CreateRedirectData("www.wikipedia.org")));

  predictor_->manifest_table_cache_->insert(
      std::make_pair("google.com", CreateManifestData()));
  predictor_->manifest_table_cache_->insert(
      std::make_pair("apple.com", CreateManifestData()));
  predictor_->manifest_table_cache_->insert(
      std::make_pair("en.wikipedia.org", CreateManifestData()));

  history::URLRows rows;
  rows.push_back(history::URLRow(GURL("http://www.google.com/page2.html")));
  rows.push_back(history::URLRow(GURL("http://www.apple.com")));
  rows.push_back(history::URLRow(GURL("http://www.nike.com")));

  std::vector<std::string> urls_to_delete, hosts_to_delete,
      url_redirects_to_delete, host_redirects_to_delete, manifests_to_delete;
  urls_to_delete.push_back("http://www.google.com/page2.html");
  urls_to_delete.push_back("http://www.apple.com/");
  urls_to_delete.push_back("http://www.nike.com/");
  hosts_to_delete.push_back("www.google.com");
  hosts_to_delete.push_back("www.apple.com");
  url_redirects_to_delete.push_back("http://www.google.com/page2.html");
  url_redirects_to_delete.push_back("http://www.apple.com/");
  host_redirects_to_delete.push_back("www.google.com");
  host_redirects_to_delete.push_back("www.nike.com");
  manifests_to_delete.push_back("google.com");
  manifests_to_delete.push_back("apple.com");

  EXPECT_CALL(*mock_tables_.get(),
              DeleteResourceData(ContainerEq(urls_to_delete),
                                 ContainerEq(hosts_to_delete)));
  EXPECT_CALL(*mock_tables_.get(),
              DeleteRedirectData(ContainerEq(url_redirects_to_delete),
                                 ContainerEq(host_redirects_to_delete)));
  EXPECT_CALL(*mock_tables_.get(),
              DeleteManifestData(ContainerEq(manifests_to_delete)));

  predictor_->DeleteUrls(rows);
  EXPECT_EQ(2U, predictor_->url_table_cache_->size());
  EXPECT_EQ(1U, predictor_->host_table_cache_->size());
  EXPECT_EQ(2U, predictor_->url_redirect_table_cache_->size());
  EXPECT_EQ(1U, predictor_->host_redirect_table_cache_->size());
  EXPECT_EQ(1U, predictor_->manifest_table_cache_->size());

  EXPECT_CALL(*mock_tables_.get(), DeleteAllData());

  predictor_->DeleteAllUrls();
  EXPECT_TRUE(predictor_->url_table_cache_->empty());
  EXPECT_TRUE(predictor_->host_table_cache_->empty());
  EXPECT_TRUE(predictor_->url_redirect_table_cache_->empty());
  EXPECT_TRUE(predictor_->host_redirect_table_cache_->empty());
  EXPECT_TRUE(predictor_->manifest_table_cache_->empty());
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

TEST_F(ResourcePrefetchPredictorTest, HandledResourceTypes) {
  EXPECT_TRUE(ResourcePrefetchPredictor::IsHandledResourceType(
      content::RESOURCE_TYPE_STYLESHEET, "bogus/mime-type"));
  EXPECT_TRUE(ResourcePrefetchPredictor::IsHandledResourceType(
      content::RESOURCE_TYPE_STYLESHEET, ""));
  EXPECT_FALSE(ResourcePrefetchPredictor::IsHandledResourceType(
      content::RESOURCE_TYPE_WORKER, "text/css"));
  EXPECT_FALSE(ResourcePrefetchPredictor::IsHandledResourceType(
      content::RESOURCE_TYPE_WORKER, ""));
  EXPECT_TRUE(ResourcePrefetchPredictor::IsHandledResourceType(
      content::RESOURCE_TYPE_PREFETCH, "text/css"));
  EXPECT_FALSE(ResourcePrefetchPredictor::IsHandledResourceType(
      content::RESOURCE_TYPE_PREFETCH, "bogus/mime-type"));
  EXPECT_FALSE(ResourcePrefetchPredictor::IsHandledResourceType(
      content::RESOURCE_TYPE_PREFETCH, ""));
  EXPECT_TRUE(ResourcePrefetchPredictor::IsHandledResourceType(
      content::RESOURCE_TYPE_PREFETCH, "application/font-woff"));
  EXPECT_TRUE(ResourcePrefetchPredictor::IsHandledResourceType(
      content::RESOURCE_TYPE_PREFETCH, "font/woff2"));
  EXPECT_FALSE(ResourcePrefetchPredictor::IsHandledResourceType(
      content::RESOURCE_TYPE_XHR, ""));
  EXPECT_FALSE(ResourcePrefetchPredictor::IsHandledResourceType(
      content::RESOURCE_TYPE_XHR, "bogus/mime-type"));
  EXPECT_TRUE(ResourcePrefetchPredictor::IsHandledResourceType(
      content::RESOURCE_TYPE_XHR, "application/javascript"));
}

TEST_F(ResourcePrefetchPredictorTest, ShouldRecordRequestMainFrame) {
  std::unique_ptr<net::URLRequest> http_request =
      CreateURLRequest(GURL("http://www.google.com"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_TRUE(ResourcePrefetchPredictor::ShouldRecordRequest(
      http_request.get(), content::RESOURCE_TYPE_MAIN_FRAME));

  std::unique_ptr<net::URLRequest> https_request =
      CreateURLRequest(GURL("https://www.google.com"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_TRUE(ResourcePrefetchPredictor::ShouldRecordRequest(
      https_request.get(), content::RESOURCE_TYPE_MAIN_FRAME));

  std::unique_ptr<net::URLRequest> file_request =
      CreateURLRequest(GURL("file://www.google.com"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordRequest(
      file_request.get(), content::RESOURCE_TYPE_MAIN_FRAME));

  std::unique_ptr<net::URLRequest> https_request_with_port =
      CreateURLRequest(GURL("https://www.google.com:666"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordRequest(
      https_request_with_port.get(), content::RESOURCE_TYPE_MAIN_FRAME));
}

TEST_F(ResourcePrefetchPredictorTest, ShouldRecordRequestSubResource) {
  std::unique_ptr<net::URLRequest> http_request =
      CreateURLRequest(GURL("http://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, false);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordRequest(
      http_request.get(), content::RESOURCE_TYPE_IMAGE));

  std::unique_ptr<net::URLRequest> https_request =
      CreateURLRequest(GURL("https://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, false);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordRequest(
      https_request.get(), content::RESOURCE_TYPE_IMAGE));

  std::unique_ptr<net::URLRequest> file_request =
      CreateURLRequest(GURL("file://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, false);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordRequest(
      file_request.get(), content::RESOURCE_TYPE_IMAGE));

  std::unique_ptr<net::URLRequest> https_request_with_port =
      CreateURLRequest(GURL("https://www.google.com:666/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, false);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordRequest(
      https_request_with_port.get(), content::RESOURCE_TYPE_IMAGE));
}

TEST_F(ResourcePrefetchPredictorTest, ShouldRecordResponseMainFrame) {
  net::HttpResponseInfo response_info;
  response_info.headers = MakeResponseHeaders("");
  url_request_job_factory_.set_response_info(response_info);

  std::unique_ptr<net::URLRequest> http_request =
      CreateURLRequest(GURL("http://www.google.com"), net::MEDIUM,
                       content::RESOURCE_TYPE_MAIN_FRAME, true);
  EXPECT_TRUE(
      ResourcePrefetchPredictor::ShouldRecordResponse(http_request.get()));

  std::unique_ptr<net::URLRequest> https_request =
      CreateURLRequest(GURL("https://www.google.com"), net::MEDIUM,
                       content::RESOURCE_TYPE_MAIN_FRAME, true);
  EXPECT_TRUE(
      ResourcePrefetchPredictor::ShouldRecordResponse(https_request.get()));

  std::unique_ptr<net::URLRequest> file_request =
      CreateURLRequest(GURL("file://www.google.com"), net::MEDIUM,
                       content::RESOURCE_TYPE_MAIN_FRAME, true);
  EXPECT_FALSE(
      ResourcePrefetchPredictor::ShouldRecordResponse(file_request.get()));

  std::unique_ptr<net::URLRequest> https_request_with_port =
      CreateURLRequest(GURL("https://www.google.com:666"), net::MEDIUM,
                       content::RESOURCE_TYPE_MAIN_FRAME, true);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordResponse(
      https_request_with_port.get()));
}

TEST_F(ResourcePrefetchPredictorTest, ShouldRecordResponseSubresource) {
  net::HttpResponseInfo response_info;
  response_info.headers =
      MakeResponseHeaders("HTTP/1.1 200 OK\n\nSome: Headers\n");
  response_info.was_cached = true;
  url_request_job_factory_.set_response_info(response_info);

  // Protocol.
  std::unique_ptr<net::URLRequest> http_image_request =
      CreateURLRequest(GURL("http://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_TRUE(ResourcePrefetchPredictor::ShouldRecordResponse(
      http_image_request.get()));

  std::unique_ptr<net::URLRequest> https_image_request =
      CreateURLRequest(GURL("https://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_TRUE(ResourcePrefetchPredictor::ShouldRecordResponse(
      https_image_request.get()));

  std::unique_ptr<net::URLRequest> https_image_request_with_port =
      CreateURLRequest(GURL("https://www.google.com:666/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordResponse(
      https_image_request_with_port.get()));

  std::unique_ptr<net::URLRequest> file_image_request =
      CreateURLRequest(GURL("file://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, true);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordResponse(
      file_image_request.get()));

  // ResourceType.
  std::unique_ptr<net::URLRequest> sub_frame_request =
      CreateURLRequest(GURL("http://www.google.com/frame.html"), net::MEDIUM,
                       content::RESOURCE_TYPE_SUB_FRAME, true);
  EXPECT_FALSE(
      ResourcePrefetchPredictor::ShouldRecordResponse(sub_frame_request.get()));

  std::unique_ptr<net::URLRequest> font_request =
      CreateURLRequest(GURL("http://www.google.com/comic-sans-ms.woff"),
                       net::MEDIUM, content::RESOURCE_TYPE_FONT_RESOURCE, true);
  EXPECT_TRUE(
      ResourcePrefetchPredictor::ShouldRecordResponse(font_request.get()));

  // From MIME Type.
  url_request_job_factory_.set_mime_type("image/png");
  std::unique_ptr<net::URLRequest> prefetch_image_request =
      CreateURLRequest(GURL("http://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_PREFETCH, true);
  EXPECT_TRUE(ResourcePrefetchPredictor::ShouldRecordResponse(
      prefetch_image_request.get()));

  url_request_job_factory_.set_mime_type("image/my-wonderful-format");
  std::unique_ptr<net::URLRequest> prefetch_unknown_image_request =
      CreateURLRequest(GURL("http://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_PREFETCH, true);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordResponse(
      prefetch_unknown_image_request.get()));

  url_request_job_factory_.set_mime_type("font/woff");
  std::unique_ptr<net::URLRequest> prefetch_font_request =
      CreateURLRequest(GURL("http://www.google.com/comic-sans-ms.woff"),
                       net::MEDIUM, content::RESOURCE_TYPE_PREFETCH, true);
  EXPECT_TRUE(ResourcePrefetchPredictor::ShouldRecordResponse(
      prefetch_font_request.get()));

  url_request_job_factory_.set_mime_type("font/woff-woff");
  std::unique_ptr<net::URLRequest> prefetch_unknown_font_request =
      CreateURLRequest(GURL("http://www.google.com/comic-sans-ms.woff"),
                       net::MEDIUM, content::RESOURCE_TYPE_PREFETCH, true);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordResponse(
      prefetch_unknown_font_request.get()));

  // Not main frame.
  std::unique_ptr<net::URLRequest> font_request_sub_frame = CreateURLRequest(
      GURL("http://www.google.com/comic-sans-ms.woff"), net::MEDIUM,
      content::RESOURCE_TYPE_FONT_RESOURCE, false);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordResponse(
      font_request_sub_frame.get()));
}

TEST_F(ResourcePrefetchPredictorTest, SummarizeResponse) {
  net::HttpResponseInfo response_info;
  response_info.headers =
      MakeResponseHeaders("HTTP/1.1 200 OK\n\nSome: Headers\n");
  response_info.was_cached = true;
  url_request_job_factory_.set_response_info(response_info);

  GURL url("http://www.google.com/cat.png");
  std::unique_ptr<net::URLRequest> request =
      CreateURLRequest(url, net::MEDIUM, content::RESOURCE_TYPE_IMAGE, true);
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

  std::unique_ptr<net::URLRequest> request =
      CreateURLRequest(GURL("http://www.google.com/cat.png"), net::MEDIUM,
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

  std::unique_ptr<net::URLRequest> request_no_validators =
      CreateURLRequest(GURL("http://www.google.com/cat.png"), net::MEDIUM,
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
  std::unique_ptr<net::URLRequest> request_etag =
      CreateURLRequest(GURL("http://www.google.com/cat.png"), net::MEDIUM,
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

  PrefetchDataMap test_data;
  test_data.insert(std::make_pair(google.primary_key(), google));
  test_data.insert(std::make_pair(twitter.primary_key(), twitter));
  test_data.insert(std::make_pair(nyt.primary_key(), nyt));

  std::vector<GURL> urls;
  EXPECT_TRUE(predictor_->PopulatePrefetcherRequest(google.primary_key(),
                                                    test_data, &urls));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL("http://google.com/image1.png"),
                                         GURL("http://google.com/style1.css")));

  urls.clear();
  EXPECT_FALSE(predictor_->PopulatePrefetcherRequest(nyt.primary_key(),
                                                     test_data, &urls));
  EXPECT_TRUE(urls.empty());

  urls.clear();
  EXPECT_FALSE(predictor_->PopulatePrefetcherRequest("http://404.com",
                                                     test_data, &urls));
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

  predictor_->manifest_table_cache_->insert({"google.com", google});
  predictor_->manifest_table_cache_->insert({"facebook.com", facebook});

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

  // The data to be sure that other RedirectData won't be affected.
  RedirectData gogle = CreateRedirectData("http://gogle.com", 2);
  InitializeRedirectStat(gogle.add_redirect_endpoints(), "https://google.com",
                         100, 0, 0);

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

  RedirectDataMap data_map;
  data_map.insert(std::make_pair(nyt.primary_key(), nyt));
  data_map.insert(std::make_pair(gogle.primary_key(), gogle));
  data_map.insert(std::make_pair(facebook.primary_key(), facebook));
  data_map.insert(std::make_pair(google.primary_key(), google));

  std::string redirect_endpoint;
  EXPECT_TRUE(predictor_->GetRedirectEndpoint("http://nyt.com", data_map,
                                              &redirect_endpoint));
  EXPECT_EQ(redirect_endpoint, "https://mobile.nytimes.com");

  // Returns the initial url if data_map doesn't contain an entry for the url.
  EXPECT_TRUE(predictor_->GetRedirectEndpoint("http://bbc.com", data_map,
                                              &redirect_endpoint));
  EXPECT_EQ(redirect_endpoint, "http://bbc.com");

  EXPECT_FALSE(predictor_->GetRedirectEndpoint("http://fb.com", data_map,
                                               &redirect_endpoint));
  EXPECT_FALSE(predictor_->GetRedirectEndpoint("http://google.com", data_map,
                                               &redirect_endpoint));
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
  predictor_->manifest_table_cache_->insert({"google.com", manifest});

  urls.clear();
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &prediction));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(resource_url)));

  // Add a resource associated with the main frame host.
  PrefetchData google_host = CreatePrefetchData("google.com", 2);
  const std::string script_url = "https://cdn.google.com/script.js";
  InitializeResourceData(google_host.add_resources(), script_url,
                         content::RESOURCE_TYPE_SCRIPT, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->host_table_cache_->insert(
      std::make_pair(google_host.primary_key(), google_host));

  urls.clear();
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &prediction));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(script_url)));

  // Add host-based redirect.
  RedirectData host_redirect = CreateRedirectData("google.com", 3);
  InitializeRedirectStat(host_redirect.add_redirect_endpoints(),
                         "www.google.fr", 10, 0, 0);
  predictor_->host_redirect_table_cache_->insert(
      std::make_pair(host_redirect.primary_key(), host_redirect));

  // Prediction failed: no data associated with the host redirect endpoint.
  urls.clear();
  EXPECT_FALSE(predictor_->GetPrefetchData(main_frame_url, &prediction));

  // Add a resource associated with host redirect endpoint.
  PrefetchData www_google_host = CreatePrefetchData("www.google.fr", 4);
  const std::string style_url = "https://cdn.google.com/style.css";
  InitializeResourceData(www_google_host.add_resources(), style_url,
                         content::RESOURCE_TYPE_STYLESHEET, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->host_table_cache_->insert(
      std::make_pair(www_google_host.primary_key(), www_google_host));

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
  predictor_->url_table_cache_->insert(
      std::make_pair(google_url.primary_key(), google_url));

  urls.clear();
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &prediction));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(image_url)));

  // Add url-based redirect.
  RedirectData url_redirect =
      CreateRedirectData("http://google.com/?query=cats", 6);
  InitializeRedirectStat(url_redirect.add_redirect_endpoints(),
                         "https://www.google.com/?query=cats", 10, 0, 0);
  predictor_->url_redirect_table_cache_->insert(
      std::make_pair(url_redirect.primary_key(), url_redirect));

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
  predictor_->url_table_cache_->insert(
      std::make_pair(www_google_url.primary_key(), www_google_url));

  urls.clear();
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &prediction));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(font_url)));
}

TEST_F(ResourcePrefetchPredictorTest, TestPrecisionRecallHistograms) {
  using testing::_;
  EXPECT_CALL(*mock_tables_.get(), UpdateResourceData(_, _));
  EXPECT_CALL(*mock_tables_.get(), UpdateRedirectData(_, _));
  EXPECT_CALL(*mock_tables_.get(), UpdateOriginData(_));

  // Fill the database with 3 resources: 1 useful, 2 useless.
  const std::string main_frame_url = "http://google.com/?query=cats";
  PrefetchData google = CreatePrefetchData("google.com", 1);

  const std::string script_url = "https://cdn.google.com/script.js";
  InitializeResourceData(google.add_resources(), script_url,
                         content::RESOURCE_TYPE_SCRIPT, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  InitializeResourceData(google.add_resources(), script_url + "foo",
                         content::RESOURCE_TYPE_SCRIPT, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  InitializeResourceData(google.add_resources(), script_url + "bar",
                         content::RESOURCE_TYPE_SCRIPT, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->host_table_cache_->insert(
      std::make_pair(google.primary_key(), google));

  ResourcePrefetchPredictor::Prediction prediction;
  EXPECT_TRUE(predictor_->GetPrefetchData(GURL(main_frame_url), &prediction));

  // Simulate a navigation with 2 resources, one we know, one we don't.
  URLRequestSummary main_frame = CreateURLRequestSummary(1, main_frame_url);
  predictor_->RecordURLRequest(main_frame);

  URLRequestSummary script = CreateURLRequestSummary(
      1, main_frame_url, script_url, content::RESOURCE_TYPE_SCRIPT);
  predictor_->RecordURLResponse(script);

  URLRequestSummary new_script = CreateURLRequestSummary(
      1, main_frame_url, script_url + "2", content::RESOURCE_TYPE_SCRIPT);
  predictor_->RecordURLResponse(new_script);

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();

  histogram_tester_->ExpectBucketCount(
      internal::kResourcePrefetchPredictorRecallHistogram, 50, 1);
  histogram_tester_->ExpectBucketCount(
      internal::kResourcePrefetchPredictorPrecisionHistogram, 33, 1);
  histogram_tester_->ExpectBucketCount(
      internal::kResourcePrefetchPredictorCountHistogram, 3, 1);
}

TEST_F(ResourcePrefetchPredictorTest, TestRedirectStatusNoRedirect) {
  TestRedirectStatusHistogram(
      "google.com", "google.com", "http://google.com?query=cats",
      "http://google.com?query=cats",
      ResourcePrefetchPredictor::RedirectStatus::NO_REDIRECT);
}

TEST_F(ResourcePrefetchPredictorTest,
       TestRedirectStatusNoRedirectButPredicted) {
  TestRedirectStatusHistogram(
      "google.com", "www.google.com", "http://google.com?query=cats",
      "http://google.com?query=cats",
      ResourcePrefetchPredictor::RedirectStatus::NO_REDIRECT_BUT_PREDICTED);
}

TEST_F(ResourcePrefetchPredictorTest, TestRedirectStatusRedirectNotPredicted) {
  TestRedirectStatusHistogram(
      "google.com", "google.com", "http://google.com?query=cats",
      "http://www.google.com?query=cats",
      ResourcePrefetchPredictor::RedirectStatus::REDIRECT_NOT_PREDICTED);
}

TEST_F(ResourcePrefetchPredictorTest,
       TestRedirectStatusRedirectWrongPredicted) {
  TestRedirectStatusHistogram(
      "google.com", "google.fr", "http://google.com?query=cats",
      "http://www.google.com?query=cats",
      ResourcePrefetchPredictor::RedirectStatus::REDIRECT_WRONG_PREDICTED);
}

TEST_F(ResourcePrefetchPredictorTest,
       TestRedirectStatusRedirectCorrectlyPredicted) {
  TestRedirectStatusHistogram(
      "google.com", "www.google.com", "http://google.com?query=cats",
      "http://www.google.com?query=cats",
      ResourcePrefetchPredictor::RedirectStatus::REDIRECT_CORRECTLY_PREDICTED);
}

TEST_F(ResourcePrefetchPredictorTest, TestPrefetchingDurationHistogram) {
  // Prefetching duration for an url without resources in the database
  // shouldn't be recorded.
  const std::string main_frame_url = "http://google.com/?query=cats";
  predictor_->StartPrefetching(GURL(main_frame_url), HintOrigin::EXTERNAL);
  predictor_->StopPrefetching(GURL(main_frame_url));
  histogram_tester_->ExpectTotalCount(
      internal::kResourcePrefetchPredictorPrefetchingDurationHistogram, 0);

  // Fill the database to record a duration.
  PrefetchData google = CreatePrefetchData("google.com", 1);
  InitializeResourceData(
      google.add_resources(), "https://cdn.google.com/script.js",
      content::RESOURCE_TYPE_SCRIPT, 10, 0, 1, 2.1, net::MEDIUM, false, false);
  predictor_->host_table_cache_->insert(
      std::make_pair(google.primary_key(), google));

  predictor_->StartPrefetching(GURL(main_frame_url), HintOrigin::EXTERNAL);
  predictor_->StopPrefetching(GURL(main_frame_url));
  histogram_tester_->ExpectTotalCount(
      internal::kResourcePrefetchPredictorPrefetchingDurationHistogram, 1);
}

TEST_F(ResourcePrefetchPredictorTest, TestRecordFirstContentfulPaint) {
  using testing::_;
  EXPECT_CALL(*mock_tables_.get(), UpdateRedirectData(_, _));
  EXPECT_CALL(*mock_tables_.get(), UpdateOriginData(_));

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
  EXPECT_CALL(*mock_tables_.get(),
              UpdateResourceData(host_data, PREFETCH_KEY_TYPE_HOST));

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();
}

}  // namespace predictors
