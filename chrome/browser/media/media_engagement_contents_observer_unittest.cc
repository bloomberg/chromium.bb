// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_contents_observer.h"

#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_engagement_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class MediaEngagementContentsObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitFromCommandLine("media-engagement", std::string());

    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(content::WebContentsTester::CreateTestWebContents(
        browser_context(), nullptr));

    MediaEngagementService* service = MediaEngagementService::Get(profile());
    ASSERT_TRUE(service);
    contents_observer_ =
        new MediaEngagementContentsObserver(web_contents(), service);

    playback_timer_ = new base::MockTimer(true, false);
    contents_observer_->SetTimerForTest(base::WrapUnique(playback_timer_));

    SimulateInaudible();
  }

  bool IsTimerRunning() const { return playback_timer_->IsRunning(); }

  bool WasSignificantPlaybackRecorded() const {
    return contents_observer_->significant_playback_recorded_;
  }

  size_t GetSignificantActivePlayersCount() const {
    return contents_observer_->significant_players_.size();
  }

  size_t GetStoredPlayerStatesCount() const {
    return contents_observer_->player_states_.size();
  }

  void SimulatePlaybackStarted(int id) {
    content::WebContentsObserver::MediaPlayerInfo player_info(true, true);
    SimulatePlaybackStarted(player_info, id);
  }

  void SimulateResizeEvent(int id, int size) {
    content::WebContentsObserver::MediaPlayerId player_id =
        std::make_pair(nullptr /* RenderFrameHost */, id);
    contents_observer_->MediaResized(gfx::Size(size, size), player_id);
  }

  void SimulatePlaybackStarted(
      content::WebContentsObserver::MediaPlayerInfo player_info,
      int id) {
    content::WebContentsObserver::MediaPlayerId player_id =
        std::make_pair(nullptr /* RenderFrameHost */, id);
    contents_observer_->MediaStartedPlaying(player_info, player_id);
    SimulateMutedStateChange(id, false);
  }

  void SimulatePlaybackStopped(int id) {
    content::WebContentsObserver::MediaPlayerInfo player_info(true, true);
    content::WebContentsObserver::MediaPlayerId player_id =
        std::make_pair(nullptr /* RenderFrameHost */, id);
    contents_observer_->MediaStoppedPlaying(player_info, player_id);
  }

  void SimulateMutedStateChange(int id, bool muted) {
    content::WebContentsObserver::MediaPlayerId player_id =
        std::make_pair(nullptr /* RenderFrameHost */, id);
    contents_observer_->MediaMutedStatusChanged(player_id, muted);
  }

  void SimulateIsVisible() { contents_observer_->WasShown(); }

  void SimulateIsHidden() { contents_observer_->WasHidden(); }

  bool AreConditionsMet() const {
    return contents_observer_->AreConditionsMet();
  }

  void SimulateSignificantPlaybackRecorded() {
    contents_observer_->significant_playback_recorded_ = true;
  }

  void SimulateSignificantPlaybackTime() {
    contents_observer_->OnSignificantMediaPlaybackTime();
  }

  void SimulatePlaybackTimerFired() { playback_timer_->Fire(); }

  void ExpectScores(GURL url,
                    double expected_score,
                    int expected_visits,
                    int expected_media_playbacks) {
    EXPECT_EQ(contents_observer_->service_->GetEngagementScore(url),
              expected_score);
    EXPECT_EQ(contents_observer_->service_->GetScoreMapForTesting()[url],
              expected_score);

    MediaEngagementScore* score =
        contents_observer_->service_->CreateEngagementScore(url);
    EXPECT_EQ(score->visits(), expected_visits);
    EXPECT_EQ(score->media_playbacks(), expected_media_playbacks);
    delete score;
  }

  void Navigate(GURL url) {
    std::unique_ptr<content::NavigationHandle> test_handle =
        content::NavigationHandle::CreateNavigationHandleForTesting(
            GURL(url), main_rfh(), true /** committed */);
    contents_observer_->DidFinishNavigation(test_handle.get());
    contents_observer_->committed_origin_ = url::Origin(url);
  }

  void SimulateAudible() {
#if defined(OS_ANDROID)
// WasRecentlyAudible is not available on Android.
#else
    content::WebContentsTester::For(web_contents())
        ->SetWasRecentlyAudible(true);
#endif
  }

  void SimulateInaudible() {
    content::WebContentsTester::For(web_contents())
        ->SetWasRecentlyAudible(false);
  }

  void SimulateSignificantPlayer(int id) {
    SimulatePlaybackStarted(id);
    SimulateIsVisible();
    SimulateAudible();
    web_contents()->SetAudioMuted(false);
    SimulateResizeEvent(id, MediaEngagementContentsObserver::kSignificantSize);
  }

  void ForceUpdateTimer() { contents_observer_->UpdateTimer(); }

 private:
  // contents_observer_ auto-destroys when WebContents is destroyed.
  MediaEngagementContentsObserver* contents_observer_;

  base::test::ScopedFeatureList scoped_feature_list_;

  base::MockTimer* playback_timer_;
};

