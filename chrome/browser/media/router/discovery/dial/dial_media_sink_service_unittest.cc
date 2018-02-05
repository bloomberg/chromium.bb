// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace media_router {

class MockMediaSinksObserverInternal : public MediaSinksObserver {
 public:
  MockMediaSinksObserverInternal(MediaRouter* router,
                                 const MediaSource& source,
                                 const url::Origin& origin)
      : MediaSinksObserver(router, source, origin) {}
  ~MockMediaSinksObserverInternal() override {}

  MOCK_METHOD2(OnSinksUpdated,
               void(const std::vector<MediaSink>& sinks,
                    const std::vector<url::Origin>& origins));
  MOCK_METHOD1(OnSinksReceived, void(const std::vector<MediaSink>& sinks));
};

class MockDialMediaSinkServiceImpl : public DialMediaSinkServiceImpl {
 public:
  MockDialMediaSinkServiceImpl(
      const OnSinksDiscoveredCallback& sinks_discovered_cb,
      const OnDialSinkAddedCallback& dial_sink_added_cb,
      const OnAvailableSinksUpdatedCallback& available_sinks_updated_cb,
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : DialMediaSinkServiceImpl(std::unique_ptr<service_manager::Connector>(),
                                 sinks_discovered_cb,
                                 dial_sink_added_cb,
                                 available_sinks_updated_cb,
                                 request_context,
                                 task_runner),
        sinks_discovered_cb_(sinks_discovered_cb),
        dial_sink_added_cb_(dial_sink_added_cb),
        available_sinks_updated_cb_(available_sinks_updated_cb) {}
  ~MockDialMediaSinkServiceImpl() override = default;

  MOCK_METHOD0(Start, void());
  MOCK_METHOD1(StartMonitoringAvailableSinksForApp, void(const std::string&));
  MOCK_METHOD1(StopMonitoringAvailableSinksForApp, void(const std::string&));

  OnSinksDiscoveredCallback sinks_discovered_cb() {
    return sinks_discovered_cb_;
  }

  OnDialSinkAddedCallback dial_sink_added_cb() { return dial_sink_added_cb_; }

  OnAvailableSinksUpdatedCallback available_sinks_updated_cb() {
    return available_sinks_updated_cb_;
  }

 private:
  OnSinksDiscoveredCallback sinks_discovered_cb_;
  OnDialSinkAddedCallback dial_sink_added_cb_;
  OnAvailableSinksUpdatedCallback available_sinks_updated_cb_;
};

class TestDialMediaSinkService : public DialMediaSinkService {
 public:
  explicit TestDialMediaSinkService(
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      const scoped_refptr<base::TestSimpleTaskRunner>& task_runner)
      : DialMediaSinkService(request_context), task_runner_(task_runner) {}
  ~TestDialMediaSinkService() override = default;

  std::unique_ptr<DialMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
  CreateImpl(const OnSinksDiscoveredCallback& sinks_discovered_cb,
             const OnDialSinkAddedCallback& dial_sink_added_cb,
             const OnAvailableSinksUpdatedCallback& available_sinks_updated_cb,
             const scoped_refptr<net::URLRequestContextGetter>& request_context)
      override {
    auto mock_impl = std::unique_ptr<MockDialMediaSinkServiceImpl,
                                     base::OnTaskRunnerDeleter>(
        new MockDialMediaSinkServiceImpl(
            sinks_discovered_cb, dial_sink_added_cb, available_sinks_updated_cb,
            request_context, task_runner_),
        base::OnTaskRunnerDeleter(task_runner_));
    mock_impl_ = mock_impl.get();
    return mock_impl;
  }

  MockDialMediaSinkServiceImpl* mock_impl() { return mock_impl_; }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  MockDialMediaSinkServiceImpl* mock_impl_ = nullptr;
};

class DialMediaSinkServiceTest : public ::testing::Test {
 public:
  DialMediaSinkServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        service_(new TestDialMediaSinkService(profile_.GetRequestContext(),
                                              task_runner_)) {}

  void SetUp() override {
    service_->Start(
        base::BindRepeating(&DialMediaSinkServiceTest::OnSinksDiscovered,
                            base::Unretained(this)),
        base::BindRepeating(&DialMediaSinkServiceTest::OnDialSinkAdded,
                            base::Unretained(this)));
    mock_impl_ = service_->mock_impl();
    ASSERT_TRUE(mock_impl_);
    EXPECT_CALL(*mock_impl_, Start()).WillOnce(InvokeWithoutArgs([this]() {
      EXPECT_TRUE(this->task_runner_->RunsTasksInCurrentSequence());
    }));
    task_runner_->RunUntilIdle();
  }

