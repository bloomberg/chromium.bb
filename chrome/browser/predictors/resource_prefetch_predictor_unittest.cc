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
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
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
  int GetResponseCode() const override { return 200; }
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

  bool IsHandledURL(const GURL& url) const override { return true; }

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

  MOCK_METHOD4(GetAllData,
               void(PrefetchDataMap* url_data_map,
                    PrefetchDataMap* host_data_map,
                    RedirectDataMap* url_redirect_data_map,
                    RedirectDataMap* host_redirect_data_map));
  MOCK_METHOD4(UpdateData,
               void(const PrefetchData& url_data,
                    const PrefetchData& host_data,
                    const RedirectData& url_redirect_data,
                    const RedirectData& host_redirect_data));
  MOCK_METHOD2(DeleteResourceData,
               void(const std::vector<std::string>& urls,
                    const std::vector<std::string>& hosts));
  MOCK_METHOD2(DeleteSingleResourceDataPoint,
               void(const std::string& key, PrefetchKeyType key_type));
  MOCK_METHOD2(DeleteRedirectData,
               void(const std::vector<std::string>& urls,
                    const std::vector<std::string>& hosts));
  MOCK_METHOD2(DeleteSingleRedirectDataPoint,
               void(const std::string& key, PrefetchKeyType key_type));
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
      int process_id,
      int render_frame_id,
      const std::string& main_frame_url,
      const std::string& redirect_url) {
    URLRequestSummary summary =
        CreateURLRequestSummary(process_id, render_frame_id, main_frame_url);
    summary.redirect_url = GURL(redirect_url);
    return summary;
  }

  std::unique_ptr<net::URLRequest> CreateURLRequest(
      const GURL& url,
      net::RequestPriority priority,
      content::ResourceType resource_type,
      int render_process_id,
      int render_frame_id,
      bool is_main_frame) {
    std::unique_ptr<net::URLRequest> request =
        url_request_context_.CreateRequest(url, priority,
                                           &url_request_delegate_);
    request->set_first_party_for_cookies(url);
    content::ResourceRequestInfo::AllocateForTesting(
        request.get(), resource_type, nullptr, render_process_id, -1,
        render_frame_id, is_main_frame, false, false, true, false);
    request->Start();
    return request;
  }

  void InitializePredictor() {
    predictor_->StartInitialization();
    base::RunLoop loop;
    loop.RunUntilIdle();  // Runs the DB lookup.
    profile_->BlockUntilHistoryProcessesPendingRequests();
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
    ResourcePrefetchPredictorConfig config;
    config.max_urls_to_track = 3;
    config.max_hosts_to_track = 2;
    config.min_url_visit_count = 2;
    config.max_resources_per_entry = 4;
    config.max_consecutive_misses = 2;
    config.min_resource_confidence_to_trigger_prefetch = 0.5;

    // TODO(shishir): Enable the prefetching mode in the tests.
    config.mode |= ResourcePrefetchPredictorConfig::LEARNING;
    predictor_.reset(new ResourcePrefetchPredictor(config, profile_.get()));
    predictor_->set_mock_tables(mock_tables_);
  }

  void InitializeSampleData();

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  net::TestURLRequestContext url_request_context_;

  std::unique_ptr<ResourcePrefetchPredictor> predictor_;
  scoped_refptr<StrictMock<MockResourcePrefetchPredictorTables> > mock_tables_;

  PrefetchDataMap test_url_data_;
  PrefetchDataMap test_host_data_;
  RedirectDataMap test_url_redirect_data_;
  RedirectDataMap test_host_redirect_data_;
  PrefetchData empty_resource_data_;
  RedirectData empty_redirect_data_;

  MockURLRequestJobFactory url_request_job_factory_;
  EmptyURLRequestDelegate url_request_delegate_;
};

ResourcePrefetchPredictorTest::ResourcePrefetchPredictorTest()
    : thread_bundle_(),
      profile_(new TestingProfile()),
      mock_tables_(new StrictMock<MockResourcePrefetchPredictorTables>()),
      empty_resource_data_(),
      empty_redirect_data_() {}

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
                         Pointee(ContainerEq(RedirectDataMap()))));
  InitializePredictor();
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::INITIALIZED);

  url_request_context_.set_job_factory(&url_request_job_factory_);
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
}

