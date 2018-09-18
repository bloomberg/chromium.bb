// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/screen_listener_impl.h"

#include "base/make_unique.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Expectation;

using State = ScreenListener::State;

class MockObserver final : public ScreenListener::Observer {
 public:
  ~MockObserver() = default;

  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD0(OnSuspended, void());
  MOCK_METHOD0(OnSearching, void());

  MOCK_METHOD1(OnScreenAdded, void(const ScreenInfo& info));
  MOCK_METHOD1(OnScreenChanged, void(const ScreenInfo& info));
  MOCK_METHOD1(OnScreenRemoved, void(const ScreenInfo& info));
  MOCK_METHOD0(OnAllScreensRemoved, void());

  MOCK_METHOD1(OnError, void(ScreenListenerError));

  MOCK_METHOD1(OnMetrics, void(ScreenListener::Metrics));
};

class MockMdnsDelegate final : public ScreenListenerImpl::Delegate {
 public:
  MockMdnsDelegate() = default;
  ~MockMdnsDelegate() override = default;

  using ScreenListenerImpl::Delegate::SetState;

  MOCK_METHOD0(StartListener, void());
  MOCK_METHOD0(StartAndSuspendListener, void());
  MOCK_METHOD0(StopListener, void());
  MOCK_METHOD0(SuspendListener, void());
  MOCK_METHOD0(ResumeListener, void());
  MOCK_METHOD1(SearchNow, void(State));
};

class ScreenListenerImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    screen_listener_ = MakeUnique<ScreenListenerImpl>(nullptr, &mock_delegate_);
  }

  MockMdnsDelegate mock_delegate_;
  std::unique_ptr<ScreenListenerImpl> screen_listener_;
};

}  // namespace

TEST_F(ScreenListenerImplTest, NormalStartStop) {
  ASSERT_EQ(State::kStopped, screen_listener_->state());

  EXPECT_CALL(mock_delegate_, StartListener());
  EXPECT_TRUE(screen_listener_->Start());
  EXPECT_FALSE(screen_listener_->Start());
  EXPECT_EQ(State::kStarting, screen_listener_->state());

  mock_delegate_.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, screen_listener_->state());

  EXPECT_CALL(mock_delegate_, StopListener());
  EXPECT_TRUE(screen_listener_->Stop());
  EXPECT_FALSE(screen_listener_->Stop());
  EXPECT_EQ(State::kStopping, screen_listener_->state());

  mock_delegate_.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, StopBeforeRunning) {
  EXPECT_CALL(mock_delegate_, StartListener());
  EXPECT_TRUE(screen_listener_->Start());
  EXPECT_EQ(State::kStarting, screen_listener_->state());

  EXPECT_CALL(mock_delegate_, StopListener());
  EXPECT_TRUE(screen_listener_->Stop());
  EXPECT_FALSE(screen_listener_->Stop());
  EXPECT_EQ(State::kStopping, screen_listener_->state());

  mock_delegate_.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, StartSuspended) {
  EXPECT_CALL(mock_delegate_, StartAndSuspendListener());
  EXPECT_CALL(mock_delegate_, StartListener()).Times(0);
  EXPECT_TRUE(screen_listener_->StartAndSuspend());
  EXPECT_FALSE(screen_listener_->Start());
  EXPECT_EQ(State::kStarting, screen_listener_->state());

  mock_delegate_.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, SuspendWhileStarting) {
  EXPECT_CALL(mock_delegate_, StartListener()).Times(1);
  EXPECT_CALL(mock_delegate_, SuspendListener()).Times(1);
  EXPECT_TRUE(screen_listener_->Start());
  EXPECT_TRUE(screen_listener_->Suspend());
  EXPECT_EQ(State::kStarting, screen_listener_->state());

  mock_delegate_.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, SuspendAndResume) {
  EXPECT_TRUE(screen_listener_->Start());
  mock_delegate_.SetState(State::kRunning);

  EXPECT_CALL(mock_delegate_, ResumeListener()).Times(0);
  EXPECT_CALL(mock_delegate_, SuspendListener()).Times(2);
  EXPECT_FALSE(screen_listener_->Resume());
  EXPECT_TRUE(screen_listener_->Suspend());
  EXPECT_TRUE(screen_listener_->Suspend());

  mock_delegate_.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, screen_listener_->state());

  EXPECT_CALL(mock_delegate_, StartListener()).Times(0);
  EXPECT_CALL(mock_delegate_, SuspendListener()).Times(0);
  EXPECT_CALL(mock_delegate_, ResumeListener()).Times(2);
  EXPECT_FALSE(screen_listener_->Start());
  EXPECT_FALSE(screen_listener_->Suspend());
  EXPECT_TRUE(screen_listener_->Resume());
  EXPECT_TRUE(screen_listener_->Resume());

  mock_delegate_.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, screen_listener_->state());

  EXPECT_CALL(mock_delegate_, ResumeListener()).Times(0);
  EXPECT_FALSE(screen_listener_->Resume());
}

