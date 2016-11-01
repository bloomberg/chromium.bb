// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <stddef.h>

#include <list>
#include <vector>

#include "base/macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/browser/media/session/audio_focus_delegate.h"
#include "content/browser/media/session/mock_media_session_player_observer.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_content_type.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::WebContents;
using content::MediaSession;
using content::MediaSessionImpl;
using content::MediaSessionObserver;
using content::AudioFocusDelegate;
using content::MediaSessionPlayerObserver;
using content::MediaSessionUmaHelper;
using content::MockMediaSessionPlayerObserver;

using ::testing::Expectation;

namespace {

const double kDefaultVolumeMultiplier = 1.0;
const double kDuckingVolumeMultiplier = 0.2;

class MockAudioFocusDelegate : public AudioFocusDelegate {
 public:
  bool RequestAudioFocus(content::AudioFocusManager::AudioFocusType) override {
    return true;
  }

  void AbandonAudioFocus() override {}
};

class MockMediaSessionObserver : public MediaSessionObserver {
 public:
  MockMediaSessionObserver(MediaSession* media_session)
      : MediaSessionObserver(media_session) {}

  MOCK_METHOD2(MediaSessionStateChanged,
               void(bool is_controllable, bool is_suspended));
  MOCK_METHOD0(MediaSessionDestroyed, void());
};

}  // namespace