// Tests that the history and the db tables data are loaded correctly.
TEST_F(ResourcePrefetchPredictorTest, LazilyInitializeWithData) {
  AddUrlToHistory("http://www.google.com/", 4);
  AddUrlToHistory("http://www.yahoo.com/", 2);

  EXPECT_CALL(*mock_tables_.get(),
              GetAllData(Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(RedirectDataMap())),
                         Pointee(ContainerEq(RedirectDataMap()))))
      .WillOnce(DoAll(SetArgPointee<0>(test_url_data_),
                      SetArgPointee<1>(test_host_data_),
                      SetArgPointee<2>(test_url_redirect_data_),
                      SetArgPointee<3>(test_host_redirect_data_)));

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
}

// Single navigation but history count is low, so should not record.
TEST_F(ResourcePrefetchPredictorTest, NavigationNotRecorded) {
  const int kVisitCount = 1;
  AddUrlToHistory("https://www.google.com", kVisitCount);

  URLRequestSummary main_frame =
      CreateURLRequestSummary(1, 1, "http://www.google.com");
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  URLRequestSummary main_frame_redirect = CreateRedirectRequestSummary(
      1, 1, "http://www.google.com", "https://www.google.com");
  predictor_->RecordURLRedirect(main_frame_redirect);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());
  main_frame = CreateURLRequestSummary(1, 1, "https://www.google.com");

  // Now add a few subresources.
  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, 1, "https://www.google.com", "https://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false);
  predictor_->RecordURLResponse(resource1);
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, 1, "https://www.google.com", "https://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  predictor_->RecordURLResponse(resource2);
  URLRequestSummary resource3 = CreateURLRequestSummary(
      1, 1, "https://www.google.com", "https://google.com/script2.js",
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
              UpdateData(empty_resource_data_, host_data, empty_redirect_data_,
                         empty_redirect_data_));

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();
}