TEST_F(ScreenListenerImplTest, SearchWhileRunning) {
  EXPECT_CALL(mock_delegate_, SearchNow(_)).Times(0);
  EXPECT_FALSE(screen_listener_->SearchNow());
  EXPECT_TRUE(screen_listener_->Start());
  mock_delegate_.SetState(State::kRunning);

  EXPECT_CALL(mock_delegate_, SearchNow(State::kRunning)).Times(2);
  EXPECT_TRUE(screen_listener_->SearchNow());
  EXPECT_TRUE(screen_listener_->SearchNow());

  mock_delegate_.SetState(State::kSearching);
  EXPECT_EQ(State::kSearching, screen_listener_->state());

  EXPECT_CALL(mock_delegate_, SearchNow(_)).Times(0);
  EXPECT_FALSE(screen_listener_->SearchNow());

  mock_delegate_.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, SearchWhileSuspended) {
  EXPECT_CALL(mock_delegate_, SearchNow(_)).Times(0);
  EXPECT_FALSE(screen_listener_->SearchNow());
  EXPECT_TRUE(screen_listener_->Start());
  mock_delegate_.SetState(State::kRunning);
  EXPECT_TRUE(screen_listener_->Suspend());
  mock_delegate_.SetState(State::kSuspended);

  EXPECT_CALL(mock_delegate_, SearchNow(State::kSuspended)).Times(2);
  EXPECT_TRUE(screen_listener_->SearchNow());
  EXPECT_TRUE(screen_listener_->SearchNow());

  mock_delegate_.SetState(State::kSearching);
  EXPECT_EQ(State::kSearching, screen_listener_->state());

  mock_delegate_.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, StopWhileSearching) {
  EXPECT_TRUE(screen_listener_->Start());
  mock_delegate_.SetState(State::kRunning);
  EXPECT_TRUE(screen_listener_->SearchNow());
  mock_delegate_.SetState(State::kSearching);

  EXPECT_CALL(mock_delegate_, StopListener());
  EXPECT_TRUE(screen_listener_->Stop());
  EXPECT_FALSE(screen_listener_->Stop());
  EXPECT_EQ(State::kStopping, screen_listener_->state());

  mock_delegate_.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, ResumeWhileSearching) {
  EXPECT_TRUE(screen_listener_->Start());
  mock_delegate_.SetState(State::kRunning);
  EXPECT_TRUE(screen_listener_->Suspend());
  mock_delegate_.SetState(State::kSuspended);
  EXPECT_TRUE(screen_listener_->SearchNow());
  mock_delegate_.SetState(State::kSearching);

  EXPECT_CALL(mock_delegate_, ResumeListener()).Times(2);
  EXPECT_TRUE(screen_listener_->Resume());
  EXPECT_TRUE(screen_listener_->Resume());

  mock_delegate_.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, SuspendWhileSearching) {
  EXPECT_TRUE(screen_listener_->Start());
  mock_delegate_.SetState(State::kRunning);
  EXPECT_TRUE(screen_listener_->SearchNow());
  mock_delegate_.SetState(State::kSearching);

  EXPECT_CALL(mock_delegate_, SuspendListener()).Times(2);
  EXPECT_TRUE(screen_listener_->Suspend());
  EXPECT_TRUE(screen_listener_->Suspend());

  mock_delegate_.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, ObserveTransitions) {
  MockObserver observer;
  MockMdnsDelegate mock_delegate;
  screen_listener_ = MakeUnique<ScreenListenerImpl>(&observer, &mock_delegate);

  screen_listener_->Start();
  Expectation start_from_stopped = EXPECT_CALL(observer, OnStarted());
  mock_delegate.SetState(State::kRunning);

  screen_listener_->SearchNow();
  Expectation search_from_running =
      EXPECT_CALL(observer, OnSearching()).After(start_from_stopped);
  mock_delegate.SetState(State::kSearching);
  EXPECT_CALL(observer, OnStarted());
  mock_delegate.SetState(State::kRunning);

  screen_listener_->Suspend();
  Expectation suspend_from_running =
      EXPECT_CALL(observer, OnSuspended()).After(search_from_running);
  mock_delegate.SetState(State::kSuspended);

  screen_listener_->SearchNow();
  Expectation search_from_suspended =
      EXPECT_CALL(observer, OnSearching()).After(suspend_from_running);
  mock_delegate.SetState(State::kSearching);
  EXPECT_CALL(observer, OnSuspended());
  mock_delegate.SetState(State::kSuspended);

  screen_listener_->Resume();
  Expectation resume_from_suspended =
      EXPECT_CALL(observer, OnStarted()).After(suspend_from_running);
  mock_delegate.SetState(State::kRunning);

  screen_listener_->Stop();
  Expectation stop =
      EXPECT_CALL(observer, OnStopped()).After(resume_from_suspended);
  mock_delegate.SetState(State::kStopped);

  screen_listener_->StartAndSuspend();
  EXPECT_CALL(observer, OnSuspended()).After(stop);
  mock_delegate.SetState(State::kSuspended);
}

