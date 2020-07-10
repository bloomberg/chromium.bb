// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/base/network_isolation_key.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using testing::_;
using testing::Return;
using testing::StrictMock;
using testing::DoAll;
using testing::SetArgPointee;

namespace predictors {

namespace {

// First two are preconnectable, last one is not (see SetUp()).
const char kUrl[] = "http://www.google.com/cats";
const char kUrl2[] = "http://www.google.com/dogs";
const char kUrl3[] =
    "file://unknown.website/catsanddogs";  // Non http(s) scheme to avoid
                                           // preconnect to the main frame.

class MockPreconnectManager : public PreconnectManager {
 public:
  MockPreconnectManager(base::WeakPtr<Delegate> delegate, Profile* profile);

  MOCK_METHOD2(StartProxy,
               void(const GURL& url,
                    const std::vector<PreconnectRequest>& requests));
  MOCK_METHOD2(StartPreresolveHost,
               void(const GURL& url,
                    const net::NetworkIsolationKey& network_isolation_key));
  MOCK_METHOD2(StartPreresolveHosts,
               void(const std::vector<std::string>& hostnames,
                    const net::NetworkIsolationKey& network_isolation_key));
  MOCK_METHOD3(StartPreconnectUrl,
               void(const GURL& url,
                    bool allow_credentials,
                    net::NetworkIsolationKey network_isolation_key));
  MOCK_METHOD1(Stop, void(const GURL& url));

  void Start(const GURL& url,
             std::vector<PreconnectRequest> requests) override {
    StartProxy(url, requests);
  }
};

MockPreconnectManager::MockPreconnectManager(base::WeakPtr<Delegate> delegate,
                                             Profile* profile)
    : PreconnectManager(delegate, profile) {}

LoadingPredictorConfig CreateConfig() {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  return config;
}

// Creates a NetworkIsolationKey for a main frame navigation to URL.
net::NetworkIsolationKey CreateNetworkIsolationKey(const GURL& main_frame_url) {
  url::Origin origin = url::Origin::Create(main_frame_url);
  return net::NetworkIsolationKey(origin, origin);
}

}  // namespace

class LoadingPredictorTest : public testing::Test {
 public:
  ~LoadingPredictorTest() override;
  void SetUp() override;
  void TearDown() override;