// Single navigation that will be recorded. Will check for duplicate
// resources and also for number of resources saved.
TEST_F(ResourcePrefetchPredictorTest, NavigationUrlNotInDB) {
  const int kVisitCount = 4;
  AddUrlToHistory("http://www.google.com", kVisitCount);

  URLRequestSummary main_frame =
      CreateURLRequestSummary(1, 1, "http://www.google.com");
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  std::vector<URLRequestSummary> resources;
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", true));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/image1.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/image2.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/style2.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true));
  predictor_->RecordURLResponse(resources.back());

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
  InitializeResourceData(
      url_data.add_resources(), "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 2.0, net::MEDIUM, false, false);
  InitializeResourceData(
      url_data.add_resources(), "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 3.0, net::MEDIUM, false, false);
  InitializeResourceData(url_data.add_resources(),
                         "http://google.com/style2.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 7.0,
                         net::MEDIUM, false, false);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateData(url_data, empty_resource_data_, empty_redirect_data_,
                         empty_redirect_data_));

  PrefetchData host_data = CreatePrefetchData("www.google.com");
  host_data.mutable_resources()->CopyFrom(url_data.resources());
  EXPECT_CALL(*mock_tables_.get(),
              UpdateData(empty_resource_data_, host_data, empty_redirect_data_,
                         empty_redirect_data_));

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
                         Pointee(ContainerEq(RedirectDataMap()))))
      .WillOnce(DoAll(SetArgPointee<0>(test_url_data_),
                      SetArgPointee<1>(test_host_data_)));
  ResetPredictor();
  InitializePredictor();
  EXPECT_EQ(3U, predictor_->url_table_cache_->size());
  EXPECT_EQ(2U, predictor_->host_table_cache_->size());

  URLRequestSummary main_frame = CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  std::vector<URLRequestSummary> resources;
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", true));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/image1.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/image2.png",
      content::RESOURCE_TYPE_IMAGE, net::MEDIUM, "image/png", false));
  predictor_->RecordURLResponse(resources.back());
  resources.push_back(CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/style2.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", true));
  predictor_->RecordURLResponse(resources.back());

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
  InitializeResourceData(
      url_data.add_resources(), "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 2.0, net::MEDIUM, false, false);
  InitializeResourceData(
      url_data.add_resources(), "http://google.com/script4.js",
      content::RESOURCE_TYPE_SCRIPT, 11, 1, 1, 2.1, net::MEDIUM, false, false);
  InitializeResourceData(
      url_data.add_resources(), "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 3.0, net::MEDIUM, false, false);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateData(url_data, empty_resource_data_, empty_redirect_data_,
                         empty_redirect_data_));
  EXPECT_CALL(*mock_tables_.get(),
              DeleteSingleResourceDataPoint("www.facebook.com",
                                            PREFETCH_KEY_TYPE_HOST));

  PrefetchData host_data = CreatePrefetchData("www.google.com");
  InitializeResourceData(host_data.add_resources(),
                         "http://google.com/style1.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 1.0,
                         net::MEDIUM, false, false);
  InitializeResourceData(
      host_data.add_resources(), "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 2.0, net::MEDIUM, false, false);
  InitializeResourceData(
      host_data.add_resources(), "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, 1, 0, 0, 3.0, net::MEDIUM, false, false);
  InitializeResourceData(host_data.add_resources(),
                         "http://google.com/style2.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 7.0,
                         net::MEDIUM, false, false);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateData(empty_resource_data_, host_data, empty_redirect_data_,
                         empty_redirect_data_));

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
                         Pointee(ContainerEq(RedirectDataMap()))))
      .WillOnce(DoAll(SetArgPointee<0>(test_url_data_),
                      SetArgPointee<1>(test_host_data_)));
  ResetPredictor();
  InitializePredictor();
  EXPECT_EQ(3U, predictor_->url_table_cache_->size());
  EXPECT_EQ(2U, predictor_->host_table_cache_->size());

  URLRequestSummary main_frame = CreateURLRequestSummary(
      1, 1, "http://www.nike.com", "http://www.nike.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  predictor_->RecordURLRequest(main_frame);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  URLRequestSummary resource1 = CreateURLRequestSummary(
      1, 1, "http://www.nike.com", "http://nike.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false);
  predictor_->RecordURLResponse(resource1);
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, 1, "http://www.nike.com", "http://nike.com/image2.png",
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

  PrefetchData url_data = CreatePrefetchData("http://www.nike.com/");
  InitializeResourceData(url_data.add_resources(), "http://nike.com/style1.css",
                         content::RESOURCE_TYPE_STYLESHEET, 1, 0, 0, 1.0,
                         net::MEDIUM, false, false);
  InitializeResourceData(url_data.add_resources(), "http://nike.com/image2.png",
                         content::RESOURCE_TYPE_IMAGE, 1, 0, 0, 2.0,
                         net::MEDIUM, false, false);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateData(url_data, empty_resource_data_, empty_redirect_data_,
                         empty_redirect_data_));

  PrefetchData host_data = CreatePrefetchData("www.nike.com");
  host_data.mutable_resources()->CopyFrom(url_data.resources());
  EXPECT_CALL(*mock_tables_.get(),
              UpdateData(empty_resource_data_, host_data, empty_redirect_data_,
                         empty_redirect_data_));

  predictor_->RecordMainFrameLoadComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();
}

