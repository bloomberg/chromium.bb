// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/time.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
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
using testing::StrictMock;

namespace predictors {

typedef ResourcePrefetchPredictor::URLRequestSummary URLRequestSummary;
typedef ResourcePrefetchPredictorTables::ResourceRow ResourceRow;
typedef std::vector<ResourceRow> ResourceRows;
typedef ResourcePrefetchPredictorTables::PrefetchData PrefetchData;
typedef ResourcePrefetchPredictorTables::PrefetchDataMap PrefetchDataMap;

// For printing failures nicely.
void PrintTo(const ResourceRow& row, ::std::ostream* os) {
  *os << "[" << row.primary_key << "," << row.resource_url
      << "," << row.resource_type << "," << row.number_of_hits
      << "," << row.number_of_misses << "," << row.consecutive_misses
      << "," << row.average_position << "," << row.score << "]";
}

void PrintTo(const PrefetchData& data, ::std::ostream* os) {
  *os << "[" << data.key_type << "," << data.primary_key
      << "," << data.last_visit.ToInternalValue() << "]\n";
  for (ResourceRows::const_iterator it = data.resources.begin();
       it != data.resources.end(); ++it) {
    *os << "\t\t";
    PrintTo(*it, os);
    *os << "\n";
  }
}

class MockResourcePrefetchPredictorTables
    : public ResourcePrefetchPredictorTables {
 public:
  MockResourcePrefetchPredictorTables() { }

  MOCK_METHOD2(GetAllData, void(PrefetchDataMap* url_data_map,
                                PrefetchDataMap* host_data_map));
  MOCK_METHOD2(UpdateData, void(const PrefetchData& url_data,
                                const PrefetchData& host_data));
  MOCK_METHOD2(DeleteData, void(const std::vector<std::string>& urls,
                                const std::vector<std::string>& hosts));
  MOCK_METHOD2(DeleteSingleDataPoint, void(const std::string& key,
                                           PrefetchKeyType key_type));
  MOCK_METHOD0(DeleteAllData, void());

 protected:
  ~MockResourcePrefetchPredictorTables() { }
};

class ResourcePrefetchPredictorTest : public testing::Test {
 public:
  ResourcePrefetchPredictorTest();
  ~ResourcePrefetchPredictorTest();
  void SetUp() OVERRIDE;
  void TearDown() OVERRIDE;

 protected:
  void AddUrlToHistory(const std::string& url, int visit_count) {
    HistoryServiceFactory::GetForProfile(profile_.get(),
                                         Profile::EXPLICIT_ACCESS)->
        AddPageWithDetails(
            GURL(url),
            string16(),
            visit_count,
            0,
            base::Time::Now(),
            false,
            history::SOURCE_BROWSED);
    profile_->BlockUntilHistoryProcessesPendingRequests();
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

    // TODO(shishir): Enable the prefetching mode in the tests.
    config.mode |= ResourcePrefetchPredictorConfig::URL_LEARNING;
    config.mode |= ResourcePrefetchPredictorConfig::HOST_LEARNING;
    predictor_.reset(new ResourcePrefetchPredictor(config, profile_.get()));
    predictor_->set_mock_tables(mock_tables_);
  }

  void InitializeSampleData();

  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  scoped_ptr<TestingProfile> profile_;

  scoped_ptr<ResourcePrefetchPredictor> predictor_;
  scoped_refptr<StrictMock<MockResourcePrefetchPredictorTables> > mock_tables_;

