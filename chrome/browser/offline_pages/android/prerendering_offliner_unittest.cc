// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/prerendering_offliner.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/offline_pages/prerendering_loader.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/mhtml_extra_parts.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const int64_t kRequestId = 7;
const GURL kHttpUrl("http://tunafish.com");
const GURL kFileUrl("file://sailfish.png");
const ClientId kClientId("AsyncLoading", "88");
const bool kUserRequested = true;
const char kRequestOrigin[] = "abc.xyz";

// Mock Loader for testing the Offliner calls.
class MockPrerenderingLoader : public PrerenderingLoader {
 public:
  explicit MockPrerenderingLoader(content::BrowserContext* browser_context)
      : PrerenderingLoader(browser_context),
        can_prerender_(true),
        mock_loading_(false),
        mock_loaded_(false),
        mock_is_lowbar_met_(false),
        start_snapshot_called_(false),
        web_contents_(nullptr) {}
  ~MockPrerenderingLoader() override { delete web_contents_; }

  bool LoadPage(const GURL& url,
                const LoadPageCallback& load_done_callback,
                const ProgressCallback& progress_callback) override {
    mock_loading_ = can_prerender_;
    load_page_callback_ = load_done_callback;
    progress_callback_ = progress_callback;
    return mock_loading_;
  }

  void StopLoading() override {
    mock_loading_ = false;
    mock_loaded_ = false;
  }

  bool IsIdle() override { return !mock_loading_ && !mock_loaded_; }
  bool IsLoaded() override { return mock_loaded_; }
  bool IsLowbarMet() override { return mock_is_lowbar_met_; }

  void StartSnapshot() override {
    start_snapshot_called_ = true;
    // Call start saving process.
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        new TestingProfile(), NULL);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(load_page_callback_,
                              Offliner::RequestStatus::LOADED, web_contents_));
  }

  void CompleteLoadingAsFailed() {
    DCHECK(mock_loading_);
    mock_loading_ = false;
    mock_loaded_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(load_page_callback_, Offliner::RequestStatus::LOADING_FAILED,
                   nullptr /* web_contents */));
  }

  void CompleteLoadingAsLoaded() {
    DCHECK(mock_loading_);
    mock_loading_ = false;
    mock_loaded_ = true;
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        new TestingProfile(), NULL);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(load_page_callback_,
                              Offliner::RequestStatus::LOADED, web_contents_));
  }

  void CompleteLoadingAsCanceled() {
    DCHECK(!IsIdle());
    mock_loading_ = false;
    mock_loaded_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(load_page_callback_,
                              Offliner::RequestStatus::LOADING_CANCELED,
                              nullptr /* web_contents */));
  }

  void DisablePrerendering() { can_prerender_ = false; }
  const LoadPageCallback& load_page_callback() { return load_page_callback_; }
  void set_is_lowbar_met(bool is_lowbar_met) {
    mock_is_lowbar_met_ = is_lowbar_met;
  }
  bool start_snapshot_called() { return start_snapshot_called_; }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  bool can_prerender_;
  bool mock_loading_;
  bool mock_loaded_;
  bool mock_is_lowbar_met_;
  bool start_snapshot_called_;
  LoadPageCallback load_page_callback_;
  ProgressCallback progress_callback_;
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(MockPrerenderingLoader);
};

// Mock OfflinePageModel for testing the SavePage calls.
class MockOfflinePageModel : public StubOfflinePageModel {
 public:
  MockOfflinePageModel() : mock_saving_(false) {}
  ~MockOfflinePageModel() override {}

  void SavePage(const SavePageParams& save_page_params,
                std::unique_ptr<OfflinePageArchiver> archiver,
                const SavePageCallback& callback) override {
    mock_saving_ = true;
    save_page_callback_ = callback;
    save_page_params_ = save_page_params;
  }