TEST_F(ResourcePrefetchPredictorTest, RedirectUrlNotInDB) {
  const int kVisitCount = 4;
  AddUrlToHistory("https://facebook.com/google", kVisitCount);

  URLRequestSummary fb1 = CreateURLRequestSummary(1, 1, "http://fb.com/google");
  predictor_->RecordURLRequest(fb1);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  URLRequestSummary fb2 = CreateRedirectRequestSummary(
      1, 1, "http://fb.com/google", "http://facebook.com/google");
  predictor_->RecordURLRedirect(fb2);
  URLRequestSummary fb3 = CreateRedirectRequestSummary(
      1, 1, "http://facebook.com/google", "https://facebook.com/google");
  predictor_->RecordURLRedirect(fb3);
  NavigationID fb_end = CreateNavigationID(1, 1, "https://facebook.com/google");

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
              UpdateData(empty_resource_data_, empty_resource_data_,
                         url_redirect_data, empty_redirect_data_));

  RedirectData host_redirect_data = CreateRedirectData("fb.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "facebook.com", 1, 0, 0);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateData(empty_resource_data_, empty_resource_data_,
                         empty_redirect_data_, host_redirect_data));

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
                         Pointee(ContainerEq(RedirectDataMap()))))
      .WillOnce(DoAll(SetArgPointee<2>(test_url_redirect_data_),
                      SetArgPointee<3>(test_host_redirect_data_)));
  ResetPredictor();
  InitializePredictor();
  EXPECT_EQ(3U, predictor_->url_redirect_table_cache_->size());
  EXPECT_EQ(2U, predictor_->host_redirect_table_cache_->size());

  URLRequestSummary fb1 = CreateURLRequestSummary(1, 1, "http://fb.com/google");
  predictor_->RecordURLRequest(fb1);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  URLRequestSummary fb2 = CreateRedirectRequestSummary(
      1, 1, "http://fb.com/google", "http://facebook.com/google");
  predictor_->RecordURLRedirect(fb2);
  URLRequestSummary fb3 = CreateRedirectRequestSummary(
      1, 1, "http://facebook.com/google", "https://facebook.com/google");
  predictor_->RecordURLRedirect(fb3);
  NavigationID fb_end = CreateNavigationID(1, 1, "https://facebook.com/google");

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
              UpdateData(empty_resource_data_, empty_resource_data_,
                         url_redirect_data, empty_redirect_data_));

  RedirectData host_redirect_data = CreateRedirectData("fb.com");
  InitializeRedirectStat(host_redirect_data.add_redirect_endpoints(),
                         "facebook.com", 1, 0, 0);
  EXPECT_CALL(*mock_tables_.get(),
              UpdateData(empty_resource_data_, empty_resource_data_,
                         empty_redirect_data_, host_redirect_data));

  predictor_->RecordMainFrameLoadComplete(fb_end);
  profile_->BlockUntilHistoryProcessesPendingRequests();
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

  history::URLRows rows;
  rows.push_back(history::URLRow(GURL("http://www.google.com/page2.html")));
  rows.push_back(history::URLRow(GURL("http://www.apple.com")));
  rows.push_back(history::URLRow(GURL("http://www.nike.com")));

  std::vector<std::string> urls_to_delete, hosts_to_delete,
      url_redirects_to_delete, host_redirects_to_delete;
  urls_to_delete.push_back("http://www.google.com/page2.html");
  urls_to_delete.push_back("http://www.apple.com/");
  urls_to_delete.push_back("http://www.nike.com/");
  hosts_to_delete.push_back("www.google.com");
  hosts_to_delete.push_back("www.apple.com");
  url_redirects_to_delete.push_back("http://www.google.com/page2.html");
  url_redirects_to_delete.push_back("http://www.apple.com/");
  host_redirects_to_delete.push_back("www.google.com");
  host_redirects_to_delete.push_back("www.nike.com");

  EXPECT_CALL(*mock_tables_.get(),
              DeleteResourceData(ContainerEq(urls_to_delete),
                                 ContainerEq(hosts_to_delete)));
  EXPECT_CALL(*mock_tables_.get(),
              DeleteRedirectData(ContainerEq(url_redirects_to_delete),
                                 ContainerEq(host_redirects_to_delete)));

  predictor_->DeleteUrls(rows);
  EXPECT_EQ(2U, predictor_->url_table_cache_->size());
  EXPECT_EQ(1U, predictor_->host_table_cache_->size());
  EXPECT_EQ(2U, predictor_->url_redirect_table_cache_->size());
  EXPECT_EQ(1U, predictor_->host_redirect_table_cache_->size());

  EXPECT_CALL(*mock_tables_.get(), DeleteAllData());

  predictor_->DeleteAllUrls();
  EXPECT_TRUE(predictor_->url_table_cache_->empty());
  EXPECT_TRUE(predictor_->host_table_cache_->empty());
  EXPECT_TRUE(predictor_->url_redirect_table_cache_->empty());
  EXPECT_TRUE(predictor_->host_redirect_table_cache_->empty());
}