TEST_F(ScreenListenerImplTest, ObserveFromSearching) {
  MockObserver observer;
  MockMdnsDelegate mock_delegate;
  screen_listener_ = MakeUnique<ScreenListenerImpl>(&observer, &mock_delegate);

  screen_listener_->Start();
  mock_delegate.SetState(State::kRunning);

  screen_listener_->SearchNow();
  mock_delegate.SetState(State::kSearching);

  screen_listener_->Suspend();
  EXPECT_CALL(observer, OnSuspended());
  mock_delegate.SetState(State::kSuspended);

  EXPECT_TRUE(screen_listener_->SearchNow());
  mock_delegate.SetState(State::kSearching);

  screen_listener_->Resume();
  EXPECT_CALL(observer, OnStarted());
  mock_delegate.SetState(State::kRunning);
}

TEST_F(ScreenListenerImplTest, ScreenObserverPassThrough) {
  const ScreenInfo screen1{"id1", "name1", 1, {{192, 168, 1, 10}, 12345}};
  const ScreenInfo screen2{"id2", "name2", 1, {{192, 168, 1, 11}, 12345}};
  const ScreenInfo screen3{"id3", "name3", 1, {{192, 168, 1, 12}, 12345}};
  const ScreenInfo screen1_alt_name{
      "id1", "name1 alt", 1, {{192, 168, 1, 10}, 12345}};
  MockObserver observer;
  MockMdnsDelegate mock_delegate;
  screen_listener_ = MakeUnique<ScreenListenerImpl>(&observer, &mock_delegate);

  EXPECT_CALL(observer, OnScreenAdded(screen1));
  screen_listener_->OnScreenAdded(screen1);
  EXPECT_THAT(screen_listener_->GetScreens(), ElementsAre(screen1));

  EXPECT_CALL(observer, OnScreenChanged(screen1_alt_name));
  screen_listener_->OnScreenChanged(screen1_alt_name);
  EXPECT_THAT(screen_listener_->GetScreens(), ElementsAre(screen1_alt_name));

  EXPECT_CALL(observer, OnScreenChanged(screen2)).Times(0);
  screen_listener_->OnScreenChanged(screen2);

  EXPECT_CALL(observer, OnScreenRemoved(screen1_alt_name));
  screen_listener_->OnScreenRemoved(screen1_alt_name);
  EXPECT_TRUE(screen_listener_->GetScreens().empty());

  EXPECT_CALL(observer, OnScreenRemoved(screen1_alt_name)).Times(0);
  screen_listener_->OnScreenRemoved(screen1_alt_name);

  EXPECT_CALL(observer, OnScreenAdded(screen2));
  screen_listener_->OnScreenAdded(screen2);
  EXPECT_THAT(screen_listener_->GetScreens(), ElementsAre(screen2));

  EXPECT_CALL(observer, OnScreenAdded(screen3));
  screen_listener_->OnScreenAdded(screen3);
  EXPECT_THAT(screen_listener_->GetScreens(), ElementsAre(screen2, screen3));

  EXPECT_CALL(observer, OnAllScreensRemoved());
  screen_listener_->OnAllScreensRemoved();
  EXPECT_TRUE(screen_listener_->GetScreens().empty());

  EXPECT_CALL(observer, OnAllScreensRemoved()).Times(0);
  screen_listener_->OnAllScreensRemoved();
}

}  // namespace openscreen