  void CompleteSavingAsArchiveCreationFailed() {
    DCHECK(mock_saving_);
    mock_saving_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(save_page_callback_,
                              SavePageResult::ARCHIVE_CREATION_FAILED, 0));
  }

  void CompleteSavingAsSuccess() {
    DCHECK(mock_saving_);
    mock_saving_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(save_page_callback_, SavePageResult::SUCCESS, 123456));
  }

  void CompleteSavingAsAlreadyExists() {
    DCHECK(mock_saving_);
    mock_saving_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(save_page_callback_,
                              SavePageResult::ALREADY_EXISTS, 123456));
  }

  bool mock_saving() const { return mock_saving_; }

  SavePageParams save_page_params() { return save_page_params_; }

 private:
  bool mock_saving_;
  SavePageParams save_page_params_;
  SavePageCallback save_page_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockOfflinePageModel);
};

void PumpLoop() {
  base::RunLoop().RunUntilIdle();
}

}  // namespace

class PrerenderingOfflinerTest : public testing::Test {
 public:
  PrerenderingOfflinerTest();
  ~PrerenderingOfflinerTest() override;

  void SetUp() override;

  Profile* profile() { return &profile_; }
  PrerenderingOffliner* offliner() const { return offliner_.get(); }
  Offliner::CompletionCallback const completion_callback() {
    return base::Bind(&PrerenderingOfflinerTest::OnCompletion,
                      base::Unretained(this));
  }
  Offliner::ProgressCallback const progress_callback() {
    return base::Bind(&PrerenderingOfflinerTest::OnProgress,
                      base::Unretained(this));
  }
  Offliner::CancelCallback const cancel_callback() {
    return base::Bind(&PrerenderingOfflinerTest::OnCancel,
                      base::Unretained(this));
  }

  bool SaveInProgress() const { return model_->mock_saving(); }
  MockPrerenderingLoader* loader() { return loader_; }
  MockOfflinePageModel* model() { return model_; }
  bool completion_callback_called() { return completion_callback_called_; }
  Offliner::RequestStatus request_status() { return request_status_; }
  OfflinerPolicy* policy() { return policy_; }
  bool cancel_callback_called() { return cancel_callback_called_; }

 private:
  void OnCompletion(const SavePageRequest& request,
                    Offliner::RequestStatus status);
  void OnProgress(const SavePageRequest& request, int64_t bytes);
  void OnCancel(const SavePageRequest& request);

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<PrerenderingOffliner> offliner_;
  // Not owned.
  MockPrerenderingLoader* loader_;
  MockOfflinePageModel* model_;
  bool completion_callback_called_;
  bool cancel_callback_called_;
  Offliner::RequestStatus request_status_;
  OfflinerPolicy* policy_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderingOfflinerTest);
};

PrerenderingOfflinerTest::PrerenderingOfflinerTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      completion_callback_called_(false),
      cancel_callback_called_(false),
      request_status_(Offliner::RequestStatus::UNKNOWN) {}

PrerenderingOfflinerTest::~PrerenderingOfflinerTest() {}

void PrerenderingOfflinerTest::SetUp() {
  model_ = new MockOfflinePageModel();
  policy_ = new OfflinerPolicy();
  offliner_.reset(new PrerenderingOffliner(profile(), policy_, model_));
  std::unique_ptr<MockPrerenderingLoader> mock_loader(
      new MockPrerenderingLoader(nullptr));
  loader_ = mock_loader.get();
  offliner_->SetLoaderForTesting(std::move(mock_loader));
}

void PrerenderingOfflinerTest::OnCompletion(const SavePageRequest& request,
                                            Offliner::RequestStatus status) {
  DCHECK(!completion_callback_called_);  // Expect single callback per request.
  completion_callback_called_ = true;
  request_status_ = status;
}

void PrerenderingOfflinerTest::OnProgress(const SavePageRequest& request,
                                          int64_t bytes) {}