TEST_F(ResourcePrefetchPredictorTest, OnMainFrameRequest) {
  URLRequestSummary summary1 = CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  URLRequestSummary summary2 = CreateURLRequestSummary(
      1, 2, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  URLRequestSummary summary3 = CreateURLRequestSummary(
      2, 1, "http://www.yahoo.com", "http://www.yahoo.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);

  predictor_->OnMainFrameRequest(summary1);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());
  predictor_->OnMainFrameRequest(summary2);
  EXPECT_EQ(2U, predictor_->inflight_navigations_.size());
  predictor_->OnMainFrameRequest(summary3);
  EXPECT_EQ(3U, predictor_->inflight_navigations_.size());

  // Insert another with same navigation id. It should replace.
  URLRequestSummary summary4 = CreateURLRequestSummary(
      1, 1, "http://www.nike.com", "http://www.nike.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  URLRequestSummary summary5 = CreateURLRequestSummary(
      1, 2, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);

  predictor_->OnMainFrameRequest(summary4);
  EXPECT_EQ(3U, predictor_->inflight_navigations_.size());

  // Change this creation time so that it will go away on the next insert.
  summary5.navigation_id.creation_time = base::TimeTicks::Now() -
      base::TimeDelta::FromDays(1);
  predictor_->OnMainFrameRequest(summary5);
  EXPECT_EQ(3U, predictor_->inflight_navigations_.size());

  URLRequestSummary summary6 = CreateURLRequestSummary(
      3, 1, "http://www.shoes.com", "http://www.shoes.com",
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
  URLRequestSummary yahoo = CreateURLRequestSummary(1, 1, "http://yahoo.com");

  URLRequestSummary bbc1 = CreateURLRequestSummary(2, 2, "http://bbc.com");
  URLRequestSummary bbc2 = CreateRedirectRequestSummary(2, 2, "http://bbc.com",
                                                        "https://www.bbc.com");
  NavigationID bbc_end = CreateNavigationID(2, 2, "https://www.bbc.com");

  URLRequestSummary youtube1 =
      CreateURLRequestSummary(1, 2, "http://youtube.com");
  URLRequestSummary youtube2 = CreateRedirectRequestSummary(
      1, 2, "http://youtube.com", "https://youtube.com");
  NavigationID youtube_end = CreateNavigationID(1, 2, "https://youtube.com");

  URLRequestSummary nyt1 = CreateURLRequestSummary(2, 1, "http://nyt.com");
  URLRequestSummary nyt2 = CreateRedirectRequestSummary(2, 1, "http://nyt.com",
                                                        "http://nytimes.com");
  URLRequestSummary nyt3 = CreateRedirectRequestSummary(
      2, 1, "http://nytimes.com", "http://m.nytimes.com");
  NavigationID nyt_end = CreateNavigationID(2, 1, "http://m.nytimes.com");

  URLRequestSummary fb1 = CreateURLRequestSummary(1, 3, "http://fb.com");
  URLRequestSummary fb2 = CreateRedirectRequestSummary(1, 3, "http://fb.com",
                                                       "http://facebook.com");
  URLRequestSummary fb3 = CreateRedirectRequestSummary(
      1, 3, "http://facebook.com", "https://facebook.com");
  URLRequestSummary fb4 = CreateRedirectRequestSummary(
      1, 3, "https://facebook.com",
      "https://m.facebook.com/?refsrc=https%3A%2F%2Fwww.facebook.com%2F&_rdr");
  NavigationID fb_end = CreateNavigationID(
      1, 3,
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
      1, 1, "http://www.google.com", "http://google.com/style1.css",
      content::RESOURCE_TYPE_STYLESHEET, net::MEDIUM, "text/css", false);
  predictor_->OnSubresourceResponse(resource1);
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());

  // Add an inflight navigation.
  URLRequestSummary main_frame1 = CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://www.google.com",
      content::RESOURCE_TYPE_MAIN_FRAME, net::MEDIUM, std::string(), false);
  predictor_->OnMainFrameRequest(main_frame1);
  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());

  // Now add a few subresources.
  URLRequestSummary resource2 = CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/script1.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  URLRequestSummary resource3 = CreateURLRequestSummary(
      1, 1, "http://www.google.com", "http://google.com/script2.js",
      content::RESOURCE_TYPE_SCRIPT, net::MEDIUM, "text/javascript", false);
  predictor_->OnSubresourceResponse(resource1);
  predictor_->OnSubresourceResponse(resource2);
  predictor_->OnSubresourceResponse(resource3);

  EXPECT_EQ(1U, predictor_->inflight_navigations_.size());
  EXPECT_EQ(3U, predictor_->inflight_navigations_[main_frame1.navigation_id]
                    ->subresource_requests.size());
  EXPECT_TRUE(URLRequestSummaryAreEqual(
      resource1, predictor_->inflight_navigations_[main_frame1.navigation_id]
                     ->subresource_requests[0]));
  EXPECT_TRUE(URLRequestSummaryAreEqual(
      resource2, predictor_->inflight_navigations_[main_frame1.navigation_id]
                     ->subresource_requests[1]));
  EXPECT_TRUE(URLRequestSummaryAreEqual(
      resource3, predictor_->inflight_navigations_[main_frame1.navigation_id]
                     ->subresource_requests[2]));
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
}