  PrefetchDataMap test_url_data_;
  PrefetchDataMap test_host_data_;
  PrefetchData empty_url_data_;
  PrefetchData empty_host_data_;
};

ResourcePrefetchPredictorTest::ResourcePrefetchPredictorTest()
    : loop_(MessageLoop::TYPE_DEFAULT),
      ui_thread_(content::BrowserThread::UI, &loop_),
      db_thread_(content::BrowserThread::DB, &loop_),
      profile_(new TestingProfile()),
      predictor_(NULL),
      mock_tables_(new StrictMock<MockResourcePrefetchPredictorTables>()),
      empty_url_data_(PREFETCH_KEY_TYPE_URL, ""),
      empty_host_data_(PREFETCH_KEY_TYPE_HOST, "") {
}

ResourcePrefetchPredictorTest::~ResourcePrefetchPredictorTest() {
  profile_.reset(NULL);
  loop_.RunUntilIdle();
}

void ResourcePrefetchPredictorTest::SetUp() {
  InitializeSampleData();

  profile_->CreateHistoryService(true, false);
  profile_->BlockUntilHistoryProcessesPendingRequests();
  EXPECT_TRUE(HistoryServiceFactory::GetForProfile(profile_.get(),
                                                   Profile::EXPLICIT_ACCESS));
  // Initialize the predictor with empty data.
  ResetPredictor();
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::NOT_INITIALIZED);
  EXPECT_CALL(*mock_tables_,
              GetAllData(Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(PrefetchDataMap()))));
  InitializePredictor();
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::INITIALIZED);
}

void ResourcePrefetchPredictorTest::TearDown() {
  predictor_.reset(NULL);
  profile_->DestroyHistoryService();
}

void ResourcePrefetchPredictorTest::InitializeSampleData() {
  {  // Url data.
    PrefetchData google(PREFETCH_KEY_TYPE_URL, "http://www.google.com/");
    google.last_visit = base::Time::FromInternalValue(1);
    google.resources.push_back(ResourceRow(
        "",
        "http://google.com/style1.css",
        ResourceType::STYLESHEET,
        3, 2, 1, 1.0));
    google.resources.push_back(ResourceRow(
        "",
        "http://google.com/script3.js",
        ResourceType::SCRIPT,
        4, 0, 1, 2.1));
    google.resources.push_back(ResourceRow(
        "",
        "http://google.com/script4.js",
        ResourceType::SCRIPT,
        11, 0, 0, 2.1));
    google.resources.push_back(ResourceRow(
        "",
        "http://google.com/image1.png",
        ResourceType::IMAGE,
        6, 3, 0, 2.2));
    google.resources.push_back(ResourceRow(
        "",
        "http://google.com/a.font",
        ResourceType::LAST_TYPE,
        2, 0, 0, 5.1));

    PrefetchData reddit(PREFETCH_KEY_TYPE_URL, "http://www.reddit.com/");
    reddit.last_visit = base::Time::FromInternalValue(2);
    reddit.resources.push_back(ResourceRow(
        "",
        "http://reddit-resource.com/script1.js",
        ResourceType::SCRIPT,
        4, 0, 1, 1.0));
    reddit.resources.push_back(ResourceRow(
        "",
        "http://reddit-resource.com/script2.js",
        ResourceType::SCRIPT,
        2, 0, 0, 2.1));

    PrefetchData yahoo(PREFETCH_KEY_TYPE_URL, "http://www.yahoo.com/");
    yahoo.last_visit = base::Time::FromInternalValue(3);
    yahoo.resources.push_back(ResourceRow(
        "",
        "http://google.com/image.png",
        ResourceType::IMAGE,
        20, 1, 0, 10.0));

    test_url_data_.clear();
    test_url_data_.insert(std::make_pair("http://www.google.com/", google));
    test_url_data_.insert(std::make_pair("http://www.reddit.com/", reddit));
    test_url_data_.insert(std::make_pair("http://www.yahoo.com/", yahoo));
  }

  {  // Host data.
    PrefetchData facebook(PREFETCH_KEY_TYPE_HOST, "www.facebook.com");
    facebook.last_visit = base::Time::FromInternalValue(4);
    facebook.resources.push_back(ResourceRow(
        "",
        "http://www.facebook.com/style.css",
        ResourceType::STYLESHEET,
        5, 2, 1, 1.1));
    facebook.resources.push_back(ResourceRow(
        "",
        "http://www.facebook.com/script.js",
        ResourceType::SCRIPT,
        4, 0, 1, 2.1));
    facebook.resources.push_back(ResourceRow(
        "",
        "http://www.facebook.com/image.png",
        ResourceType::IMAGE,
        6, 3, 0, 2.2));
    facebook.resources.push_back(ResourceRow(
        "",
        "http://www.facebook.com/a.font",
        ResourceType::LAST_TYPE,
        2, 0, 0, 5.1));
    facebook.resources.push_back(ResourceRow(
        "",
        "http://www.resources.facebook.com/script.js",
        ResourceType::SCRIPT,
        11, 0, 0, 8.5));

    PrefetchData yahoo(PREFETCH_KEY_TYPE_HOST, "www.yahoo.com");
    yahoo.last_visit = base::Time::FromInternalValue(5);
    yahoo.resources.push_back(ResourceRow(
        "",
        "http://google.com/image.png",
        ResourceType::IMAGE,
        20, 1, 0, 10.0));

    test_host_data_.clear();
    test_host_data_.insert(std::make_pair("www.facebook.com", facebook));
    test_host_data_.insert(std::make_pair("www.yahoo.com", yahoo));
  }
}