class MediaSessionImplBrowserTest : public content::ContentBrowserTest {
 protected:
  MediaSessionImplBrowserTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    media_session_ = MediaSessionImpl::Get(shell()->web_contents());
    mock_media_session_observer_.reset(
        new MockMediaSessionObserver(media_session_));
    media_session_->SetDelegateForTests(
        std::unique_ptr<AudioFocusDelegate>(new MockAudioFocusDelegate()));
    ASSERT_TRUE(media_session_);
  }

  void TearDownOnMainThread() override {
    mock_media_session_observer_.reset();

    media_session_->RemoveAllPlayersForTest();
    media_session_ = nullptr;

    ContentBrowserTest::TearDownOnMainThread();
  }

  void StartNewPlayer(MockMediaSessionPlayerObserver* player_observer,
                      media::MediaContentType media_content_type) {
    bool result = AddPlayer(player_observer, player_observer->StartNewPlayer(),
                            media_content_type);
    EXPECT_TRUE(result);
  }

  bool AddPlayer(MockMediaSessionPlayerObserver* player_observer,
                 int player_id,
                 media::MediaContentType type) {
    return media_session_->AddPlayer(player_observer, player_id, type);
  }

  void RemovePlayer(MockMediaSessionPlayerObserver* player_observer,
                    int player_id) {
    media_session_->RemovePlayer(player_observer, player_id);
  }

  void RemovePlayers(MockMediaSessionPlayerObserver* player_observer) {
    media_session_->RemovePlayers(player_observer);
  }

  void OnPlayerPaused(MockMediaSessionPlayerObserver* player_observer,
                      int player_id) {
    media_session_->OnPlayerPaused(player_observer, player_id);
  }

  bool HasAudioFocus() { return media_session_->IsActiveForTest(); }

  content::AudioFocusManager::AudioFocusType GetSessionAudioFocusType() {
    return media_session_->audio_focus_type();
  }

  bool IsControllable() { return media_session_->IsControllable(); }

  bool IsSuspended() { return media_session_->IsSuspended(); }

  void UIResume() { media_session_->Resume(MediaSession::SuspendType::UI); }

  void SystemResume() {
    media_session_->OnResumeInternal(MediaSession::SuspendType::SYSTEM);
  }

  void UISuspend() { media_session_->Suspend(MediaSession::SuspendType::UI); }

  void SystemSuspend(bool temporary) {
    media_session_->OnSuspendInternal(MediaSession::SuspendType::SYSTEM,
                                      temporary
                                          ? MediaSessionImpl::State::SUSPENDED
                                          : MediaSessionImpl::State::INACTIVE);
  }

  void SystemStartDucking() { media_session_->StartDucking(); }

  void SystemStopDucking() { media_session_->StopDucking(); }

  MockMediaSessionObserver* mock_media_session_observer() {
    return mock_media_session_observer_.get();
  }

  std::unique_ptr<MediaSessionImpl> CreateDummyMediaSession() {
    return std::unique_ptr<MediaSessionImpl>(new MediaSessionImpl(nullptr));
  }

  MediaSessionUmaHelper* GetMediaSessionUMAHelper() {
    return media_session_->uma_helper_for_test();
  }

 protected:
  MediaSessionImpl* media_session_;
  std::unique_ptr<MockMediaSessionObserver> mock_media_session_observer_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionImplBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       PlayersFromSameObserverDoNotStopEachOtherInSameSession) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       PlayersFromManyObserverDoNotStopEachOtherInSameSession) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer_1(
      new MockMediaSessionPlayerObserver);
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer_2(
      new MockMediaSessionPlayerObserver);
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer_3(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer_1.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_2.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_3.get(), media::MediaContentType::Persistent);

  EXPECT_TRUE(player_observer_1->IsPlaying(0));
  EXPECT_TRUE(player_observer_2->IsPlaying(0));
  EXPECT_TRUE(player_observer_3->IsPlaying(0));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       SuspendedMediaSessionStopsPlayers) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  SystemSuspend(true);

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_FALSE(player_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ResumedMediaSessionRestartsPlayers) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  SystemSuspend(true);
  SystemResume();

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       StartedPlayerOnSuspendedSessionPlaysAlone) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_TRUE(player_observer->IsPlaying(0));

  SystemSuspend(true);

  EXPECT_FALSE(player_observer->IsPlaying(0));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, InitialVolumeMultiplier) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(1));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       StartDuckingReducesVolumeMultiplier) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  SystemStartDucking();

  EXPECT_EQ(kDuckingVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDuckingVolumeMultiplier, player_observer->GetVolumeMultiplier(1));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_EQ(kDuckingVolumeMultiplier, player_observer->GetVolumeMultiplier(2));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       StopDuckingRecoversVolumeMultiplier) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  SystemStartDucking();
  SystemStopDucking();

  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(1));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(2));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, AudioFocusInitialState) {
  EXPECT_FALSE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, StartPlayerGivesFocus) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_TRUE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       SuspendGivesAwayAudioFocus) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  SystemSuspend(true);

  EXPECT_FALSE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, StopGivesAwayAudioFocus) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  media_session_->Stop(MediaSession::SuspendType::UI);

  EXPECT_FALSE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, ResumeGivesBackAudioFocus) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  SystemSuspend(true);
  SystemResume();

  EXPECT_TRUE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       RemovingLastPlayerDropsAudioFocus) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  RemovePlayer(player_observer.get(), 0);
  EXPECT_TRUE(HasAudioFocus());
  RemovePlayer(player_observer.get(), 1);
  EXPECT_TRUE(HasAudioFocus());
  RemovePlayer(player_observer.get(), 2);
  EXPECT_FALSE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       RemovingLastPlayerFromManyObserversDropsAudioFocus) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer_1(
      new MockMediaSessionPlayerObserver);
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer_2(
      new MockMediaSessionPlayerObserver);
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer_3(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer_1.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_2.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_3.get(), media::MediaContentType::Persistent);

  RemovePlayer(player_observer_1.get(), 0);
  EXPECT_TRUE(HasAudioFocus());
  RemovePlayer(player_observer_2.get(), 0);
  EXPECT_TRUE(HasAudioFocus());
  RemovePlayer(player_observer_3.get(), 0);
  EXPECT_FALSE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       RemovingAllPlayersFromObserversDropsAudioFocus) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer_1(
      new MockMediaSessionPlayerObserver);
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer_2(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer_1.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_1.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_2.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer_2.get(), media::MediaContentType::Persistent);

  RemovePlayers(player_observer_1.get());
  EXPECT_TRUE(HasAudioFocus());
  RemovePlayers(player_observer_2.get());
  EXPECT_FALSE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, ResumePlayGivesAudioFocus) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  RemovePlayer(player_observer.get(), 0);
  EXPECT_FALSE(HasAudioFocus());

  EXPECT_TRUE(
      AddPlayer(player_observer.get(), 0, media::MediaContentType::Persistent));
  EXPECT_TRUE(HasAudioFocus());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ResumeSuspendAreSentOnlyOncePerPlayers) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());

  SystemSuspend(true);
  EXPECT_EQ(3, player_observer->received_suspend_calls());

  SystemResume();
  EXPECT_EQ(3, player_observer->received_resume_calls());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ResumeSuspendAreSentOnlyOncePerPlayersAddedTwice) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  // Adding the three players above again.
  EXPECT_TRUE(
      AddPlayer(player_observer.get(), 0, media::MediaContentType::Persistent));
  EXPECT_TRUE(
      AddPlayer(player_observer.get(), 1, media::MediaContentType::Persistent));
  EXPECT_TRUE(
      AddPlayer(player_observer.get(), 2, media::MediaContentType::Persistent));

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());

  SystemSuspend(true);
  EXPECT_EQ(3, player_observer->received_suspend_calls());

  SystemResume();
  EXPECT_EQ(3, player_observer->received_resume_calls());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       RemovingTheSamePlayerTwiceIsANoop) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  RemovePlayer(player_observer.get(), 0);
  RemovePlayer(player_observer.get(), 0);
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, AudioFocusType) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  // Starting a player with a given type should set the session to that type.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  EXPECT_EQ(content::AudioFocusManager::AudioFocusType::GainTransientMayDuck,
            GetSessionAudioFocusType());

  // Adding a player of the same type should have no effect on the type.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  EXPECT_EQ(content::AudioFocusManager::AudioFocusType::GainTransientMayDuck,
            GetSessionAudioFocusType());

  // Adding a player of Content type should override the current type.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  EXPECT_EQ(content::AudioFocusManager::AudioFocusType::Gain,
            GetSessionAudioFocusType());

  // Adding a player of the Transient type should have no effect on the type.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);
  EXPECT_EQ(content::AudioFocusManager::AudioFocusType::Gain,
            GetSessionAudioFocusType());

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
  EXPECT_TRUE(player_observer->IsPlaying(3));

  SystemSuspend(true);

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_FALSE(player_observer->IsPlaying(2));
  EXPECT_FALSE(player_observer->IsPlaying(3));

  EXPECT_EQ(content::AudioFocusManager::AudioFocusType::Gain,
            GetSessionAudioFocusType());

  SystemResume();

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
  EXPECT_TRUE(player_observer->IsPlaying(3));

  EXPECT_EQ(content::AudioFocusManager::AudioFocusType::Gain,
            GetSessionAudioFocusType());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, ControlsShowForContent) {
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false));

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  // Starting a player with a content type should show the media controls.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsNoShowForTransient) {
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, false));

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  // Starting a player with a transient type should not show the media controls.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, ControlsHideWhenStopped) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, true))
      .After(showControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  RemovePlayers(player_observer.get());

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsShownAcceptTransient) {
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false));

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  // Transient player join the session without affecting the controls.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsShownAfterContentAdded) {
  Expectation dontShowControls = EXPECT_CALL(
      *mock_media_session_observer(), MediaSessionStateChanged(false, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .After(dontShowControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);

  // The controls are shown when the content player is added.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsStayIfOnlyOnePlayerHasBeenPaused) {
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false));

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);

  // Removing only content player doesn't hide the controls since the session
  // is still active.
  RemovePlayer(player_observer.get(), 0);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsHideWhenTheLastPlayerIsRemoved) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, true))
      .After(showControls);
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  RemovePlayer(player_observer.get(), 0);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());

  RemovePlayer(player_observer.get(), 1);

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsHideWhenAllThePlayersAreRemoved) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, true))
      .After(showControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  RemovePlayers(player_observer.get());

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsNotHideWhenTheLastPlayerIsPaused) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, true))
      .After(showControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  OnPlayerPaused(player_observer.get(), 0);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());

  OnPlayerPaused(player_observer.get(), 1);

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       SuspendTemporaryUpdatesControls) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, true))
      .After(showControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  SystemSuspend(true);

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsUpdatedWhenResumed) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_media_session_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .After(pauseControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  SystemSuspend(true);
  SystemResume();

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsHideWhenSessionSuspendedPermanently) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, true))
      .After(showControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  SystemSuspend(false);

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ConstrolsHideWhenSessionStops) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_media_session_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, true))
      .After(pauseControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  media_session_->Stop(MediaSession::SuspendType::UI);

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsHideWhenSessionChangesFromContentToTransient) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_media_session_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(false, false))
      .After(pauseControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  SystemSuspend(true);

  // This should reset the session and change it to a transient, so
  // hide the controls.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Transient);

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsUpdatedWhenNewPlayerResetsSession) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_media_session_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .After(pauseControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  SystemSuspend(true);

  // This should reset the session and update the controls.
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsResumedWhenPlayerIsResumed) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_media_session_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .After(pauseControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  SystemSuspend(true);

  // This should resume the session and update the controls.
  AddPlayer(player_observer.get(), 0, media::MediaContentType::Persistent);

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsUpdatedDueToResumeSessionAction) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, true))
      .After(showControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  UISuspend();

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       ControlsUpdatedDueToSuspendSessionAction) {
  Expectation showControls = EXPECT_CALL(*mock_media_session_observer(),
                                         MediaSessionStateChanged(true, false));
  Expectation pauseControls = EXPECT_CALL(*mock_media_session_observer(),
                                          MediaSessionStateChanged(true, true))
                                  .After(showControls);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionStateChanged(true, false))
      .After(pauseControls);

  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  UISuspend();
  UIResume();

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       DontResumeBySystemUISuspendedSessions) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  UISuspend();
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());

  SystemResume();
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       AllowUIResumeForSystemSuspend) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  SystemSuspend(true);
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());

  UIResume();
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, ResumeSuspendFromUI) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  UISuspend();
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());

  UIResume();
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, ResumeSuspendFromSystem) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  SystemSuspend(true);
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsSuspended());

  SystemResume();
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsSuspended());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       UMA_Suspended_SystemTransient) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  SystemSuspend(true);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));  // System Transient
  EXPECT_EQ(0, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(0, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       UMA_Suspended_SystemPermantent) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  SystemSuspend(false);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(0, samples->GetCount(0));  // System Transient
  EXPECT_EQ(1, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(0, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, UMA_Suspended_UI) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  UISuspend();

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(0, samples->GetCount(0));  // System Transient
  EXPECT_EQ(0, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(1, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, UMA_Suspended_Multiple) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  UISuspend();
  UIResume();

  SystemSuspend(true);
  SystemResume();

  UISuspend();
  UIResume();

  SystemSuspend(false);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(4, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));  // System Transient
  EXPECT_EQ(1, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(2, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, UMA_Suspended_Crossing) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  UISuspend();
  SystemSuspend(true);
  SystemSuspend(false);
  UIResume();

  SystemSuspend(true);
  SystemSuspend(true);
  SystemSuspend(false);
  SystemResume();

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));  // System Transient
  EXPECT_EQ(0, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(1, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, UMA_Suspended_Stop) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  media_session_->Stop(MediaSession::SuspendType::UI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(0, samples->GetCount(0));  // System Transient
  EXPECT_EQ(0, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(1, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       UMA_ActiveTime_NoActivation) {
  base::HistogramTester tester;

  std::unique_ptr<MediaSessionImpl> media_session = CreateDummyMediaSession();
  media_session.reset();

  // A MediaSession that wasn't active doesn't register an active time.
  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(0, samples->TotalCount());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       UMA_ActiveTime_SimpleActivation) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(
      std::unique_ptr<base::SimpleTestTickClock>(clock));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  media_session_->Stop(MediaSession::SuspendType::UI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(1000));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       UMA_ActiveTime_ActivationWithUISuspension) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(
      std::unique_ptr<base::SimpleTestTickClock>(clock));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  UISuspend();

  clock->Advance(base::TimeDelta::FromMilliseconds(2000));
  UIResume();

  clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  media_session_->Stop(MediaSession::SuspendType::UI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(2000));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       UMA_ActiveTime_ActivationWithSystemSuspension) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(
      std::unique_ptr<base::SimpleTestTickClock>(clock));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);

  clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  SystemSuspend(true);

  clock->Advance(base::TimeDelta::FromMilliseconds(2000));
  SystemResume();

  clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  media_session_->Stop(MediaSession::SuspendType::UI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(2000));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       UMA_ActiveTime_ActivateSuspendedButNotStopped) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(
      std::unique_ptr<base::SimpleTestTickClock>(clock));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  clock->Advance(base::TimeDelta::FromMilliseconds(500));
  SystemSuspend(true);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
    EXPECT_EQ(0, samples->TotalCount());
  }

  SystemResume();
  clock->Advance(base::TimeDelta::FromMilliseconds(5000));
  UISuspend();

  {
    std::unique_ptr<base::HistogramSamples> samples(
        tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
    EXPECT_EQ(0, samples->TotalCount());
  }
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       UMA_ActiveTime_ActivateSuspendStopTwice) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(
      std::unique_ptr<base::SimpleTestTickClock>(clock));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  clock->Advance(base::TimeDelta::FromMilliseconds(500));
  SystemSuspend(true);
  media_session_->Stop(MediaSession::SuspendType::UI);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  clock->Advance(base::TimeDelta::FromMilliseconds(5000));
  SystemResume();
  media_session_->Stop(MediaSession::SuspendType::UI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(500));
  EXPECT_EQ(1, samples->GetCount(5000));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       UMA_ActiveTime_MultipleActivations) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(
      std::unique_ptr<base::SimpleTestTickClock>(clock));

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  clock->Advance(base::TimeDelta::FromMilliseconds(10000));
  RemovePlayer(player_observer.get(), 0);

  StartNewPlayer(player_observer.get(), media::MediaContentType::Persistent);
  clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  media_session_->Stop(MediaSession::SuspendType::UI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(1000));
  EXPECT_EQ(1, samples->GetCount(10000));
}
