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
#include "chrome/common/media_router/discovery/media_sink_service_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace media_router {

class MockDialMediaSinkServiceImpl : public DialMediaSinkServiceImpl {
 public:
  MockDialMediaSinkServiceImpl(
      const OnSinksDiscoveredCallback& sinks_discovered_cb,
      const OnDialSinkAddedCallback& dial_sink_added_cb,
      const OnAvailableSinksUpdatedCallback& available_sinks_updated_cb,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : DialMediaSinkServiceImpl(std::unique_ptr<service_manager::Connector>(),
                                 sinks_discovered_cb,
                                 dial_sink_added_cb,
                                 available_sinks_updated_cb,
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
      const scoped_refptr<base::TestSimpleTaskRunner>& task_runner)
      : DialMediaSinkService(), task_runner_(task_runner) {}
  ~TestDialMediaSinkService() override = default;

  std::unique_ptr<DialMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
  CreateImpl(const OnSinksDiscoveredCallback& sinks_discovered_cb,
             const OnDialSinkAddedCallback& dial_sink_added_cb,
             const OnAvailableSinksUpdatedCallback& available_sinks_updated_cb)
      override {
    auto mock_impl = std::unique_ptr<MockDialMediaSinkServiceImpl,
                                     base::OnTaskRunnerDeleter>(
        new MockDialMediaSinkServiceImpl(
            sinks_discovered_cb, dial_sink_added_cb, available_sinks_updated_cb,
            task_runner_),
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
  using SinkQueryByAppCallback = DialMediaSinkService::SinkQueryByAppCallback;

  DialMediaSinkServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        service_(new TestDialMediaSinkService(task_runner_)) {}

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
  base::MockCallback<SinkQueryByAppCallback> mock_callback;
  std::string app_name("YouTube");

  EXPECT_CALL(*mock_impl_, StartMonitoringAvailableSinksForApp(app_name));
  auto subscription = service_->StartMonitoringAvailableSinksForApp(
      app_name, mock_callback.Get());
  task_runner_->RunUntilIdle();

  MediaSinkInternal sink_internal = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks{sink_internal};

  EXPECT_CALL(mock_callback, Run(app_name, sinks));
  mock_impl_->available_sinks_updated_cb().Run(app_name, sinks);
  base::RunLoop().RunUntilIdle();

  subscription.reset();
  EXPECT_CALL(*mock_impl_, StopMonitoringAvailableSinksForApp(app_name));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_callback, Run(_, _)).Times(0);
  mock_impl_->available_sinks_updated_cb().Run(app_name, sinks);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaSinkServiceTest, TestAddSinkQuerySameAppSameObserver) {
  base::MockCallback<SinkQueryByAppCallback> mock_callback;
  std::string app_name("YouTube");

  EXPECT_CALL(*mock_impl_, StartMonitoringAvailableSinksForApp(app_name))
      .Times(1);
  auto subscription1 = service_->StartMonitoringAvailableSinksForApp(
      app_name, mock_callback.Get());
  auto subscription2 = service_->StartMonitoringAvailableSinksForApp(
      app_name, mock_callback.Get());
  task_runner_->RunUntilIdle();

  MediaSinkInternal sink_internal = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks{sink_internal};

  EXPECT_CALL(mock_callback, Run(app_name, sinks)).Times(2);
  mock_impl_->available_sinks_updated_cb().Run("YouTube", sinks);
  base::RunLoop().RunUntilIdle();

  subscription1.reset();
  subscription2.reset();
  EXPECT_CALL(*mock_impl_, StopMonitoringAvailableSinksForApp(app_name));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaSinkServiceTest, TestAddSinkQuerySameAppDifferentObservers) {
  base::MockCallback<SinkQueryByAppCallback> mock_callback1;
  base::MockCallback<SinkQueryByAppCallback> mock_callback2;
  std::string app_name("YouTube");

  EXPECT_CALL(*mock_impl_, StartMonitoringAvailableSinksForApp(app_name))
      .Times(1);
  auto subscription1 = service_->StartMonitoringAvailableSinksForApp(
      app_name, mock_callback1.Get());
  auto subscription2 = service_->StartMonitoringAvailableSinksForApp(
      app_name, mock_callback2.Get());
  task_runner_->RunUntilIdle();

  MediaSinkInternal sink_internal = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks{sink_internal};

  EXPECT_CALL(mock_callback1, Run(app_name, sinks));
  EXPECT_CALL(mock_callback2, Run(app_name, sinks));
  mock_impl_->available_sinks_updated_cb().Run(app_name, sinks);
  base::RunLoop().RunUntilIdle();

  subscription1.reset();
  EXPECT_CALL(*mock_impl_, StopMonitoringAvailableSinksForApp(app_name))
      .Times(0);
  base::RunLoop().RunUntilIdle();

  subscription2.reset();
  EXPECT_CALL(*mock_impl_, StopMonitoringAvailableSinksForApp(app_name));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaSinkServiceTest, TestAddSinkQueryDifferentApps) {
  base::MockCallback<SinkQueryByAppCallback> mock_callback1;
  base::MockCallback<SinkQueryByAppCallback> mock_callback2;
  std::string youtube_app_name("YouTube");
  std::string netflix_app_name("Netflix");

  EXPECT_CALL(*mock_impl_,
              StartMonitoringAvailableSinksForApp(youtube_app_name));
  EXPECT_CALL(*mock_impl_,
              StartMonitoringAvailableSinksForApp(netflix_app_name));
  auto subscription1 = service_->StartMonitoringAvailableSinksForApp(
      youtube_app_name, mock_callback1.Get());
  auto subscription2 = service_->StartMonitoringAvailableSinksForApp(
      netflix_app_name, mock_callback2.Get());
  task_runner_->RunUntilIdle();

  MediaSinkInternal sink_internal = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks{sink_internal};

  EXPECT_CALL(mock_callback1, Run(youtube_app_name, sinks));
  mock_impl_->available_sinks_updated_cb().Run(youtube_app_name, sinks);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_callback2, Run(netflix_app_name, sinks));
  mock_impl_->available_sinks_updated_cb().Run(netflix_app_name, sinks);
  base::RunLoop().RunUntilIdle();

  subscription1.reset();
  subscription2.reset();
  EXPECT_CALL(*mock_impl_,
              StopMonitoringAvailableSinksForApp(youtube_app_name));
  EXPECT_CALL(*mock_impl_,
              StopMonitoringAvailableSinksForApp(netflix_app_name));
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