void PrerenderingOfflinerTest::OnCancel(const SavePageRequest& request) {
  DCHECK(!cancel_callback_called_);
  cancel_callback_called_ = true;
}

TEST_F(PrerenderingOfflinerTest, LoadAndSaveBadUrl) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kFileUrl, kClientId, creation_time, kUserRequested);
  EXPECT_FALSE(offliner()->LoadAndSave(request, completion_callback(),
                                       progress_callback()));
  EXPECT_TRUE(loader()->IsIdle());
}

TEST_F(PrerenderingOfflinerTest, LoadAndSavePrerenderingDisabled) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kHttpUrl, kClientId, creation_time, kUserRequested);
  loader()->DisablePrerendering();
  EXPECT_FALSE(offliner()->LoadAndSave(request, completion_callback(),
                                       progress_callback()));
  EXPECT_TRUE(loader()->IsIdle());
}

TEST_F(PrerenderingOfflinerTest,
       LoadAndSaveBlockThirdPartyCookiesForCustomTabs) {
  base::Time creation_time = base::Time::Now();
  ClientId custom_tabs_client_id("custom_tabs", "88");
  SavePageRequest request(kRequestId, kHttpUrl, custom_tabs_client_id,
                          creation_time, kUserRequested);
  profile()->GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_FALSE(offliner()->LoadAndSave(request, completion_callback(),
                                       progress_callback()));
  EXPECT_TRUE(loader()->IsIdle());
}

TEST_F(PrerenderingOfflinerTest,
       LoadAndSaveBlockOnDisabledPrerendererForCustomTabs) {
  base::Time creation_time = base::Time::Now();
  ClientId custom_tabs_client_id("custom_tabs", "88");
  SavePageRequest request(kRequestId, kHttpUrl, custom_tabs_client_id,
                          creation_time, kUserRequested);
  profile()->GetPrefs()->SetInteger(
      prefs::kNetworkPredictionOptions,
      chrome_browser_net::NETWORK_PREDICTION_NEVER);
  EXPECT_FALSE(offliner()->LoadAndSave(request, completion_callback(),
                                       progress_callback()));
  EXPECT_TRUE(loader()->IsIdle());
}

TEST_F(PrerenderingOfflinerTest, LoadAndSaveLoadStartedButFails) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kHttpUrl, kClientId, creation_time, kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsFailed();
  PumpLoop();
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADING_FAILED, request_status());
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(PrerenderingOfflinerTest, CancelWhenLoading) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kHttpUrl, kClientId, creation_time, kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  EXPECT_FALSE(loader()->IsIdle());

  offliner()->Cancel(cancel_callback());
  PumpLoop();
  EXPECT_TRUE(cancel_callback_called());
  EXPECT_TRUE(loader()->IsIdle());
}

TEST_F(PrerenderingOfflinerTest, PropagatesRequestOrigin) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  request.set_request_origin(kRequestOrigin);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  loader()->CompleteLoadingAsLoaded();
  PumpLoop();
  EXPECT_EQ(kRequestOrigin, model()->save_page_params().request_origin);
}