TEST_F(ResourcePrefetchPredictorTest, LazilyInitializeEmpty) {
  // Tests that the predictor initializes correctly without any data.
  EXPECT_TRUE(predictor_->url_table_cache_->empty());
  EXPECT_TRUE(predictor_->host_table_cache_->empty());
}

TEST_F(ResourcePrefetchPredictorTest, LazilyInitializeWithData) {
  // Tests that the history and the db tables data are loaded correctly.
  AddUrlToHistory("http://www.google.com/", 4);
  AddUrlToHistory("http://www.yahoo.com/", 2);

  EXPECT_CALL(*mock_tables_,
              GetAllData(Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(PrefetchDataMap()))))
      .WillOnce(DoAll(SetArgPointee<0>(test_url_data_),
                      SetArgPointee<1>(test_host_data_)));

  ResetPredictor();
  InitializePredictor();

  // Test that the internal variables correctly initialized.
  EXPECT_EQ(predictor_->initialization_state_,
            ResourcePrefetchPredictor::INITIALIZED);
  EXPECT_TRUE(predictor_->inflight_navigations_.empty());

  EXPECT_EQ(test_url_data_, *predictor_->url_table_cache_);
  EXPECT_EQ(test_host_data_, *predictor_->host_table_cache_);
}

TEST_F(ResourcePrefetchPredictorTest, NavigationNotRecorded) {
  // Single navigation but history count is low, so should not record.
  AddUrlToHistory("http://www.google.com", 1);

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

  PrefetchData host_data(PREFETCH_KEY_TYPE_HOST, "www.google.com");
  host_data.resources.push_back(ResourceRow(
      "", "http://google.com/style1.css",
      ResourceType::STYLESHEET, 1, 0, 0, 1.0));
  host_data.resources.push_back(ResourceRow(
      "", "http://google.com/script1.js",
      ResourceType::SCRIPT, 1, 0, 0, 2.0));
  host_data.resources.push_back(ResourceRow(
      "", "http://google.com/script2.js",
      ResourceType::SCRIPT, 1, 0, 0, 3.0));
  EXPECT_CALL(*mock_tables_, UpdateData(empty_url_data_, host_data));

  predictor_->OnNavigationComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();
}

TEST_F(ResourcePrefetchPredictorTest, NavigationUrlNotInDB) {
  // Single navigation that will be recorded. Will check for duplicate
  // resources and also for number of resources saved.
  AddUrlToHistory("http://www.google.com", 4);

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

  PrefetchData url_data(PREFETCH_KEY_TYPE_URL, "http://www.google.com/");
  url_data.resources.push_back(ResourceRow(
      "", "http://google.com/style1.css",
      ResourceType::STYLESHEET, 1, 0, 0, 1.0));
  url_data.resources.push_back(ResourceRow(
      "", "http://google.com/script1.js",
      ResourceType::SCRIPT, 1, 0, 0, 2.0));
  url_data.resources.push_back(ResourceRow(
      "", "http://google.com/script2.js",
      ResourceType::SCRIPT, 1, 0, 0, 3.0));
  url_data.resources.push_back(ResourceRow(
      "", "http://google.com/style2.css",
      ResourceType::STYLESHEET, 1, 0, 0, 7.0));
  EXPECT_CALL(*mock_tables_, UpdateData(url_data, empty_host_data_));

  PrefetchData host_data(PREFETCH_KEY_TYPE_HOST, "www.google.com");
  host_data.resources = url_data.resources;
  EXPECT_CALL(*mock_tables_, UpdateData(empty_url_data_, host_data));

  predictor_->OnNavigationComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();
}

