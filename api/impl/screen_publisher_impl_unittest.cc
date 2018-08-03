// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/screen_publisher_impl.h"

#include "base/make_unique.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace {

using ::testing::Expectation;
using State = ScreenPublisher::State;

class MockObserver final : public ScreenPublisher::Observer {
 public:
  ~MockObserver() = default;

  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD0(OnSuspended, void());

  MOCK_METHOD1(OnError, void(ScreenPublisherError));

  MOCK_METHOD1(OnMetrics, void(ScreenPublisher::Metrics));
};

class MockMdnsDelegate final : public ScreenPublisherImpl::Delegate {
 public:
  MockMdnsDelegate() = default;
  ~MockMdnsDelegate() override = default;

  using ScreenPublisherImpl::Delegate::SetState;

  MOCK_METHOD0(StartPublisher, void());
  MOCK_METHOD0(StartAndSuspendPublisher, void());
  MOCK_METHOD0(StopPublisher, void());
  MOCK_METHOD0(SuspendPublisher, void());
  MOCK_METHOD0(ResumePublisher, void());
  MOCK_METHOD1(UpdateFriendlyName, void(const std::string&));
};

class ScreenPublisherImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    screen_publisher_ =
        MakeUnique<ScreenPublisherImpl>(nullptr, &mock_delegate_);
  }

  MockMdnsDelegate mock_delegate_;
  std::unique_ptr<ScreenPublisherImpl> screen_publisher_;
};

}  // namespace

TEST_F(ScreenPublisherImplTest, NormalStartStop) {
  ASSERT_EQ(State::kStopped, screen_publisher_->state());

  EXPECT_CALL(mock_delegate_, StartPublisher());
  EXPECT_TRUE(screen_publisher_->Start());
  EXPECT_FALSE(screen_publisher_->Start());
  EXPECT_EQ(State::kStarting, screen_publisher_->state());

  mock_delegate_.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, screen_publisher_->state());

  EXPECT_CALL(mock_delegate_, StopPublisher());
  EXPECT_TRUE(screen_publisher_->Stop());
  EXPECT_FALSE(screen_publisher_->Stop());
  EXPECT_EQ(State::kStopping, screen_publisher_->state());

  mock_delegate_.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, screen_publisher_->state());
}

TEST_F(ScreenPublisherImplTest, StopBeforeRunning) {
  EXPECT_CALL(mock_delegate_, StartPublisher());
  EXPECT_TRUE(screen_publisher_->Start());
  EXPECT_EQ(State::kStarting, screen_publisher_->state());

  EXPECT_CALL(mock_delegate_, StopPublisher());
  EXPECT_TRUE(screen_publisher_->Stop());
  EXPECT_FALSE(screen_publisher_->Stop());
  EXPECT_EQ(State::kStopping, screen_publisher_->state());

  mock_delegate_.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, screen_publisher_->state());
}

TEST_F(ScreenPublisherImplTest, StartSuspended) {
  EXPECT_CALL(mock_delegate_, StartAndSuspendPublisher());
  EXPECT_CALL(mock_delegate_, StartPublisher()).Times(0);
  EXPECT_TRUE(screen_publisher_->StartAndSuspend());
  EXPECT_FALSE(screen_publisher_->Start());
  EXPECT_EQ(State::kStarting, screen_publisher_->state());

  mock_delegate_.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, screen_publisher_->state());
}

TEST_F(ScreenPublisherImplTest, SuspendAndResume) {
  EXPECT_TRUE(screen_publisher_->Start());
  mock_delegate_.SetState(State::kRunning);

  EXPECT_CALL(mock_delegate_, ResumePublisher()).Times(0);
  EXPECT_CALL(mock_delegate_, SuspendPublisher()).Times(2);
  EXPECT_FALSE(screen_publisher_->Resume());
  EXPECT_TRUE(screen_publisher_->Suspend());
  EXPECT_TRUE(screen_publisher_->Suspend());

  mock_delegate_.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, screen_publisher_->state());

  EXPECT_CALL(mock_delegate_, StartPublisher()).Times(0);
  EXPECT_CALL(mock_delegate_, SuspendPublisher()).Times(0);
  EXPECT_CALL(mock_delegate_, ResumePublisher()).Times(2);
  EXPECT_FALSE(screen_publisher_->Start());
  EXPECT_FALSE(screen_publisher_->Suspend());
  EXPECT_TRUE(screen_publisher_->Resume());
  EXPECT_TRUE(screen_publisher_->Resume());

  mock_delegate_.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, screen_publisher_->state());

  EXPECT_CALL(mock_delegate_, ResumePublisher()).Times(0);
  EXPECT_FALSE(screen_publisher_->Resume());
}

TEST_F(ScreenPublisherImplTest, ObserverTransitions) {
  MockObserver observer;
  MockMdnsDelegate mock_delegate;
  screen_publisher_ =
      MakeUnique<ScreenPublisherImpl>(&observer, &mock_delegate);

  screen_publisher_->Start();
  Expectation start_from_stopped = EXPECT_CALL(observer, OnStarted());
  mock_delegate.SetState(State::kRunning);

  screen_publisher_->Suspend();
  Expectation suspend_from_running =
      EXPECT_CALL(observer, OnSuspended()).After(start_from_stopped);
  mock_delegate.SetState(State::kSuspended);

  screen_publisher_->Resume();
  Expectation resume_from_suspended =
      EXPECT_CALL(observer, OnStarted()).After(suspend_from_running);
  mock_delegate.SetState(State::kRunning);

  screen_publisher_->Stop();
  EXPECT_CALL(observer, OnStopped()).After(resume_from_suspended);
  mock_delegate.SetState(State::kStopped);
}

TEST_F(ScreenPublisherImplTest, FriendlyNamePassThrough) {
  const std::string friendly_name("alpha");
  EXPECT_CALL(mock_delegate_, UpdateFriendlyName(friendly_name));
  screen_publisher_->UpdateFriendlyName(friendly_name);
}

}  // namespace openscreen
