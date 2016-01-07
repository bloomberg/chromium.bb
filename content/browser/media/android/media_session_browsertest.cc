// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_session.h"

#include <stddef.h>

#include <list>
#include <vector>

#include "base/macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "content/browser/media/android/media_session_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::WebContents;
using content::WebContentsObserver;
using content::MediaSession;
using content::MediaSessionObserver;
using content::MediaSessionUmaHelper;

using ::testing::Expectation;

namespace {

class MockMediaSessionObserver : public MediaSessionObserver {
 public:
  MockMediaSessionObserver()
      : received_resume_calls_(0),
        received_suspend_calls_(0) {
  }

  ~MockMediaSessionObserver() override = default;

  // Implements MediaSessionObserver.
  void OnSuspend(int player_id) override {
    DCHECK(player_id >= 0);
    DCHECK(players_.size() > static_cast<size_t>(player_id));

    ++received_suspend_calls_;
    players_[player_id] = false;
  }
  void OnResume(int player_id) override {
    DCHECK(player_id >= 0);
    DCHECK(players_.size() > static_cast<size_t>(player_id));

    ++received_resume_calls_;
    players_[player_id] = true;
  }

  int StartNewPlayer() {
    players_.push_back(true);
    return players_.size() - 1;
  }

  bool IsPlaying(size_t player_id) {
    DCHECK(players_.size() > player_id);
    return players_[player_id];
  }

  void SetPlaying(size_t player_id, bool playing) {
    DCHECK(players_.size() > player_id);
    players_[player_id] = playing;
  }

  int received_suspend_calls() const {
    return received_suspend_calls_;
  }

  int received_resume_calls() const {
    return received_resume_calls_;
  }

 private:
  // Basic representation of the players. The position in the vector is the
  // player_id. The value of the vector is the playing status.
  std::vector<bool> players_;

  int received_resume_calls_;
  int received_suspend_calls_;
};

class MockWebContentsObserver : public WebContentsObserver {
 public:
  MockWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  MOCK_METHOD2(MediaSessionStateChanged,
               void(bool is_controllable, bool is_suspended));
};

}  // namespace