TEST_F(PrerenderingOfflinerTest, CancelWhenLoaded) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kHttpUrl, kClientId, creation_time, kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsLoaded();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(loader()->IsLoaded());
  EXPECT_TRUE(SaveInProgress());

  offliner()->Cancel(cancel_callback());
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(cancel_callback_called());
  EXPECT_FALSE(loader()->IsLoaded());
  // Note: save still in progress since it does not support canceling.
  EXPECT_TRUE(SaveInProgress());

  // Subsequent save callback causes no harm (no crash and no callback).
  model()->CompleteSavingAsArchiveCreationFailed();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(PrerenderingOfflinerTest, LoadAndSaveLoadedButSaveFails) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kHttpUrl, kClientId, creation_time, kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsLoaded();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(loader()->IsLoaded());
  EXPECT_TRUE(SaveInProgress());

  model()->CompleteSavingAsArchiveCreationFailed();
  PumpLoop();
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::SAVE_FAILED, request_status());
  EXPECT_FALSE(loader()->IsLoaded());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(PrerenderingOfflinerTest, LoadAndSaveSuccessful) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kHttpUrl, kClientId, creation_time, kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsLoaded();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(loader()->IsLoaded());
  EXPECT_TRUE(SaveInProgress());

  model()->CompleteSavingAsSuccess();
  PumpLoop();
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::SAVED, request_status());
  EXPECT_FALSE(loader()->IsLoaded());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(PrerenderingOfflinerTest, LoadAndSavePageAlreadyExists) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsLoaded();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(loader()->IsLoaded());
  EXPECT_TRUE(SaveInProgress());

  model()->CompleteSavingAsAlreadyExists();
  PumpLoop();
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::SAVED, request_status());
  EXPECT_FALSE(loader()->IsLoaded());
  EXPECT_FALSE(SaveInProgress());
}

TEST_F(PrerenderingOfflinerTest, LoadAndSaveLoadedButThenCanceledFromLoader) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kHttpUrl, kClientId, creation_time, kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsLoaded();
  PumpLoop();
  EXPECT_FALSE(completion_callback_called());
  EXPECT_TRUE(loader()->IsLoaded());
  EXPECT_TRUE(SaveInProgress());

  loader()->CompleteLoadingAsCanceled();
  PumpLoop();
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::LOADING_CANCELED, request_status());
  EXPECT_FALSE(loader()->IsLoaded());
  // Note: save still in progress since it does not support canceling.
  EXPECT_TRUE(SaveInProgress());
}

TEST_F(PrerenderingOfflinerTest, ForegroundTransitionCancelsOnLowEndDevice) {
  offliner()->SetLowEndDeviceForTesting(true);

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kHttpUrl, kClientId, creation_time, kUserRequested);
  // LoadAndSave completes asynchronously, and notifies us via the
  // completion_callback() when it completes.
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  EXPECT_FALSE(loader()->IsIdle());

  offliner()->SetApplicationStateForTesting(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);

  // Wait until the completion callback arrives before checking postconditions.
  PumpLoop();

  // Loading canceled on low-end device.
  EXPECT_TRUE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::FOREGROUND_CANCELED, request_status());
}

TEST_F(PrerenderingOfflinerTest, ForegroundTransitionIgnoredOnHighEndDevice) {
  offliner()->SetLowEndDeviceForTesting(false);

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(
      kRequestId, kHttpUrl, kClientId, creation_time, kUserRequested);
  // LoadAndSave completes asynchronously, and notifies us via the
  // completion_callback() when it completes.
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  EXPECT_FALSE(loader()->IsIdle());

  offliner()->SetApplicationStateForTesting(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);

  // Wait until the completion callback arrives before checking postconditions.
  PumpLoop();

  // Loader still loading since not low-end device.
  EXPECT_FALSE(loader()->IsIdle());
}

TEST_F(PrerenderingOfflinerTest, HandleTimeoutWithLowbarAndCompletedTriesMet) {
  offliner()->SetLowEndDeviceForTesting(false);

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  request.set_completed_attempt_count(policy()->GetMaxCompletedTries() - 1);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  loader()->set_is_lowbar_met(true);
  EXPECT_TRUE(offliner()->HandleTimeout(kRequestId));
  EXPECT_TRUE(loader()->start_snapshot_called());
  PumpLoop();
  // EXPECT_TRUE(SaveInProgress());
  model()->CompleteSavingAsSuccess();
  PumpLoop();
  EXPECT_TRUE(completion_callback_called());
  EXPECT_EQ(Offliner::RequestStatus::SAVED_ON_LAST_RETRY, request_status());
}