// TODO(mlamouri): test that visits are not recorded multiple times when a
// same-origin navigation happens.

TEST_F(MediaEngagementContentsObserverTest, SignificantActivePlayerCount) {
  EXPECT_EQ(0u, GetSignificantActivePlayersCount());

  SimulatePlaybackStarted(0);
  SimulateResizeEvent(0, MediaEngagementContentsObserver::kSignificantSize);
  EXPECT_EQ(1u, GetSignificantActivePlayersCount());

  SimulatePlaybackStarted(1);
  SimulateResizeEvent(1, MediaEngagementContentsObserver::kSignificantSize);
  EXPECT_EQ(2u, GetSignificantActivePlayersCount());

  SimulatePlaybackStarted(2);
  SimulateResizeEvent(2, MediaEngagementContentsObserver::kSignificantSize);
  EXPECT_EQ(3u, GetSignificantActivePlayersCount());

  SimulatePlaybackStopped(1);
  EXPECT_EQ(2u, GetSignificantActivePlayersCount());

  SimulatePlaybackStopped(0);
  EXPECT_EQ(1u, GetSignificantActivePlayersCount());

  SimulateResizeEvent(2, 1);
  EXPECT_EQ(0u, GetSignificantActivePlayersCount());
}

TEST_F(MediaEngagementContentsObserverTest, AreConditionsMet) {
  EXPECT_FALSE(AreConditionsMet());

  SimulatePlaybackStarted(0);
  SimulateIsVisible();
  web_contents()->SetAudioMuted(false);
  SimulateResizeEvent(0, MediaEngagementContentsObserver::kSignificantSize);
  EXPECT_TRUE(AreConditionsMet());

  SimulateResizeEvent(0, 1);
  EXPECT_FALSE(AreConditionsMet());
  SimulateResizeEvent(0, MediaEngagementContentsObserver::kSignificantSize);

  web_contents()->SetAudioMuted(true);
  EXPECT_FALSE(AreConditionsMet());
  web_contents()->SetAudioMuted(false);

  SimulateIsHidden();
  EXPECT_FALSE(AreConditionsMet());

  SimulateIsVisible();
  SimulatePlaybackStopped(0);
  EXPECT_FALSE(AreConditionsMet());

  SimulatePlaybackStarted(0);
  EXPECT_TRUE(AreConditionsMet());

  SimulateMutedStateChange(0, true);
  EXPECT_FALSE(AreConditionsMet());

  SimulatePlaybackStarted(1);
  SimulateResizeEvent(1, MediaEngagementContentsObserver::kSignificantSize);
  EXPECT_TRUE(AreConditionsMet());
}

TEST_F(MediaEngagementContentsObserverTest, EnsureCleanupAfterNavigation) {
  EXPECT_FALSE(GetStoredPlayerStatesCount());

  SimulateMutedStateChange(0, true);
  EXPECT_TRUE(GetStoredPlayerStatesCount());

  Navigate(GURL("https://example.com"));
  EXPECT_FALSE(GetStoredPlayerStatesCount());
}

