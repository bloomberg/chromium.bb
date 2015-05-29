// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_session.h"

#include <list>
#include <vector>

#include "content/browser/media/android/media_session_observer.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"

namespace content {

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

class MediaSessionBrowserTest : public ContentBrowserTest  {
 protected:
  MediaSessionBrowserTest() = default;

  void DisableNativeBackend(MediaSession* media_session) {
    media_session->ResetJavaRefForTest();
  }

  void StartNewPlayer(MediaSession* media_session,
                      MockMediaSessionObserver* media_session_observer,
                      MediaSession::Type type) {
    bool result = media_session->AddPlayer(
        media_session_observer,
        media_session_observer->StartNewPlayer(),
        type);
    EXPECT_TRUE(result);
  }

  void SuspendSession(MediaSession* media_session) {
    media_session->OnSuspend(true);
  }

  void ResumeSession(MediaSession* media_session) {
    media_session->OnResume();
  }

  bool HasAudioFocus(MediaSession* media_session) {
    return media_session->IsActiveForTest();
  }

  MediaSession::Type GetSessionType(MediaSession* media_session) {
    return media_session->audio_focus_type_for_test();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaSessionBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       PlayersFromSameObserverDoNotStopEachOtherInSameSession) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  EXPECT_TRUE(media_session_observer->IsPlaying(0));
  EXPECT_TRUE(media_session_observer->IsPlaying(1));
  EXPECT_TRUE(media_session_observer->IsPlaying(2));

  media_session->RemovePlayers(media_session_observer.get());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       PlayersFromManyObserverDoNotStopEachOtherInSameSession) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer_1(
      new MockMediaSessionObserver);
  scoped_ptr<MockMediaSessionObserver> media_session_observer_2(
      new MockMediaSessionObserver);
  scoped_ptr<MockMediaSessionObserver> media_session_observer_3(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer_1.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer_2.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer_3.get(),
                 MediaSession::Type::Content);

  EXPECT_TRUE(media_session_observer_1->IsPlaying(0));
  EXPECT_TRUE(media_session_observer_2->IsPlaying(0));
  EXPECT_TRUE(media_session_observer_3->IsPlaying(0));

