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

class MockObserver final : public ScreenListenerObserver {
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

  MOCK_METHOD1(OnError, void(ScreenListenerErrorInfo));

  MOCK_METHOD1(OnMetrics, void(ScreenListenerMetrics));
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
  MOCK_METHOD1(SearchNow, void(ScreenListenerState));
};

class ScreenListenerImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    screen_listener_ = MakeUnique<ScreenListenerImpl>(&mock_delegate_);
  }

  MockMdnsDelegate mock_delegate_;
  std::unique_ptr<ScreenListenerImpl> screen_listener_;
};

}  // namespace

TEST_F(ScreenListenerImplTest, NormalStartStop) {
  ASSERT_EQ(ScreenListenerState::kStopped, screen_listener_->state());

  EXPECT_CALL(mock_delegate_, StartListener());
  EXPECT_TRUE(screen_listener_->Start());
  EXPECT_FALSE(screen_listener_->Start());
  EXPECT_EQ(ScreenListenerState::kStarting, screen_listener_->state());

  mock_delegate_.SetState(ScreenListenerState::kRunning);
  EXPECT_EQ(ScreenListenerState::kRunning, screen_listener_->state());

  EXPECT_CALL(mock_delegate_, StopListener());
  EXPECT_TRUE(screen_listener_->Stop());
  EXPECT_FALSE(screen_listener_->Stop());
  EXPECT_EQ(ScreenListenerState::kStopping, screen_listener_->state());

  mock_delegate_.SetState(ScreenListenerState::kStopped);
  EXPECT_EQ(ScreenListenerState::kStopped, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, StopBeforeRunning) {
  EXPECT_CALL(mock_delegate_, StartListener());
  EXPECT_TRUE(screen_listener_->Start());
  EXPECT_EQ(ScreenListenerState::kStarting, screen_listener_->state());

  EXPECT_CALL(mock_delegate_, StopListener());
  EXPECT_TRUE(screen_listener_->Stop());
  EXPECT_FALSE(screen_listener_->Stop());
  EXPECT_EQ(ScreenListenerState::kStopping, screen_listener_->state());

  mock_delegate_.SetState(ScreenListenerState::kStopped);
  EXPECT_EQ(ScreenListenerState::kStopped, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, StartSuspended) {
  EXPECT_CALL(mock_delegate_, StartAndSuspendListener());
  EXPECT_CALL(mock_delegate_, StartListener()).Times(0);
  EXPECT_TRUE(screen_listener_->StartAndSuspend());
  EXPECT_FALSE(screen_listener_->Start());
  EXPECT_EQ(ScreenListenerState::kStarting, screen_listener_->state());

  mock_delegate_.SetState(ScreenListenerState::kSuspended);
  EXPECT_EQ(ScreenListenerState::kSuspended, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, SuspendWhileStarting) {
  EXPECT_CALL(mock_delegate_, StartListener()).Times(1);
  EXPECT_CALL(mock_delegate_, SuspendListener()).Times(1);
  EXPECT_TRUE(screen_listener_->Start());
  EXPECT_TRUE(screen_listener_->Suspend());
  EXPECT_EQ(ScreenListenerState::kStarting, screen_listener_->state());

  mock_delegate_.SetState(ScreenListenerState::kSuspended);
  EXPECT_EQ(ScreenListenerState::kSuspended, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, SuspendAndResume) {
  EXPECT_TRUE(screen_listener_->Start());
  mock_delegate_.SetState(ScreenListenerState::kRunning);

  EXPECT_CALL(mock_delegate_, ResumeListener()).Times(0);
  EXPECT_CALL(mock_delegate_, SuspendListener()).Times(2);
  EXPECT_FALSE(screen_listener_->Resume());
  EXPECT_TRUE(screen_listener_->Suspend());
  EXPECT_TRUE(screen_listener_->Suspend());

  mock_delegate_.SetState(ScreenListenerState::kSuspended);
  EXPECT_EQ(ScreenListenerState::kSuspended, screen_listener_->state());

  EXPECT_CALL(mock_delegate_, StartListener()).Times(0);
  EXPECT_CALL(mock_delegate_, SuspendListener()).Times(0);
  EXPECT_CALL(mock_delegate_, ResumeListener()).Times(2);
  EXPECT_FALSE(screen_listener_->Start());
  EXPECT_FALSE(screen_listener_->Suspend());
  EXPECT_TRUE(screen_listener_->Resume());
  EXPECT_TRUE(screen_listener_->Resume());

  mock_delegate_.SetState(ScreenListenerState::kRunning);
  EXPECT_EQ(ScreenListenerState::kRunning, screen_listener_->state());

  EXPECT_CALL(mock_delegate_, ResumeListener()).Times(0);
  EXPECT_FALSE(screen_listener_->Resume());
}

TEST_F(ScreenListenerImplTest, SearchWhileRunning) {
  EXPECT_CALL(mock_delegate_, SearchNow(_)).Times(0);
  EXPECT_FALSE(screen_listener_->SearchNow());
  EXPECT_TRUE(screen_listener_->Start());
  mock_delegate_.SetState(ScreenListenerState::kRunning);

  EXPECT_CALL(mock_delegate_, SearchNow(ScreenListenerState::kRunning))
      .Times(2);
  EXPECT_TRUE(screen_listener_->SearchNow());
  EXPECT_TRUE(screen_listener_->SearchNow());

  mock_delegate_.SetState(ScreenListenerState::kSearching);
  EXPECT_EQ(ScreenListenerState::kSearching, screen_listener_->state());

  EXPECT_CALL(mock_delegate_, SearchNow(_)).Times(0);
  EXPECT_FALSE(screen_listener_->SearchNow());

  mock_delegate_.SetState(ScreenListenerState::kRunning);
  EXPECT_EQ(ScreenListenerState::kRunning, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, SearchWhileSuspended) {
  EXPECT_CALL(mock_delegate_, SearchNow(_)).Times(0);
  EXPECT_FALSE(screen_listener_->SearchNow());
  EXPECT_TRUE(screen_listener_->Start());
  mock_delegate_.SetState(ScreenListenerState::kRunning);
  EXPECT_TRUE(screen_listener_->Suspend());
  mock_delegate_.SetState(ScreenListenerState::kSuspended);

  EXPECT_CALL(mock_delegate_, SearchNow(ScreenListenerState::kSuspended))
      .Times(2);
  EXPECT_TRUE(screen_listener_->SearchNow());
  EXPECT_TRUE(screen_listener_->SearchNow());

  mock_delegate_.SetState(ScreenListenerState::kSearching);
  EXPECT_EQ(ScreenListenerState::kSearching, screen_listener_->state());

  mock_delegate_.SetState(ScreenListenerState::kSuspended);
  EXPECT_EQ(ScreenListenerState::kSuspended, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, StopWhileSearching) {
  EXPECT_TRUE(screen_listener_->Start());
  mock_delegate_.SetState(ScreenListenerState::kRunning);
  EXPECT_TRUE(screen_listener_->SearchNow());
  mock_delegate_.SetState(ScreenListenerState::kSearching);

  EXPECT_CALL(mock_delegate_, StopListener());
  EXPECT_TRUE(screen_listener_->Stop());
  EXPECT_FALSE(screen_listener_->Stop());
  EXPECT_EQ(ScreenListenerState::kStopping, screen_listener_->state());

  mock_delegate_.SetState(ScreenListenerState::kStopped);
  EXPECT_EQ(ScreenListenerState::kStopped, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, ResumeWhileSearching) {
  EXPECT_TRUE(screen_listener_->Start());
  mock_delegate_.SetState(ScreenListenerState::kRunning);
  EXPECT_TRUE(screen_listener_->Suspend());
  mock_delegate_.SetState(ScreenListenerState::kSuspended);
  EXPECT_TRUE(screen_listener_->SearchNow());
  mock_delegate_.SetState(ScreenListenerState::kSearching);

  EXPECT_CALL(mock_delegate_, ResumeListener()).Times(2);
  EXPECT_TRUE(screen_listener_->Resume());
  EXPECT_TRUE(screen_listener_->Resume());

  mock_delegate_.SetState(ScreenListenerState::kRunning);
  EXPECT_EQ(ScreenListenerState::kRunning, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, SuspendWhileSearching) {
  EXPECT_TRUE(screen_listener_->Start());
  mock_delegate_.SetState(ScreenListenerState::kRunning);
  EXPECT_TRUE(screen_listener_->SearchNow());
  mock_delegate_.SetState(ScreenListenerState::kSearching);

  EXPECT_CALL(mock_delegate_, SuspendListener()).Times(2);
  EXPECT_TRUE(screen_listener_->Suspend());
  EXPECT_TRUE(screen_listener_->Suspend());

  mock_delegate_.SetState(ScreenListenerState::kSuspended);
  EXPECT_EQ(ScreenListenerState::kSuspended, screen_listener_->state());
}

TEST_F(ScreenListenerImplTest, ObserveTransitions) {
  MockObserver observer;
  screen_listener_->SetObserver(&observer);

  screen_listener_->Start();
  Expectation start_from_stopped = EXPECT_CALL(observer, OnStarted());
  mock_delegate_.SetState(ScreenListenerState::kRunning);

  screen_listener_->SearchNow();
  Expectation search_from_running =
      EXPECT_CALL(observer, OnSearching()).After(start_from_stopped);
  mock_delegate_.SetState(ScreenListenerState::kSearching);
  EXPECT_CALL(observer, OnStarted());
  mock_delegate_.SetState(ScreenListenerState::kRunning);

  screen_listener_->Suspend();
  Expectation suspend_from_running =
      EXPECT_CALL(observer, OnSuspended()).After(search_from_running);
  mock_delegate_.SetState(ScreenListenerState::kSuspended);

  screen_listener_->SearchNow();
  Expectation search_from_suspended =
      EXPECT_CALL(observer, OnSearching()).After(suspend_from_running);
  mock_delegate_.SetState(ScreenListenerState::kSearching);
  EXPECT_CALL(observer, OnSuspended());
  mock_delegate_.SetState(ScreenListenerState::kSuspended);

  screen_listener_->Resume();
  Expectation resume_from_suspended =
      EXPECT_CALL(observer, OnStarted()).After(suspend_from_running);
  mock_delegate_.SetState(ScreenListenerState::kRunning);

  screen_listener_->Stop();
  EXPECT_CALL(observer, OnStopped()).After(resume_from_suspended);
  mock_delegate_.SetState(ScreenListenerState::kStopped);
}

TEST_F(ScreenListenerImplTest, ObserveFromSearching) {
  MockObserver observer;
  screen_listener_->SetObserver(&observer);

  screen_listener_->Start();
  mock_delegate_.SetState(ScreenListenerState::kRunning);

  screen_listener_->SearchNow();
  mock_delegate_.SetState(ScreenListenerState::kSearching);

  screen_listener_->Suspend();
  EXPECT_CALL(observer, OnSuspended());
  mock_delegate_.SetState(ScreenListenerState::kSuspended);

  EXPECT_TRUE(screen_listener_->SearchNow());
  mock_delegate_.SetState(ScreenListenerState::kSearching);

  screen_listener_->Resume();
  EXPECT_CALL(observer, OnStarted());
  mock_delegate_.SetState(ScreenListenerState::kRunning);
}

TEST_F(ScreenListenerImplTest, ScreenObserverPassThrough) {
  const ScreenInfo screen1{
      "id1", "name1", "eth0", {{192, 168, 1, 10}, 12345}, {{}, 0}};
  const ScreenInfo screen2{
      "id2", "name2", "eth0", {{192, 168, 1, 11}, 12345}, {{}, 0}};
  const ScreenInfo screen3{
      "id3", "name3", "eth0", {{192, 168, 1, 12}, 12345}, {{}, 0}};
  const ScreenInfo screen1_alt_name{
      "id1", "name1 alt", "eth0", {{192, 168, 1, 10}, 12345}, {{}, 0}};
  MockObserver observer;
  screen_listener_->SetObserver(&observer);

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