 protected:
  virtual void SetPreference();

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<LoadingPredictor> predictor_;
  StrictMock<MockResourcePrefetchPredictor>* mock_predictor_;
};

LoadingPredictorTest::~LoadingPredictorTest() = default;

void LoadingPredictorTest::SetUp() {
  profile_ = std::make_unique<TestingProfile>();
  SetPreference();
  auto config = CreateConfig();
  predictor_ = std::make_unique<LoadingPredictor>(config, profile_.get());

  auto mock = std::make_unique<StrictMock<MockResourcePrefetchPredictor>>(
      config, profile_.get());
  EXPECT_CALL(*mock, PredictPreconnectOrigins(GURL(kUrl), _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock, PredictPreconnectOrigins(GURL(kUrl2), _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock, PredictPreconnectOrigins(GURL(kUrl3), _))
      .WillRepeatedly(Return(false));

  mock_predictor_ = mock.get();
  predictor_->set_mock_resource_prefetch_predictor(std::move(mock));

  predictor_->StartInitialization();
  content::RunAllTasksUntilIdle();
}

void LoadingPredictorTest::TearDown() {
  predictor_->Shutdown();
}

void LoadingPredictorTest::SetPreference() {
  profile_->GetPrefs()->SetInteger(
      prefs::kNetworkPredictionOptions,
      chrome_browser_net::NETWORK_PREDICTION_NEVER);
}

class LoadingPredictorPreconnectTest : public LoadingPredictorTest {
 public:
  void SetUp() override;

 protected:
  void SetPreference() override;

  StrictMock<MockPreconnectManager>* mock_preconnect_manager_;
};

void LoadingPredictorPreconnectTest::SetUp() {
  LoadingPredictorTest::SetUp();
  auto mock_preconnect_manager =
      std::make_unique<StrictMock<MockPreconnectManager>>(
          predictor_->GetWeakPtr(), profile_.get());
  mock_preconnect_manager_ = mock_preconnect_manager.get();
  predictor_->set_mock_preconnect_manager(std::move(mock_preconnect_manager));
}

void LoadingPredictorPreconnectTest::SetPreference() {
  profile_->GetPrefs()->SetInteger(
      prefs::kNetworkPredictionOptions,
      chrome_browser_net::NETWORK_PREDICTION_ALWAYS);
}

TEST_F(LoadingPredictorTest, TestMainFrameResponseCancelsHint) {
  const GURL url = GURL(kUrl);
  predictor_->PrepareForPageLoad(url, HintOrigin::EXTERNAL);
  EXPECT_EQ(1UL, predictor_->active_hints_.size());

  auto navigation_id =
      CreateNavigationID(SessionID::FromSerializedValue(12), url.spec());
  predictor_->OnNavigationFinished(navigation_id, navigation_id, false);
  EXPECT_TRUE(predictor_->active_hints_.empty());
}

TEST_F(LoadingPredictorTest, TestMainFrameRequestCancelsStaleNavigations) {
  const std::string url = kUrl;
  const std::string url2 = kUrl2;
  const SessionID tab_id = SessionID::FromSerializedValue(12);
  const auto& active_navigations = predictor_->active_navigations_;
  const auto& active_hints = predictor_->active_hints_;

  auto navigation_id = CreateNavigationID(tab_id, url);

  predictor_->OnNavigationStarted(navigation_id);
  EXPECT_NE(active_navigations.find(navigation_id), active_navigations.end());
  EXPECT_NE(active_hints.find(GURL(url)), active_hints.end());

  auto navigation_id2 = CreateNavigationID(tab_id, url2);
  predictor_->OnNavigationStarted(navigation_id2);
  EXPECT_EQ(active_navigations.find(navigation_id), active_navigations.end());
  EXPECT_EQ(active_hints.find(GURL(url)), active_hints.end());

  EXPECT_NE(active_navigations.find(navigation_id2), active_navigations.end());
}

TEST_F(LoadingPredictorTest, TestMainFrameResponseClearsNavigations) {
  const std::string url = kUrl;
  const std::string redirected = kUrl2;
  const SessionID tab_id = SessionID::FromSerializedValue(12);
  const auto& active_navigations = predictor_->active_navigations_;
  const auto& active_hints = predictor_->active_hints_;

  auto navigation_id = CreateNavigationID(tab_id, url);

  predictor_->OnNavigationStarted(navigation_id);
  EXPECT_NE(active_navigations.find(navigation_id), active_navigations.end());
  EXPECT_FALSE(active_hints.empty());

  predictor_->OnNavigationFinished(navigation_id, navigation_id, false);
  EXPECT_TRUE(active_navigations.empty());
  EXPECT_TRUE(active_hints.empty());

  // With redirects.
  predictor_->OnNavigationStarted(navigation_id);
  EXPECT_NE(active_navigations.find(navigation_id), active_navigations.end());
  EXPECT_FALSE(active_hints.empty());

  auto new_navigation_id = CreateNavigationID(tab_id, redirected);
  predictor_->OnNavigationFinished(navigation_id, new_navigation_id, false);
  EXPECT_TRUE(active_navigations.empty());
  EXPECT_TRUE(active_hints.empty());
}

TEST_F(LoadingPredictorTest, TestMainFrameRequestDoesntCancelExternalHint) {
  const GURL url = GURL(kUrl);
  const SessionID tab_id = SessionID::FromSerializedValue(12);
  const auto& active_navigations = predictor_->active_navigations_;
  auto& active_hints = predictor_->active_hints_;

  predictor_->PrepareForPageLoad(url, HintOrigin::EXTERNAL);
  auto it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_TRUE(active_navigations.empty());

  // To check that the hint is not replaced, set the start time in the past,
  // and check later that it didn't change.
  base::TimeTicks start_time = it->second - base::TimeDelta::FromSeconds(10);
  it->second = start_time;

  auto navigation_id = CreateNavigationID(tab_id, url.spec());
  predictor_->OnNavigationStarted(navigation_id);
  EXPECT_NE(active_navigations.find(navigation_id), active_navigations.end());
  it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_EQ(start_time, it->second);
}

TEST_F(LoadingPredictorTest, TestDuplicateHintAfterPreconnectCompleteCalled) {
  const GURL url = GURL(kUrl);
  const auto& active_navigations = predictor_->active_navigations_;
  auto& active_hints = predictor_->active_hints_;

  predictor_->PrepareForPageLoad(url, HintOrigin::EXTERNAL);
  auto it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_TRUE(active_navigations.empty());

  // To check that the hint is replaced, set the start time in the past,
  // and check later that it changed.
  base::TimeTicks start_time = it->second - base::TimeDelta::FromSeconds(10);
  it->second = start_time;

  std::unique_ptr<PreconnectStats> preconnect_stats =
      std::make_unique<PreconnectStats>(url);
  predictor_->PreconnectFinished(std::move(preconnect_stats));

  predictor_->PrepareForPageLoad(url, HintOrigin::NAVIGATION_PREDICTOR);
  it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_TRUE(active_navigations.empty());

  // Calling PreconnectFinished() must have cleared the hint, and duplicate
  // PrepareForPageLoad() call should be honored.
  EXPECT_LT(start_time, it->second);
}

TEST_F(LoadingPredictorTest,
       TestDuplicateHintAfterPreconnectCompleteNotCalled) {
  const GURL url = GURL(kUrl);
  const auto& active_navigations = predictor_->active_navigations_;
  auto& active_hints = predictor_->active_hints_;

  predictor_->PrepareForPageLoad(url, HintOrigin::EXTERNAL);
  auto it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_TRUE(active_navigations.empty());

  content::RunAllTasksUntilIdle();
  it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());

  // To check that the hint is not replaced, set the start time in the recent
  // past, and check later that it didn't change.
  base::TimeTicks start_time = it->second - base::TimeDelta::FromSeconds(10);
  it->second = start_time;

  predictor_->PrepareForPageLoad(url, HintOrigin::NAVIGATION_PREDICTOR);
  it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_TRUE(active_navigations.empty());

  // Duplicate PrepareForPageLoad() call should not be honored.
  EXPECT_EQ(start_time, it->second);
}

TEST_F(LoadingPredictorTest, TestDontTrackNonPrefetchableUrls) {
  const GURL url3 = GURL(kUrl3);
  predictor_->PrepareForPageLoad(url3, HintOrigin::NAVIGATION);
  EXPECT_TRUE(predictor_->active_hints_.empty());
}

TEST_F(LoadingPredictorTest, TestDontPredictOmniboxHints) {
  const GURL omnibox_suggestion = GURL("http://search.com/kittens");
  // We expect that no prediction will be requested.
  predictor_->PrepareForPageLoad(omnibox_suggestion, HintOrigin::OMNIBOX);
  EXPECT_TRUE(predictor_->active_hints_.empty());
}

TEST_F(LoadingPredictorPreconnectTest, TestHandleOmniboxHint) {
  const GURL preconnect_suggestion = GURL("http://search.com/kittens");
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartPreconnectUrl(preconnect_suggestion, true,
                         CreateNetworkIsolationKey(preconnect_suggestion)));
  predictor_->PrepareForPageLoad(preconnect_suggestion, HintOrigin::OMNIBOX,
                                 true);
  // The second suggestion for the same host should be filtered out.
  const GURL preconnect_suggestion2 = GURL("http://search.com/puppies");
  predictor_->PrepareForPageLoad(preconnect_suggestion2, HintOrigin::OMNIBOX,
                                 true);

  const GURL preresolve_suggestion = GURL("http://en.wikipedia.org/wiki/main");
  url::Origin origin = url::Origin::Create(preresolve_suggestion);
  EXPECT_CALL(*mock_preconnect_manager_,
              StartPreresolveHost(preresolve_suggestion,
                                  net::NetworkIsolationKey(origin, origin)));
  predictor_->PrepareForPageLoad(preresolve_suggestion, HintOrigin::OMNIBOX,
                                 false);
  // The second suggestions should be filtered out as well.
  const GURL preresolve_suggestion2 =
      GURL("http://en.wikipedia.org/wiki/random");
  predictor_->PrepareForPageLoad(preresolve_suggestion2, HintOrigin::OMNIBOX,
                                 false);
}

// Checks that the predictor preconnects to an initial origin even when it
// doesn't have any historical data for this host.
TEST_F(LoadingPredictorPreconnectTest, TestAddInitialUrlToEmptyPrediction) {
  GURL main_frame_url("http://search.com/kittens");
  EXPECT_CALL(*mock_predictor_, PredictPreconnectOrigins(main_frame_url, _))
      .WillOnce(Return(false));
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartProxy(main_frame_url,
                 std::vector<PreconnectRequest>(
                     {{url::Origin::Create(GURL("http://search.com")), 2,
                       CreateNetworkIsolationKey(main_frame_url)}})));
  predictor_->PrepareForPageLoad(main_frame_url, HintOrigin::NAVIGATION);
}