  void TearDown() override {
    service_.reset();
    task_runner_->RunUntilIdle();
  }

  MOCK_METHOD1(OnSinksDiscovered, void(std::vector<MediaSinkInternal> sinks));
  MOCK_METHOD1(OnDialSinkAdded, void(const MediaSinkInternal& sink));

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<TestDialMediaSinkService> service_;
  MockDialMediaSinkServiceImpl* mock_impl_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DialMediaSinkServiceTest);
};

TEST_F(DialMediaSinkServiceTest, OnDialSinkAddedCallback) {
  // Runs the callback on |task_runner_| and expects it to post task back to
  // UI thread.
  MediaSinkInternal sink = CreateDialSink(1);

  EXPECT_CALL(*this, OnDialSinkAdded(sink));
  mock_impl_->dial_sink_added_cb().Run(sink);
}

TEST_F(DialMediaSinkServiceTest, TestAddRemoveSinkQuery) {
  MediaSource youtube_source("cast-dial:YouTube");
  MediaSource netflix_source("cast-dial:Netflix");
  std::vector<url::Origin> youtube_origins = {
      url::Origin::Create(GURL("https://tv.youtube.com")),
      url::Origin::Create(GURL("https://tv-green-qa.youtube.com")),
      url::Origin::Create(GURL("https://tv-release-qa.youtube.com")),
      url::Origin::Create(GURL("https://web-green-qa.youtube.com")),
      url::Origin::Create(GURL("https://web-release-qa.youtube.com")),
      url::Origin::Create(GURL("https://www.youtube.com"))};
  std::vector<url::Origin> netflix_origins = {
      url::Origin::Create(GURL("https://www.netflix.com"))};

  MockMediaRouter mock_router;
  MockMediaSinksObserverInternal observer1(
      &mock_router, youtube_source,
      url::Origin::Create(GURL("https://tv.youtube.com")));
  MockMediaSinksObserverInternal observer2(
      &mock_router, youtube_source,
      url::Origin::Create(GURL("https://www.youtube.com")));
  MockMediaSinksObserverInternal observer3(
      &mock_router, netflix_source,
      url::Origin::Create(GURL("https://www.netflix.com")));
  MockMediaSinksObserverInternal observer4(
      &mock_router, netflix_source,
      url::Origin::Create(GURL("https://www.netflix.com")));

  EXPECT_CALL(*mock_impl_, StartMonitoringAvailableSinksForApp("YouTube"))
      .Times(1);
  EXPECT_CALL(*mock_impl_, StartMonitoringAvailableSinksForApp("Netflix"))
      .Times(1);
  service_->RegisterMediaSinksObserver(&observer1);
  service_->RegisterMediaSinksObserver(&observer3);
  EXPECT_CALL(observer4,
              OnSinksUpdated(std::vector<MediaSink>(), netflix_origins));
  service_->RegisterMediaSinksObserver(&observer4);
  task_runner_->RunUntilIdle();

  MediaSink sink("sink id 1", "sink name 1", SinkIconType::GENERIC);
  MediaSinkInternal sink_internal(sink, DialSinkExtraData());
  std::vector<MediaSink> sinks{sink};

  EXPECT_CALL(observer1, OnSinksUpdated(sinks, youtube_origins));
  mock_impl_->available_sinks_updated_cb().Run("YouTube", {sink_internal});
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(observer2, OnSinksUpdated(sinks, youtube_origins));
  service_->RegisterMediaSinksObserver(&observer2);

  EXPECT_CALL(observer3, OnSinksUpdated(sinks, netflix_origins));
  EXPECT_CALL(observer4, OnSinksUpdated(sinks, netflix_origins));
  mock_impl_->available_sinks_updated_cb().Run("Netflix", {sink_internal});
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*mock_impl_, StopMonitoringAvailableSinksForApp("YouTube"))
      .Times(2);
  service_->UnregisterMediaSinksObserver(&observer1);
  service_->UnregisterMediaSinksObserver(&observer2);
  task_runner_->RunUntilIdle();

  EXPECT_CALL(observer1, OnSinksUpdated(_, _)).Times(0);
  EXPECT_CALL(observer2, OnSinksUpdated(_, _)).Times(0);
  mock_impl_->available_sinks_updated_cb().Run("YouTube", {sink_internal});
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