TEST_F(ResourcePrefetchPredictorTest, NavigationUrlInDB) {
  // Tests that navigation is recorded correctly for URL already present in
  // the database cache.
  AddUrlToHistory("http://www.google.com", 4);

  EXPECT_CALL(*mock_tables_,
              GetAllData(Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(PrefetchDataMap()))))
      .WillOnce(DoAll(SetArgPointee<0>(test_url_data_),
                      SetArgPointee<1>(test_host_data_)));
  ResetPredictor();
  InitializePredictor();
  EXPECT_EQ(3, static_cast<int>(predictor_->url_table_cache_->size()));
  EXPECT_EQ(2, static_cast<int>(predictor_->host_table_cache_->size()));

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

  PrefetchData url_data(PREFETCH_KEY_TYPE_URL, "http://www.google.com/");
  url_data.resources.push_back(ResourceRow(
      "", "http://google.com/style1.css",
      ResourceType::STYLESHEET, 4, 2, 0, 1.0));
  url_data.resources.push_back(ResourceRow(
      "", "http://google.com/script1.js",
      ResourceType::SCRIPT, 1, 0, 0, 2.0));
  url_data.resources.push_back(ResourceRow(
      "", "http://google.com/script4.js",
      ResourceType::SCRIPT, 11, 1, 1, 2.1));
  url_data.resources.push_back(ResourceRow(
      "", "http://google.com/script2.js",
      ResourceType::SCRIPT, 1, 0, 0, 3.0));
  EXPECT_CALL(*mock_tables_, UpdateData(url_data, empty_host_data_));

  EXPECT_CALL(*mock_tables_,
              DeleteSingleDataPoint("www.facebook.com",
                                    PREFETCH_KEY_TYPE_HOST));

  PrefetchData host_data(PREFETCH_KEY_TYPE_HOST, "www.google.com");
  host_data.resources.push_back(ResourceRow(
      "", "http://google.com/style1.css",
      ResourceType::STYLESHEET, 1, 0, 0, 1.0));
  host_data.resources.push_back(ResourceRow(
      "", "http://google.com/script1.js",
      ResourceType::SCRIPT, 1, 0, 0, 2.0));
  host_data.resources.push_back(ResourceRow(
      "", "http://google.com/script2.js",
      ResourceType::SCRIPT, 1, 0, 0, 3.0));
  host_data.resources.push_back(ResourceRow(
      "", "http://google.com/style2.css",
      ResourceType::STYLESHEET, 1, 0, 0, 7.0));
  EXPECT_CALL(*mock_tables_, UpdateData(empty_url_data_, host_data));

  predictor_->OnNavigationComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();
}

TEST_F(ResourcePrefetchPredictorTest, NavigationUrlNotInDBAndDBFull) {
  // Tests that a URL is deleted before another is added if the cache is full.
  AddUrlToHistory("http://www.nike.com/", 4);

  EXPECT_CALL(*mock_tables_,
              GetAllData(Pointee(ContainerEq(PrefetchDataMap())),
                         Pointee(ContainerEq(PrefetchDataMap()))))
      .WillOnce(DoAll(SetArgPointee<0>(test_url_data_),
                      SetArgPointee<1>(test_host_data_)));
  ResetPredictor();
  InitializePredictor();
  EXPECT_EQ(3, static_cast<int>(predictor_->url_table_cache_->size()));
  EXPECT_EQ(2, static_cast<int>(predictor_->host_table_cache_->size()));

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

  EXPECT_CALL(*mock_tables_,
              DeleteSingleDataPoint("http://www.google.com/",
                                    PREFETCH_KEY_TYPE_URL));
  EXPECT_CALL(*mock_tables_,
              DeleteSingleDataPoint("www.facebook.com",
                                    PREFETCH_KEY_TYPE_HOST));

  PrefetchData url_data(PREFETCH_KEY_TYPE_URL, "http://www.nike.com/");
  url_data.resources.push_back(ResourceRow(
      "", "http://nike.com/style1.css",
      ResourceType::STYLESHEET, 1, 0, 0, 1.0));
  url_data.resources.push_back(ResourceRow(
      "", "http://nike.com/image2.png",
      ResourceType::IMAGE, 1, 0, 0, 2.0));
  EXPECT_CALL(*mock_tables_, UpdateData(url_data, empty_host_data_));

  PrefetchData host_data(PREFETCH_KEY_TYPE_HOST, "www.nike.com");
  host_data.resources = url_data.resources;
  EXPECT_CALL(*mock_tables_, UpdateData(empty_url_data_, host_data));

  predictor_->OnNavigationComplete(main_frame.navigation_id);
  profile_->BlockUntilHistoryProcessesPendingRequests();
}