class MediaSessionBrowserTest : public content::ContentBrowserTest {
 protected:
  MediaSessionBrowserTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    mock_web_contents_observer_.reset(
        new MockWebContentsObserver(shell()->web_contents()));
    media_session_ = MediaSession::Get(shell()->web_contents());
    media_session_->ResetJavaRefForTest();
    ASSERT_TRUE(media_session_);
  }

  void TearDownOnMainThread() override {
    mock_web_contents_observer_.reset();

    media_session_->RemoveAllPlayersForTest();
    media_session_ = nullptr;

    ContentBrowserTest::TearDownOnMainThread();
  }

  void StartNewPlayer(MockMediaSessionObserver* media_session_observer,
                      MediaSession::Type type) {
    bool result = AddPlayer(
        media_session_observer,
        media_session_observer->StartNewPlayer(),
        type);
    EXPECT_TRUE(result);
  }

  bool AddPlayer(MockMediaSessionObserver* media_session_observer,
                 int player_id,
                 MediaSession::Type type) {
    return media_session_->AddPlayer(media_session_observer, player_id, type);
  }

  void RemovePlayer(MockMediaSessionObserver* media_session_observer,
                    int player_id) {
    media_session_->RemovePlayer(media_session_observer, player_id);
  }

  void RemovePlayers(MockMediaSessionObserver* media_session_observer) {
    media_session_->RemovePlayers(media_session_observer);
  }

  void OnPlayerPaused(MockMediaSessionObserver* media_session_observer,
                      int player_id) {
    media_session_->OnPlayerPaused(media_session_observer, player_id);
  }

  bool HasAudioFocus() { return media_session_->IsActiveForTest(); }

  MediaSession::Type GetSessionType() {
    return media_session_->audio_focus_type_for_test();
  }

  bool IsControllable() { return media_session_->IsControllable(); }

  bool IsSuspended() { return media_session_->IsSuspended(); }

  void UIResume() {
    media_session_->Resume();
  }

  void SystemResume() {
    media_session_->OnResume(nullptr, nullptr);
  }

  void UISuspend() {
    media_session_->Suspend();
  }

  void SystemSuspend(bool temporary) {
    media_session_->OnSuspend(nullptr, nullptr, temporary);
  }

  MockWebContentsObserver* mock_web_contents_observer() {
    return mock_web_contents_observer_.get();
  }

  scoped_ptr<MediaSession> CreateDummyMediaSession() {
      return scoped_ptr<MediaSession>(new MediaSession(nullptr));
  }

  MediaSessionUmaHelper* GetMediaSessionUMAHelper() {
    return media_session_->uma_helper_for_test();
  }

 protected:
  MediaSession* media_session_;
  scoped_ptr<MockWebContentsObserver> mock_web_contents_observer_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       PlayersFromSameObserverDoNotStopEachOtherInSameSession) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  EXPECT_TRUE(media_session_observer->IsPlaying(0));
  EXPECT_TRUE(media_session_observer->IsPlaying(1));
  EXPECT_TRUE(media_session_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       PlayersFromManyObserverDoNotStopEachOtherInSameSession) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer_1(
      new MockMediaSessionObserver);
  scoped_ptr<MockMediaSessionObserver> media_session_observer_2(
      new MockMediaSessionObserver);
  scoped_ptr<MockMediaSessionObserver> media_session_observer_3(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer_1.get(), MediaSession::Type::Content);

  StartNewPlayer(media_session_observer_2.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer_3.get(), MediaSession::Type::Content);

  EXPECT_TRUE(media_session_observer_1->IsPlaying(0));
  EXPECT_TRUE(media_session_observer_2->IsPlaying(0));
  EXPECT_TRUE(media_session_observer_3->IsPlaying(0));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       SuspendedMediaSessionStopsPlayers) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  SystemSuspend(true);

  EXPECT_FALSE(media_session_observer->IsPlaying(0));
  EXPECT_FALSE(media_session_observer->IsPlaying(1));
  EXPECT_FALSE(media_session_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ResumedMediaSessionRestartsPlayers) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  SystemSuspend(true);
  SystemResume();

  EXPECT_TRUE(media_session_observer->IsPlaying(0));
  EXPECT_TRUE(media_session_observer->IsPlaying(1));
  EXPECT_TRUE(media_session_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       StartedPlayerOnSuspendedSessionPlaysAlone) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  EXPECT_TRUE(media_session_observer->IsPlaying(0));

  SystemSuspend(true);

  EXPECT_FALSE(media_session_observer->IsPlaying(0));

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  EXPECT_FALSE(media_session_observer->IsPlaying(0));
  EXPECT_TRUE(media_session_observer->IsPlaying(1));

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  EXPECT_FALSE(media_session_observer->IsPlaying(0));
  EXPECT_TRUE(media_session_observer->IsPlaying(1));
  EXPECT_TRUE(media_session_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, AudioFocusInitialState) {
  EXPECT_FALSE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, StartPlayerGivesFocus) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  EXPECT_TRUE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, SuspendGivesAwayAudioFocus) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  SystemSuspend(true);

  EXPECT_FALSE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, StopGivesAwayAudioFocus) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  media_session_->Stop();

  EXPECT_FALSE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, ResumeGivesBackAudioFocus) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  SystemSuspend(true);
  SystemResume();

  EXPECT_TRUE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       RemovingLastPlayerDropsAudioFocus) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  RemovePlayer(media_session_observer.get(), 0);
  EXPECT_TRUE(HasAudioFocus());
  RemovePlayer(media_session_observer.get(), 1);
  EXPECT_TRUE(HasAudioFocus());
  RemovePlayer(media_session_observer.get(), 2);
  EXPECT_FALSE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       RemovingLastPlayerFromManyObserversDropsAudioFocus) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer_1(
      new MockMediaSessionObserver);
  scoped_ptr<MockMediaSessionObserver> media_session_observer_2(
      new MockMediaSessionObserver);
  scoped_ptr<MockMediaSessionObserver> media_session_observer_3(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer_1.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer_2.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer_3.get(), MediaSession::Type::Content);

  RemovePlayer(media_session_observer_1.get(), 0);
  EXPECT_TRUE(HasAudioFocus());
  RemovePlayer(media_session_observer_2.get(), 0);
  EXPECT_TRUE(HasAudioFocus());
  RemovePlayer(media_session_observer_3.get(), 0);
  EXPECT_FALSE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       RemovingAllPlayersFromObserversDropsAudioFocus) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer_1(
      new MockMediaSessionObserver);
  scoped_ptr<MockMediaSessionObserver> media_session_observer_2(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer_1.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer_1.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer_2.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer_2.get(), MediaSession::Type::Content);

  RemovePlayers(media_session_observer_1.get());
  EXPECT_TRUE(HasAudioFocus());
  RemovePlayers(media_session_observer_2.get());
  EXPECT_FALSE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, ResumePlayGivesAudioFocus) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  RemovePlayer(media_session_observer.get(), 0);
  EXPECT_FALSE(HasAudioFocus());

  EXPECT_TRUE(
      AddPlayer(media_session_observer.get(), 0, MediaSession::Type::Content));
  EXPECT_TRUE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ResumeSuspendAreSentOnlyOncePerPlayers) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  EXPECT_EQ(0, media_session_observer->received_suspend_calls());
  EXPECT_EQ(0, media_session_observer->received_resume_calls());

  SystemSuspend(true);
  EXPECT_EQ(3, media_session_observer->received_suspend_calls());

  SystemResume();
  EXPECT_EQ(3, media_session_observer->received_resume_calls());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ResumeSuspendAreSentOnlyOncePerPlayersAddedTwice) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  // Adding the three players above again.
  EXPECT_TRUE(
      AddPlayer(media_session_observer.get(), 0, MediaSession::Type::Content));
  EXPECT_TRUE(
      AddPlayer(media_session_observer.get(), 1, MediaSession::Type::Content));
  EXPECT_TRUE(
      AddPlayer(media_session_observer.get(), 2, MediaSession::Type::Content));

  EXPECT_EQ(0, media_session_observer->received_suspend_calls());
  EXPECT_EQ(0, media_session_observer->received_resume_calls());

  SystemSuspend(true);
  EXPECT_EQ(3, media_session_observer->received_suspend_calls());

  SystemResume();
  EXPECT_EQ(3, media_session_observer->received_resume_calls());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       RemovingTheSamePlayerTwiceIsANoop) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  RemovePlayer(media_session_observer.get(), 0);
  RemovePlayer(media_session_observer.get(), 0);
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, MediaSessionType) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  // Starting a player with a given type should set the session to that type.
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Transient);
  EXPECT_EQ(MediaSession::Type::Transient, GetSessionType());

  // Adding a player of the same type should have no effect on the type.
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Transient);
  EXPECT_EQ(MediaSession::Type::Transient, GetSessionType());

  // Adding a player of Content type should override the current type.
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  EXPECT_EQ(MediaSession::Type::Content, GetSessionType());

  // Adding a player of the Transient type should have no effect on the type.
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Transient);
  EXPECT_EQ(MediaSession::Type::Content, GetSessionType());

  EXPECT_TRUE(media_session_observer->IsPlaying(0));
  EXPECT_TRUE(media_session_observer->IsPlaying(1));
  EXPECT_TRUE(media_session_observer->IsPlaying(2));
  EXPECT_TRUE(media_session_observer->IsPlaying(3));

  SystemSuspend(true);

  EXPECT_FALSE(media_session_observer->IsPlaying(0));
  EXPECT_FALSE(media_session_observer->IsPlaying(1));
  EXPECT_FALSE(media_session_observer->IsPlaying(2));
  EXPECT_FALSE(media_session_observer->IsPlaying(3));

  EXPECT_EQ(MediaSession::Type::Content, GetSessionType());

  SystemResume();

  EXPECT_TRUE(media_session_observer->IsPlaying(0));
  EXPECT_TRUE(media_session_observer->IsPlaying(1));
  EXPECT_TRUE(media_session_observer->IsPlaying(2));
  EXPECT_TRUE(media_session_observer->IsPlaying(3));

  EXPECT_EQ(MediaSession::Type::Content, GetSessionType());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, ControlsShowForContent) {
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(true, false));

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  // Starting a player with a content type should show the media controls.
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, ControlsNoShowForTransient) {
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(false, false));

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  // Starting a player with a transient type should not show the media controls.
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Transient);

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, ControlsHideWhenStopped) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(false, true))
      .After(showControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  RemovePlayers(media_session_observer.get());

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, ControlsShownAcceptTransient) {
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(true, false));

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  // Transient player join the session without affecting the controls.
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Transient);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ControlsShownAfterContentAdded) {
  Expectation dontShowControls = EXPECT_CALL(
      *mock_web_contents_observer(), MediaSessionStateChanged(false, false));
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(true, false))
      .After(dontShowControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Transient);

  // The controls are shown when the content player is added.
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ControlsStayIfOnlyOnePlayerHasBeenPaused) {
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(true, false));

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Transient);

  // Removing only content player doesn't hide the controls since the session
  // is still active.
  RemovePlayer(media_session_observer.get(), 0);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ControlsHideWhenTheLastPlayerIsRemoved) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(false, true))
      .After(showControls);
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  RemovePlayer(media_session_observer.get(), 0);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());

  RemovePlayer(media_session_observer.get(), 1);

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ControlsHideWhenAllThePlayersAreRemoved) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(false, true))
      .After(showControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  RemovePlayers(media_session_observer.get());

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ControlsNotHideWhenTheLastPlayerIsPaused) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(true, true))
      .After(showControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  OnPlayerPaused(media_session_observer.get(), 0);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());

  OnPlayerPaused(media_session_observer.get(), 1);

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       SuspendTemporaryUpdatesControls) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(true, true))
      .After(showControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  SystemSuspend(true);

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, ControlsUpdatedWhenResumed) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_web_contents_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(true, false))
      .After(pauseControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  SystemSuspend(true);
  SystemResume();

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ControlsHideWhenSessionSuspendedPermanently) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(false, true))
      .After(showControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  SystemSuspend(false);

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ConstrolsHideWhenSessionStops) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_web_contents_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(false, true))
      .After(pauseControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  media_session_->Stop();

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ControlsHideWhenSessionChangesFromContentToTransient) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_web_contents_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(false, false))
      .After(pauseControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  SystemSuspend(true);

  // This should reset the session and change it to a transient, so
  // hide the controls.
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Transient);

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ControlsUpdatedWhenNewPlayerResetsSession) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_web_contents_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(true, false))
      .After(pauseControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  SystemSuspend(true);

  // This should reset the session and update the controls.
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ControlsResumedWhenPlayerIsResumed) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_web_contents_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(true, false))
      .After(pauseControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  SystemSuspend(true);

  // This should resume the session and update the controls.
  AddPlayer(media_session_observer.get(), 0, MediaSession::Type::Content);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ControlsUpdatedDueToResumeSessionAction) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(true, true)).After(showControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  UISuspend();

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ControlsUpdatedDueToSuspendSessionAction) {
  Expectation showControls = EXPECT_CALL(*mock_web_contents_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_web_contents_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_web_contents_observer(),
              MediaSessionStateChanged(true, false))
      .After(pauseControls);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  UISuspend();
  UIResume();

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       DontResumeBySystemUISuspendedSessions) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  UISuspend();
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());

  SystemResume();
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       AllowUIResumeForSystemSuspend) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  SystemSuspend(true);
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());

  UIResume();
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, ResumeSuspendFromUI) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  UISuspend();
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());

  UIResume();
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, ResumeSuspendFromSystem) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);
  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  SystemSuspend(true);
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());

  SystemResume();
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, UMA_Suspended_SystemTransient) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);
  base::HistogramTester tester;

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  SystemSuspend(true);

  scoped_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0)); // System Transient
  EXPECT_EQ(0, samples->GetCount(1)); // System Permanent
  EXPECT_EQ(0, samples->GetCount(2)); // UI
}


IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       UMA_Suspended_SystemPermantent) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);
  base::HistogramTester tester;

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  SystemSuspend(false);

  scoped_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(0, samples->GetCount(0)); // System Transient
  EXPECT_EQ(1, samples->GetCount(1)); // System Permanent
  EXPECT_EQ(0, samples->GetCount(2)); // UI
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, UMA_Suspended_UI) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);
  base::HistogramTester tester;

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  UISuspend();

  scoped_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(0, samples->GetCount(0)); // System Transient
  EXPECT_EQ(0, samples->GetCount(1)); // System Permanent
  EXPECT_EQ(1, samples->GetCount(2)); // UI
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, UMA_Suspended_Multiple) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);
  base::HistogramTester tester;

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  UISuspend();
  UIResume();

  SystemSuspend(true);
  SystemResume();

  UISuspend();
  UIResume();

  SystemSuspend(false);

  scoped_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(4, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0)); // System Transient
  EXPECT_EQ(1, samples->GetCount(1)); // System Permanent
  EXPECT_EQ(2, samples->GetCount(2)); // UI
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, UMA_Suspended_Crossing) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);
  base::HistogramTester tester;

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  UISuspend();
  SystemSuspend(true);
  SystemSuspend(false);
  UIResume();

  SystemSuspend(true);
  SystemSuspend(true);
  SystemSuspend(false);
  SystemResume();

  scoped_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0)); // System Transient
  EXPECT_EQ(0, samples->GetCount(1)); // System Permanent
  EXPECT_EQ(1, samples->GetCount(2)); // UI
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, UMA_Suspended_Stop) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);
  base::HistogramTester tester;

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  media_session_->Stop();

  scoped_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(0, samples->GetCount(0)); // System Transient
  EXPECT_EQ(0, samples->GetCount(1)); // System Permanent
  EXPECT_EQ(1, samples->GetCount(2)); // UI
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, UMA_ActiveTime_NoActivation) {
  base::HistogramTester tester;

  scoped_ptr<MediaSession> media_session = CreateDummyMediaSession();
  media_session.reset();

  // A MediaSession that wasn't active doesn't register an active time.
  scoped_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(0, samples->TotalCount());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       UMA_ActiveTime_SimpleActivation) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
    new MockMediaSessionObserver);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  clock->SetNow(base::Time::Now());
  media_session_uma_helper->SetClockForTest(
      scoped_ptr<base::SimpleTestClock>(clock));

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  media_session_->Stop();

  scoped_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(1000));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       UMA_ActiveTime_ActivationWithUISuspension) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
    new MockMediaSessionObserver);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  clock->SetNow(base::Time::Now());
  media_session_uma_helper->SetClockForTest(
      scoped_ptr<base::SimpleTestClock>(clock));

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  UISuspend();

  clock->Advance(base::TimeDelta::FromMilliseconds(2000));
  UIResume();

  clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  media_session_->Stop();

  scoped_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(2000));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       UMA_ActiveTime_ActivationWithSystemSuspension) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
    new MockMediaSessionObserver);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  clock->SetNow(base::Time::Now());
  media_session_uma_helper->SetClockForTest(
      scoped_ptr<base::SimpleTestClock>(clock));

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);

  clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  SystemSuspend(true);

  clock->Advance(base::TimeDelta::FromMilliseconds(2000));
  SystemResume();

  clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  media_session_->Stop();

  scoped_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(2000));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       UMA_ActiveTime_ActivateSuspendedButNotStopped) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
    new MockMediaSessionObserver);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  clock->SetNow(base::Time::Now());
  media_session_uma_helper->SetClockForTest(
      scoped_ptr<base::SimpleTestClock>(clock));

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  clock->Advance(base::TimeDelta::FromMilliseconds(500));
  SystemSuspend(true);

  {
    scoped_ptr<base::HistogramSamples> samples(
        tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
    EXPECT_EQ(0, samples->TotalCount());
  }

  SystemResume();
  clock->Advance(base::TimeDelta::FromMilliseconds(5000));
  UISuspend();

  {
    scoped_ptr<base::HistogramSamples> samples(
        tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
    EXPECT_EQ(0, samples->TotalCount());
  }
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       UMA_ActiveTime_ActivateSuspendStopTwice) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
    new MockMediaSessionObserver);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  clock->SetNow(base::Time::Now());
  media_session_uma_helper->SetClockForTest(
      scoped_ptr<base::SimpleTestClock>(clock));

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  clock->Advance(base::TimeDelta::FromMilliseconds(500));
  SystemSuspend(true);
  media_session_->Stop();

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  clock->Advance(base::TimeDelta::FromMilliseconds(5000));
  SystemResume();
  media_session_->Stop();

  scoped_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(500));
  EXPECT_EQ(1, samples->GetCount(5000));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       UMA_ActiveTime_MultipleActivations) {
  scoped_ptr<MockMediaSessionObserver> media_session_observer(
    new MockMediaSessionObserver);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  clock->SetNow(base::Time::Now());
  media_session_uma_helper->SetClockForTest(
      scoped_ptr<base::SimpleTestClock>(clock));

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  clock->Advance(base::TimeDelta::FromMilliseconds(10000));
  RemovePlayer(media_session_observer.get(), 0);

  StartNewPlayer(media_session_observer.get(), MediaSession::Type::Content);
  clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  media_session_->Stop();

  scoped_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(1000));
  EXPECT_EQ(1, samples->GetCount(10000));
}