  media_session->RemovePlayers(media_session_observer_1.get());
  media_session->RemovePlayers(media_session_observer_2.get());
  media_session->RemovePlayers(media_session_observer_3.get());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       SuspendedMediaSessionStopsPlayers) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  SuspendSession(media_session);

  EXPECT_FALSE(media_session_observer->IsPlaying(0));
  EXPECT_FALSE(media_session_observer->IsPlaying(1));
  EXPECT_FALSE(media_session_observer->IsPlaying(2));

  media_session->RemovePlayers(media_session_observer.get());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ResumedMediaSessionRestartsPlayers) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  SuspendSession(media_session);
  ResumeSession(media_session);

  EXPECT_TRUE(media_session_observer->IsPlaying(0));
  EXPECT_TRUE(media_session_observer->IsPlaying(1));
  EXPECT_TRUE(media_session_observer->IsPlaying(2));

  media_session->RemovePlayers(media_session_observer.get());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       StartedPlayerOnSuspendedSessionPlaysAlone) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  EXPECT_TRUE(media_session_observer->IsPlaying(0));

  SuspendSession(media_session);

  EXPECT_FALSE(media_session_observer->IsPlaying(0));

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  EXPECT_FALSE(media_session_observer->IsPlaying(0));
  EXPECT_TRUE(media_session_observer->IsPlaying(1));

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  EXPECT_FALSE(media_session_observer->IsPlaying(0));
  EXPECT_TRUE(media_session_observer->IsPlaying(1));
  EXPECT_TRUE(media_session_observer->IsPlaying(2));

  media_session->RemovePlayers(media_session_observer.get());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, AudioFocusInitialState) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  EXPECT_FALSE(HasAudioFocus(media_session));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, StartPlayerGivesFocus) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  EXPECT_TRUE(HasAudioFocus(media_session));

  media_session->RemovePlayers(media_session_observer.get());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, SuspendGivesAwayAudioFocus) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  SuspendSession(media_session);

  EXPECT_FALSE(HasAudioFocus(media_session));

  media_session->RemovePlayers(media_session_observer.get());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, ResumeGivesBackAudioFocus) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  SuspendSession(media_session);
  ResumeSession(media_session);

  EXPECT_TRUE(HasAudioFocus(media_session));

  media_session->RemovePlayers(media_session_observer.get());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       RemovingLastPlayerDropsAudioFocus_1) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  media_session->RemovePlayer(media_session_observer.get(), 0);
  EXPECT_TRUE(HasAudioFocus(media_session));
  media_session->RemovePlayer(media_session_observer.get(), 1);
  EXPECT_TRUE(HasAudioFocus(media_session));
  media_session->RemovePlayer(media_session_observer.get(), 2);
  EXPECT_FALSE(HasAudioFocus(media_session));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       RemovingLastPlayerDropsAudioFocus_2) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer_1(
      new MockMediaSessionObserver);
  scoped_ptr<MockMediaSessionObserver> media_session_observer_2(
      new MockMediaSessionObserver);
  scoped_ptr<MockMediaSessionObserver> media_session_observer_3(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer_1.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer_2.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer_3.get(),
                 MediaSession::Type::Content);

  media_session->RemovePlayer(media_session_observer_1.get(), 0);
  EXPECT_TRUE(HasAudioFocus(media_session));
  media_session->RemovePlayer(media_session_observer_2.get(), 0);
  EXPECT_TRUE(HasAudioFocus(media_session));
  media_session->RemovePlayer(media_session_observer_3.get(), 0);
  EXPECT_FALSE(HasAudioFocus(media_session));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       RemovingLastPlayerDropsAudioFocus_3) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer_1(
      new MockMediaSessionObserver);
  scoped_ptr<MockMediaSessionObserver> media_session_observer_2(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer_1.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer_1.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer_2.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer_2.get(),
                 MediaSession::Type::Content);

  media_session->RemovePlayers(media_session_observer_1.get());
  EXPECT_TRUE(HasAudioFocus(media_session));
  media_session->RemovePlayers(media_session_observer_2.get());
  EXPECT_FALSE(HasAudioFocus(media_session));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, ResumePlayGivesAudioFocus) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  media_session->RemovePlayer(media_session_observer.get(), 0);
  EXPECT_FALSE(HasAudioFocus(media_session));

  EXPECT_TRUE(media_session->AddPlayer(media_session_observer.get(), 0,
                                       MediaSession::Type::Content));
  EXPECT_TRUE(HasAudioFocus(media_session));

  media_session->RemovePlayers(media_session_observer.get());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ResumeSuspendAreSentOnlyOncePerPlayers_1) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  EXPECT_EQ(0, media_session_observer->received_suspend_calls());
  EXPECT_EQ(0, media_session_observer->received_resume_calls());

  SuspendSession(media_session);
  EXPECT_EQ(3, media_session_observer->received_suspend_calls());

  ResumeSession(media_session);
  EXPECT_EQ(3, media_session_observer->received_resume_calls());

  media_session->RemovePlayers(media_session_observer.get());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       ResumeSuspendAreSentOnlyOncePerPlayers_2) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  // Adding the three players above again.
  EXPECT_TRUE(media_session->AddPlayer(media_session_observer.get(), 0,
                                       MediaSession::Type::Content));
  EXPECT_TRUE(media_session->AddPlayer(media_session_observer.get(), 1,
                                       MediaSession::Type::Content));
  EXPECT_TRUE(media_session->AddPlayer(media_session_observer.get(), 2,
                                       MediaSession::Type::Content));

  EXPECT_EQ(0, media_session_observer->received_suspend_calls());
  EXPECT_EQ(0, media_session_observer->received_resume_calls());

  SuspendSession(media_session);
  EXPECT_EQ(3, media_session_observer->received_suspend_calls());

  ResumeSession(media_session);
  EXPECT_EQ(3, media_session_observer->received_resume_calls());

  media_session->RemovePlayers(media_session_observer.get());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       RemovingTheSamePlayerTwiceIsANoop) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);

  media_session->RemovePlayer(media_session_observer.get(), 0);
  media_session->RemovePlayer(media_session_observer.get(), 0);

  media_session->RemovePlayers(media_session_observer.get());
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, MediaSessionType) {
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);
  DisableNativeBackend(media_session);

  scoped_ptr<MockMediaSessionObserver> media_session_observer(
      new MockMediaSessionObserver);

  // Starting a player with a given type should set the session to that type.
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Transient);
  EXPECT_EQ(MediaSession::Type::Transient, GetSessionType(media_session));

  // Adding a player of the same type should have no effect on the type.
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Transient);
  EXPECT_EQ(MediaSession::Type::Transient, GetSessionType(media_session));

  // Adding a player of Content type should override the current type.
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Content);
  EXPECT_EQ(MediaSession::Type::Content, GetSessionType(media_session));

  // Adding a player of the Transient type should have no effect on the type.
  StartNewPlayer(media_session, media_session_observer.get(),
                 MediaSession::Type::Transient);
  EXPECT_EQ(MediaSession::Type::Content, GetSessionType(media_session));

  EXPECT_TRUE(media_session_observer->IsPlaying(0));
  EXPECT_TRUE(media_session_observer->IsPlaying(1));
  EXPECT_TRUE(media_session_observer->IsPlaying(2));
  EXPECT_TRUE(media_session_observer->IsPlaying(3));

  SuspendSession(media_session);

  EXPECT_FALSE(media_session_observer->IsPlaying(0));
  EXPECT_FALSE(media_session_observer->IsPlaying(1));
  EXPECT_FALSE(media_session_observer->IsPlaying(2));
  EXPECT_FALSE(media_session_observer->IsPlaying(3));

  EXPECT_EQ(MediaSession::Type::Content, GetSessionType(media_session));

  ResumeSession(media_session);

  EXPECT_TRUE(media_session_observer->IsPlaying(0));
  EXPECT_TRUE(media_session_observer->IsPlaying(1));
  EXPECT_TRUE(media_session_observer->IsPlaying(2));
  EXPECT_TRUE(media_session_observer->IsPlaying(3));

  EXPECT_EQ(MediaSession::Type::Content, GetSessionType(media_session));

  media_session->RemovePlayers(media_session_observer.get());
}

}  // namespace content