TEST_F(ResourcePrefetchPredictorTest, DeleteUrls) {
  // Add some dummy entries to cache.
  predictor_->url_table_cache_->insert(std::make_pair(
      "http://www.google.com/page1.html",
      PrefetchData(PREFETCH_KEY_TYPE_URL, "http://www.google.com/page1.html")));
  predictor_->url_table_cache_->insert(std::make_pair(
      "http://www.google.com/page2.html",
      PrefetchData(PREFETCH_KEY_TYPE_URL, "http://www.google.com/page2.html")));
  predictor_->url_table_cache_->insert(std::make_pair(
      "http://www.yahoo.com/",
      PrefetchData(PREFETCH_KEY_TYPE_URL, "http://www.yahoo.com/")));
  predictor_->url_table_cache_->insert(std::make_pair(
      "http://www.apple.com/",
      PrefetchData(PREFETCH_KEY_TYPE_URL, "http://www.apple.com/")));
  predictor_->url_table_cache_->insert(std::make_pair(
      "http://www.nike.com/",
      PrefetchData(PREFETCH_KEY_TYPE_URL, "http://www.nike.com/")));

  predictor_->host_table_cache_->insert(std::make_pair(
      "www.google.com",
      PrefetchData(PREFETCH_KEY_TYPE_HOST, "www.google.com")));
  predictor_->host_table_cache_->insert(std::make_pair(
      "www.yahoo.com",
      PrefetchData(PREFETCH_KEY_TYPE_HOST, "www.yahoo.com")));
  predictor_->host_table_cache_->insert(std::make_pair(
      "www.apple.com",
      PrefetchData(PREFETCH_KEY_TYPE_HOST, "www.apple.com")));

  history::URLRows rows;
  rows.push_back(history::URLRow(GURL("http://www.google.com/page2.html")));
  rows.push_back(history::URLRow(GURL("http://www.apple.com")));
  rows.push_back(history::URLRow(GURL("http://www.nike.com")));

  std::vector<std::string> urls_to_delete, hosts_to_delete;
  urls_to_delete.push_back("http://www.google.com/page2.html");
  urls_to_delete.push_back("http://www.apple.com/");
  urls_to_delete.push_back("http://www.nike.com/");
  hosts_to_delete.push_back("www.google.com");
  hosts_to_delete.push_back("www.apple.com");

  EXPECT_CALL(*mock_tables_,
              DeleteData(ContainerEq(urls_to_delete),
                         ContainerEq(hosts_to_delete)));

  predictor_->DeleteUrls(rows);
  EXPECT_EQ(2, static_cast<int>(predictor_->url_table_cache_->size()));
  EXPECT_EQ(1, static_cast<int>(predictor_->host_table_cache_->size()));

  EXPECT_CALL(*mock_tables_, DeleteAllData());

  predictor_->DeleteAllUrls();
  EXPECT_TRUE(predictor_->url_table_cache_->empty());
  EXPECT_TRUE(predictor_->host_table_cache_->empty());
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
      predictor_->inflight_navigations_[main_frame1.navigation_id]->size()));
  EXPECT_TRUE(URLRequestSummaryAreEqual(
      resource1,
      predictor_->inflight_navigations_[main_frame1.navigation_id]->at(0)));
  EXPECT_TRUE(URLRequestSummaryAreEqual(
      resource2,
      predictor_->inflight_navigations_[main_frame1.navigation_id]->at(1)));
  EXPECT_TRUE(URLRequestSummaryAreEqual(
      resource3,
      predictor_->inflight_navigations_[main_frame1.navigation_id]->at(2)));
}

}  // namespace predictors