// Checks that the predictor doesn't add an initial origin to a preconnect list
// if the list already containts the origin.
TEST_F(LoadingPredictorPreconnectTest, TestAddInitialUrlMatchesPrediction) {
  GURL main_frame_url("http://search.com/kittens");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  PreconnectPrediction prediction = CreatePreconnectPrediction(
      "search.com", true,
      {{url::Origin::Create(GURL("http://search.com")), 1,
        network_isolation_key},
       {url::Origin::Create(GURL("http://cdn.search.com")), 1,
        network_isolation_key},
       {url::Origin::Create(GURL("http://ads.search.com")), 0,
        network_isolation_key}});
  EXPECT_CALL(*mock_predictor_, PredictPreconnectOrigins(main_frame_url, _))
      .WillOnce(DoAll(SetArgPointee<1>(prediction), Return(true)));
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartProxy(main_frame_url,
                 std::vector<PreconnectRequest>(
                     {{url::Origin::Create(GURL("http://search.com")), 2,
                       network_isolation_key},
                      {url::Origin::Create(GURL("http://cdn.search.com")), 1,
                       network_isolation_key},
                      {url::Origin::Create(GURL("http://ads.search.com")), 0,
                       network_isolation_key}})));
  predictor_->PrepareForPageLoad(main_frame_url, HintOrigin::EXTERNAL);
}