TEST_F(MediaEngagementContentsObserverTest, TimerRunsDependingOnConditions) {
  EXPECT_FALSE(IsTimerRunning());

  SimulatePlaybackStarted(0);
  SimulateIsVisible();
  web_contents()->SetAudioMuted(false);
  SimulateResizeEvent(0, MediaEngagementContentsObserver::kSignificantSize);
  EXPECT_TRUE(IsTimerRunning());

  SimulateResizeEvent(0, 1);
  EXPECT_FALSE(IsTimerRunning());
  SimulateResizeEvent(0, MediaEngagementContentsObserver::kSignificantSize);

  web_contents()->SetAudioMuted(true);
  EXPECT_FALSE(IsTimerRunning());

  web_contents()->SetAudioMuted(false);
  SimulateIsHidden();
  EXPECT_FALSE(IsTimerRunning());

  SimulateIsVisible();
  SimulatePlaybackStopped(0);
  EXPECT_FALSE(IsTimerRunning());

  SimulatePlaybackStarted(0);
  EXPECT_TRUE(IsTimerRunning());

  SimulateMutedStateChange(0, true);
  EXPECT_FALSE(IsTimerRunning());

  SimulatePlaybackStarted(1);
  SimulateResizeEvent(1, MediaEngagementContentsObserver::kSignificantSize);
  EXPECT_TRUE(IsTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest, TimerDoesNotRunIfEntryRecorded) {
  SimulateSignificantPlaybackRecorded();

  SimulatePlaybackStarted(0);
  SimulateIsVisible();
  web_contents()->SetAudioMuted(false);
  SimulateResizeEvent(0, MediaEngagementContentsObserver::kSignificantSize);

  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest,
       SignificantPlaybackRecordedWhenTimerFires) {
  SimulateSignificantPlayer(0);
  EXPECT_TRUE(IsTimerRunning());
  EXPECT_FALSE(WasSignificantPlaybackRecorded());

  SimulatePlaybackTimerFired();
  EXPECT_TRUE(WasSignificantPlaybackRecorded());
}

TEST_F(MediaEngagementContentsObserverTest, InteractionsRecorded) {
  GURL url("https://www.example.com");
  ExpectScores(url, 0.0, 0, 0);

  Navigate(url);
  ExpectScores(url, 0.0, 1, 0);

  SimulateAudible();
  SimulateSignificantPlaybackTime();
  ExpectScores(url, 0.0, 1, 1);
}

TEST_F(MediaEngagementContentsObserverTest,
       SignificantPlaybackNotRecordedIfAudioSilent) {
  SimulatePlaybackStarted(0);
  SimulateIsVisible();
  SimulateInaudible();
  web_contents()->SetAudioMuted(false);
  EXPECT_FALSE(IsTimerRunning());
  EXPECT_FALSE(WasSignificantPlaybackRecorded());
}

TEST_F(MediaEngagementContentsObserverTest, DoNotRecordAudiolessTrack) {
  EXPECT_EQ(0u, GetSignificantActivePlayersCount());

  content::WebContentsObserver::MediaPlayerInfo player_info(true, false);
  SimulatePlaybackStarted(player_info, 0);
  EXPECT_EQ(0u, GetSignificantActivePlayersCount());
}

TEST_F(MediaEngagementContentsObserverTest,
       ResetStateOnNavigationWithPlayingPlayers) {
  Navigate(GURL("https://www.google.com"));
  SimulateSignificantPlayer(0);
  ForceUpdateTimer();
  EXPECT_TRUE(IsTimerRunning());

  Navigate(GURL("https://www.example.com"));
  EXPECT_FALSE(GetSignificantActivePlayersCount());
  EXPECT_FALSE(GetStoredPlayerStatesCount());
  EXPECT_FALSE(IsTimerRunning());
}