TEST_F(ResourcePrefetchPredictorTest, ShouldRecordRequestMainFrame) {
  std::unique_ptr<net::URLRequest> http_request =
      CreateURLRequest(GURL("http://www.google.com"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, 1, 1, true);
  EXPECT_TRUE(ResourcePrefetchPredictor::ShouldRecordRequest(
      http_request.get(), content::RESOURCE_TYPE_MAIN_FRAME));

  std::unique_ptr<net::URLRequest> https_request =
      CreateURLRequest(GURL("https://www.google.com"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, 1, 1, true);
  EXPECT_TRUE(ResourcePrefetchPredictor::ShouldRecordRequest(
      https_request.get(), content::RESOURCE_TYPE_MAIN_FRAME));

  std::unique_ptr<net::URLRequest> file_request =
      CreateURLRequest(GURL("file://www.google.com"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, 1, 1, true);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordRequest(
      file_request.get(), content::RESOURCE_TYPE_MAIN_FRAME));
}

TEST_F(ResourcePrefetchPredictorTest, ShouldRecordRequestSubResource) {
  std::unique_ptr<net::URLRequest> http_request =
      CreateURLRequest(GURL("http://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, 1, 1, false);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordRequest(
      http_request.get(), content::RESOURCE_TYPE_IMAGE));

  std::unique_ptr<net::URLRequest> https_request =
      CreateURLRequest(GURL("https://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, 1, 1, false);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordRequest(
      https_request.get(), content::RESOURCE_TYPE_IMAGE));

  std::unique_ptr<net::URLRequest> file_request =
      CreateURLRequest(GURL("file://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, 1, 1, false);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordRequest(
      file_request.get(), content::RESOURCE_TYPE_IMAGE));
}

TEST_F(ResourcePrefetchPredictorTest, ShouldRecordResponseMainFrame) {
  net::HttpResponseInfo response_info;
  response_info.headers = MakeResponseHeaders("");
  url_request_job_factory_.set_response_info(response_info);

  std::unique_ptr<net::URLRequest> http_request =
      CreateURLRequest(GURL("http://www.google.com"), net::MEDIUM,
                       content::RESOURCE_TYPE_MAIN_FRAME, 1, 1, true);
  EXPECT_TRUE(
      ResourcePrefetchPredictor::ShouldRecordResponse(http_request.get()));

  std::unique_ptr<net::URLRequest> https_request =
      CreateURLRequest(GURL("https://www.google.com"), net::MEDIUM,
                       content::RESOURCE_TYPE_MAIN_FRAME, 1, 1, true);
  EXPECT_TRUE(
      ResourcePrefetchPredictor::ShouldRecordResponse(https_request.get()));

  std::unique_ptr<net::URLRequest> file_request =
      CreateURLRequest(GURL("file://www.google.com"), net::MEDIUM,
                       content::RESOURCE_TYPE_MAIN_FRAME, 1, 1, true);
  EXPECT_FALSE(
      ResourcePrefetchPredictor::ShouldRecordResponse(file_request.get()));
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
                       content::RESOURCE_TYPE_IMAGE, 1, 1, true);
  EXPECT_TRUE(ResourcePrefetchPredictor::ShouldRecordResponse(
      http_image_request.get()));

  std::unique_ptr<net::URLRequest> https_image_request =
      CreateURLRequest(GURL("https://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, 1, 1, true);
  EXPECT_TRUE(ResourcePrefetchPredictor::ShouldRecordResponse(
      https_image_request.get()));

  std::unique_ptr<net::URLRequest> file_image_request =
      CreateURLRequest(GURL("file://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_IMAGE, 1, 1, true);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordResponse(
      file_image_request.get()));

  // ResourceType.
  std::unique_ptr<net::URLRequest> sub_frame_request =
      CreateURLRequest(GURL("http://www.google.com/frame.html"), net::MEDIUM,
                       content::RESOURCE_TYPE_SUB_FRAME, 1, 1, true);
  EXPECT_FALSE(
      ResourcePrefetchPredictor::ShouldRecordResponse(sub_frame_request.get()));

  std::unique_ptr<net::URLRequest> font_request = CreateURLRequest(
      GURL("http://www.google.com/comic-sans-ms.woff"), net::MEDIUM,
      content::RESOURCE_TYPE_FONT_RESOURCE, 1, 1, true);
  EXPECT_TRUE(
      ResourcePrefetchPredictor::ShouldRecordResponse(font_request.get()));

  // From MIME Type.
  url_request_job_factory_.set_mime_type("image/png");
  std::unique_ptr<net::URLRequest> prefetch_image_request =
      CreateURLRequest(GURL("http://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_PREFETCH, 1, 1, true);
  EXPECT_TRUE(ResourcePrefetchPredictor::ShouldRecordResponse(
      prefetch_image_request.get()));

  url_request_job_factory_.set_mime_type("image/my-wonderful-format");
  std::unique_ptr<net::URLRequest> prefetch_unknown_image_request =
      CreateURLRequest(GURL("http://www.google.com/cat.png"), net::MEDIUM,
                       content::RESOURCE_TYPE_PREFETCH, 1, 1, true);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordResponse(
      prefetch_unknown_image_request.get()));

  url_request_job_factory_.set_mime_type("font/woff");
  std::unique_ptr<net::URLRequest> prefetch_font_request = CreateURLRequest(
      GURL("http://www.google.com/comic-sans-ms.woff"), net::MEDIUM,
      content::RESOURCE_TYPE_PREFETCH, 1, 1, true);
  EXPECT_TRUE(ResourcePrefetchPredictor::ShouldRecordResponse(
      prefetch_font_request.get()));

  url_request_job_factory_.set_mime_type("font/woff-woff");
  std::unique_ptr<net::URLRequest> prefetch_unknown_font_request =
      CreateURLRequest(GURL("http://www.google.com/comic-sans-ms.woff"),
                       net::MEDIUM, content::RESOURCE_TYPE_PREFETCH, 1, 1,
                       true);
  EXPECT_FALSE(ResourcePrefetchPredictor::ShouldRecordResponse(
      prefetch_unknown_font_request.get()));

  // Not main frame.
  std::unique_ptr<net::URLRequest> font_request_sub_frame = CreateURLRequest(
      GURL("http://www.google.com/comic-sans-ms.woff"), net::MEDIUM,
      content::RESOURCE_TYPE_FONT_RESOURCE, 1, 1, false);
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
  std::unique_ptr<net::URLRequest> request = CreateURLRequest(
      url, net::MEDIUM, content::RESOURCE_TYPE_IMAGE, 1, 1, true);
  URLRequestSummary summary;
  EXPECT_TRUE(URLRequestSummary::SummarizeResponse(*request, &summary));
  EXPECT_EQ(1, summary.navigation_id.render_process_id);
  EXPECT_EQ(1, summary.navigation_id.render_frame_id);
  EXPECT_EQ(url, summary.navigation_id.main_frame_url);
  EXPECT_EQ(url, summary.resource_url);
  EXPECT_EQ(content::RESOURCE_TYPE_IMAGE, summary.resource_type);
  EXPECT_TRUE(summary.was_cached);
  EXPECT_FALSE(summary.has_validators);
  EXPECT_FALSE(summary.always_revalidate);
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
                       content::RESOURCE_TYPE_PREFETCH, 1, 1, true);
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
                       content::RESOURCE_TYPE_PREFETCH, 1, 1, true);

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
                       content::RESOURCE_TYPE_PREFETCH, 1, 1, true);
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

TEST_F(ResourcePrefetchPredictorTest, GetRedirectEndpoint) {
  // The data to be requested for the most confident endpoint.
  RedirectData google = CreateRedirectData("http://google.com/", 1);
  InitializeRedirectStat(google.add_redirect_endpoints(), "https://google.com",
                         10, 0, 0);
  InitializeRedirectStat(google.add_redirect_endpoints(), "https://google.fr",
                         10, 1, 0);
  InitializeRedirectStat(google.add_redirect_endpoints(), "https://google.ws",
                         20, 20, 0);

  // The data to be sure that other RedirectData won't be affected.
  RedirectData gogle = CreateRedirectData("http://gogle.com", 2);
  InitializeRedirectStat(gogle.add_redirect_endpoints(), "https://google.com",
                         100, 0, 0);

  // The data to check negative result.
  RedirectData facebook = CreateRedirectData("http://fb.com/", 3);
  InitializeRedirectStat(facebook.add_redirect_endpoints(),
                         "http://facebook.com", 5, 5,
                         0);  // not enough confidence

  RedirectDataMap data_map;
  data_map.insert(std::make_pair(google.primary_key(), google));
  data_map.insert(std::make_pair(gogle.primary_key(), gogle));
  data_map.insert(std::make_pair(facebook.primary_key(), facebook));

  std::string redirect_endpoint;
  EXPECT_TRUE(predictor_->GetRedirectEndpoint("http://google.com/", data_map,
                                              &redirect_endpoint));
  EXPECT_EQ(redirect_endpoint, "https://google.com");

  EXPECT_FALSE(predictor_->GetRedirectEndpoint("http://fb.com", data_map,
                                               &redirect_endpoint));
  EXPECT_FALSE(predictor_->GetRedirectEndpoint("http://404.com", data_map,
                                               &redirect_endpoint));
}

TEST_F(ResourcePrefetchPredictorTest, GetPrefetchData) {
  const GURL main_frame_url("http://google.com/?query=cats");
  std::vector<GURL> urls;
  // No prefetch data.
  EXPECT_FALSE(predictor_->GetPrefetchData(main_frame_url, &urls));

  // Add a resource associated with the main frame host.
  PrefetchData google_host = CreatePrefetchData("google.com", 1);
  const std::string script_url = "https://cdn.google.com/script.js";
  InitializeResourceData(google_host.add_resources(), script_url,
                         content::RESOURCE_TYPE_SCRIPT, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->host_table_cache_->insert(
      std::make_pair(google_host.primary_key(), google_host));

  urls.clear();
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &urls));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(script_url)));

  // Add a resource associated with the main frame url.
  PrefetchData google_url =
      CreatePrefetchData("http://google.com/?query=cats", 2);
  const std::string image_url = "https://cdn.google.com/image.png";
  InitializeResourceData(google_url.add_resources(), image_url,
                         content::RESOURCE_TYPE_IMAGE, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->url_table_cache_->insert(
      std::make_pair(google_url.primary_key(), google_url));

  urls.clear();
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &urls));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(image_url)));

  // Add host-based redirect.
  RedirectData host_redirect = CreateRedirectData("google.com", 3);
  InitializeRedirectStat(host_redirect.add_redirect_endpoints(),
                         "www.google.com", 10, 0, 0);
  predictor_->host_redirect_table_cache_->insert(
      std::make_pair(host_redirect.primary_key(), host_redirect));

  // Nothing changed: new redirect endpoint doesn't have any associated
  // resources
  urls.clear();
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &urls));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(image_url)));

  // Add a resource associated with host redirect endpoint.
  PrefetchData www_google_host = CreatePrefetchData("www.google.com", 4);
  const std::string style_url = "https://cdn.google.com/style.css";
  InitializeResourceData(www_google_host.add_resources(), style_url,
                         content::RESOURCE_TYPE_STYLESHEET, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->host_table_cache_->insert(
      std::make_pair(www_google_host.primary_key(), www_google_host));

  urls.clear();
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &urls));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(style_url)));

  // Add url-based redirect.
  RedirectData url_redirect =
      CreateRedirectData("http://google.com/?query=cats", 5);
  InitializeRedirectStat(url_redirect.add_redirect_endpoints(),
                         "https://www.google.com/?query=cats", 10, 0, 0);
  predictor_->url_redirect_table_cache_->insert(
      std::make_pair(url_redirect.primary_key(), url_redirect));

  // Url redirect endpoint doesn't have associated resources.
  urls.clear();
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &urls));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(style_url)));

  // Add a resource associated with url redirect endpoint.
  PrefetchData www_google_url =
      CreatePrefetchData("https://www.google.com/?query=cats", 4);
  const std::string font_url = "https://cdn.google.com/comic-sans-ms.woff";
  InitializeResourceData(www_google_url.add_resources(), font_url,
                         content::RESOURCE_TYPE_FONT_RESOURCE, 10, 0, 1, 2.1,
                         net::MEDIUM, false, false);
  predictor_->url_table_cache_->insert(
      std::make_pair(www_google_url.primary_key(), www_google_url));

  urls.clear();
  EXPECT_TRUE(predictor_->GetPrefetchData(main_frame_url, &urls));
  EXPECT_THAT(urls, UnorderedElementsAre(GURL(font_url)));
}

}  // namespace predictors