// Checks that the predictor adds an initial origin to a preconnect list if the
// list doesn't contain this origin already. It may be possible if an initial
// url redirects to another host.
TEST_F(LoadingPredictorPreconnectTest, TestAddInitialUrlDoesntMatchPrediction) {
  GURL main_frame_url("http://search.com/kittens");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  PreconnectPrediction prediction = CreatePreconnectPrediction(
      "search.com", true,
      {{url::Origin::Create(GURL("http://en.search.com")), 1,
        network_isolation_key},
       {url::Origin::Create(GURL("http://cdn.search.com")), 1,
        network_isolation_key},
       {url::Origin::Create(GURL("http://ads.search.com")), 0,
        network_isolation_key}});
  EXPECT_CALL(*mock_predictor_, PredictPreconnectOrigins(main_frame_url, _))
      .WillOnce(DoAll(SetArgPointee<1>(prediction), Return(true)));
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartProxy(main_frame_url,
                 std::vector<PreconnectRequest>(
                     {{url::Origin::Create(GURL("http://search.com")), 2,
                       network_isolation_key},
                      {url::Origin::Create(GURL("http://en.search.com")), 1,
                       network_isolation_key},
                      {url::Origin::Create(GURL("http://cdn.search.com")), 1,
                       network_isolation_key},
                      {url::Origin::Create(GURL("http://ads.search.com")), 0,
                       network_isolation_key}})));
  predictor_->PrepareForPageLoad(main_frame_url, HintOrigin::EXTERNAL);
}

// Checks that the predictor doesn't preconnect to a bad url.
TEST_F(LoadingPredictorPreconnectTest, TestAddInvalidInitialUrl) {
  GURL main_frame_url("file:///tmp/index.html");
  EXPECT_CALL(*mock_predictor_, PredictPreconnectOrigins(main_frame_url, _))
      .WillOnce(Return(false));
  predictor_->PrepareForPageLoad(main_frame_url, HintOrigin::EXTERNAL);
}

}  // namespace predictors