TEST_F(PrerenderingOfflinerTest,
       HandleTimeoutWithLowbarAndCompletedTriesMetLowEndDevice) {
  offliner()->SetLowEndDeviceForTesting(true);

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  request.set_completed_attempt_count(policy()->GetMaxCompletedTries() - 1);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  loader()->set_is_lowbar_met(true);
  EXPECT_TRUE(offliner()->HandleTimeout(kRequestId));
  EXPECT_TRUE(loader()->start_snapshot_called());
}

TEST_F(PrerenderingOfflinerTest,
       HandleTimeoutCompletedTriesMetWithoutLowbarMet) {
  offliner()->SetLowEndDeviceForTesting(false);

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  request.set_completed_attempt_count(policy()->GetMaxCompletedTries() - 1);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  loader()->set_is_lowbar_met(false);
  EXPECT_FALSE(offliner()->HandleTimeout(kRequestId));
  EXPECT_FALSE(loader()->start_snapshot_called());
}

TEST_F(PrerenderingOfflinerTest, HandleTimeoutStartedTriesMetWithoutLowbarMet) {
  offliner()->SetLowEndDeviceForTesting(false);

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  request.set_started_attempt_count(policy()->GetMaxStartedTries() - 1);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  loader()->set_is_lowbar_met(false);
  EXPECT_FALSE(offliner()->HandleTimeout(kRequestId));
  EXPECT_FALSE(loader()->start_snapshot_called());
}

TEST_F(PrerenderingOfflinerTest, HandleTimeoutWithLowbarAndStartedTriesMet) {
  offliner()->SetLowEndDeviceForTesting(false);

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  request.set_started_attempt_count(policy()->GetMaxStartedTries() - 1);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  loader()->set_is_lowbar_met(true);
  EXPECT_TRUE(offliner()->HandleTimeout(kRequestId));
  EXPECT_TRUE(loader()->start_snapshot_called());
}

TEST_F(PrerenderingOfflinerTest, HandleTimeoutWithOnlyLowbarMet) {
  offliner()->SetLowEndDeviceForTesting(false);

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  loader()->set_is_lowbar_met(true);
  EXPECT_FALSE(offliner()->HandleTimeout(kRequestId));
  EXPECT_FALSE(loader()->start_snapshot_called());
}

TEST_F(PrerenderingOfflinerTest, SignalCollectionDisabled) {
  // Ensure feature flag for Signal collection is off,
  EXPECT_FALSE(offline_pages::IsOfflinePagesLoadSignalCollectingEnabled());

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsLoaded();
  PumpLoop();

  model()->CompleteSavingAsSuccess();
  PumpLoop();
  EXPECT_EQ(Offliner::RequestStatus::SAVED, request_status());

  // No extra parts should be added if the flag is off.
  content::MHTMLExtraParts* extra_parts =
      content::MHTMLExtraParts::FromWebContents(loader()->web_contents());
  EXPECT_EQ(extra_parts->size(), 0);
}

TEST_F(PrerenderingOfflinerTest, SignalCollectionEnabled) {
  // Ensure feature flag for signal collection is on.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOfflinePagesLoadSignalCollectingFeature);
  EXPECT_TRUE(IsOfflinePagesLoadSignalCollectingEnabled());

  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kHttpUrl, kClientId, creation_time,
                          kUserRequested);
  EXPECT_TRUE(offliner()->LoadAndSave(request, completion_callback(),
                                      progress_callback()));
  EXPECT_FALSE(loader()->IsIdle());
  EXPECT_EQ(Offliner::RequestStatus::UNKNOWN, request_status());

  loader()->CompleteLoadingAsLoaded();
  PumpLoop();

  model()->CompleteSavingAsSuccess();
  PumpLoop();
  EXPECT_EQ(Offliner::RequestStatus::SAVED, request_status());

  // One extra part should be added if the flag is on.
  content::MHTMLExtraParts* extra_parts =
      content::MHTMLExtraParts::FromWebContents(loader()->web_contents());
  EXPECT_EQ(extra_parts->size(), 1);
}

}  // namespace offline_pages
