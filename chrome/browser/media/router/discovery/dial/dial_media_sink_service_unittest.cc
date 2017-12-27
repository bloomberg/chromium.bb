// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace media_router {

namespace {

MediaSinkInternal CreateTestDialSink() {
  DialSinkExtraData extra_data;
  EXPECT_TRUE(extra_data.ip_address.AssignFromIPLiteral("192.168.0.11"));
  extra_data.model_name = "model name";
  extra_data.app_url = GURL("https://192.168.0.11/app_url");
  return MediaSinkInternal(MediaSink("sink_id", "name", SinkIconType::GENERIC),
                           extra_data);
}

}  // namespace

class MockDialMediaSinkServiceImpl : public DialMediaSinkServiceImpl {
 public:
  MockDialMediaSinkServiceImpl(
      const OnSinksDiscoveredCallback& sinks_discovered_cb,
      const OnDialSinkAddedCallback& dial_sink_added_cb,
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : DialMediaSinkServiceImpl(std::unique_ptr<service_manager::Connector>(),
                                 sinks_discovered_cb,
                                 dial_sink_added_cb,
                                 request_context,
                                 task_runner),
        sinks_discovered_cb_(sinks_discovered_cb),
        dial_sink_added_cb_(dial_sink_added_cb) {}
  ~MockDialMediaSinkServiceImpl() override = default;

  MOCK_METHOD0(Start, void());

  OnSinksDiscoveredCallback sinks_discovered_cb() {
    return sinks_discovered_cb_;
  }

  OnDialSinkAddedCallback dial_sink_added_cb() { return dial_sink_added_cb_; }

 private:
  OnSinksDiscoveredCallback sinks_discovered_cb_;
  OnDialSinkAddedCallback dial_sink_added_cb_;
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
             const scoped_refptr<net::URLRequestContextGetter>& request_context)
      override {
    auto mock_impl = std::unique_ptr<MockDialMediaSinkServiceImpl,
                                     base::OnTaskRunnerDeleter>(
        new MockDialMediaSinkServiceImpl(sinks_discovered_cb,
                                         dial_sink_added_cb, request_context,
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
  MediaSinkInternal sink = CreateTestDialSink();

  EXPECT_CALL(*this, OnDialSinkAdded(sink));
  mock_impl_->dial_sink_added_cb().Run(sink);
}

}  // namespace media_router
