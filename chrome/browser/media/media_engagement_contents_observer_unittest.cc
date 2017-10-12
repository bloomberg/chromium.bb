// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_contents_observer.h"

#include "base/optional.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_engagement_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

class MediaEngagementContentsObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitFromCommandLine("RecordMediaEngagementScores",
                                             std::string());

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

  void SimulatePlaybackStarted(int id, bool has_audio, bool has_video) {
    content::WebContentsObserver::MediaPlayerInfo player_info(has_video,
                                                              has_audio);
    SimulatePlaybackStarted(player_info, id, false);
  }

  void SimulateResizeEvent(int id, gfx::Size size) {
    content::WebContentsObserver::MediaPlayerId player_id =
        std::make_pair(nullptr /* RenderFrameHost */, id);
    contents_observer_->MediaResized(size, player_id);
  }

  void SimulateAudioVideoPlaybackStarted(int id) {
    SimulatePlaybackStarted(id, true, true);
  }

  void SimulateResizeEventSignificantSize(int id) {
    SimulateResizeEvent(id, MediaEngagementContentsObserver::kSignificantSize);
  }

  void SimulatePlaybackStarted(
      content::WebContentsObserver::MediaPlayerInfo player_info,
      int id,
      bool muted_state) {
    content::WebContentsObserver::MediaPlayerId player_id =
        std::make_pair(nullptr /* RenderFrameHost */, id);
    contents_observer_->MediaStartedPlaying(player_info, player_id);
    SimulateMutedStateChange(id, muted_state);
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

    MediaEngagementScore score =
        contents_observer_->service_->CreateEngagementScore(url);
    EXPECT_EQ(expected_visits, score.visits());
    EXPECT_EQ(expected_media_playbacks, score.media_playbacks());
  }

  void SetScores(GURL url, int visits, int media_playbacks) {
    MediaEngagementScore score =
        contents_observer_->service_->CreateEngagementScore(url);
    score.SetVisits(visits);
    score.SetMediaPlaybacks(media_playbacks);
    score.Commit();
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

  void ExpectUkmEntry(GURL url,
                      int playbacks_total,
                      int visits_total,
                      int score,
                      int playbacks_delta,
                      bool high_score) {
    using Entry = ukm::builders::Media_Engagement_SessionFinished;

    std::vector<std::pair<const char*, int64_t>> metrics = {
        {Entry::kPlaybacks_TotalName, playbacks_total},
        {Entry::kVisits_TotalName, visits_total},
        {Entry::kEngagement_ScoreName, score},
        {Entry::kPlaybacks_DeltaName, playbacks_delta},
        {Entry::kEngagement_IsHighName, high_score},
    };

    const ukm::UkmSource* source =
        test_ukm_recorder_.GetSourceForUrl(url.spec().c_str());
    EXPECT_EQ(url, source->url());
    EXPECT_EQ(1, test_ukm_recorder_.CountEntries(*source, Entry::kEntryName));
    test_ukm_recorder_.ExpectMetric(*source, Entry::kEntryName,
                                    Entry::kVisits_TotalName, visits_total);
    test_ukm_recorder_.ExpectMetric(*source, Entry::kEntryName,
                                    Entry::kPlaybacks_TotalName,
                                    playbacks_total);
    test_ukm_recorder_.ExpectMetric(*source, Entry::kEntryName,
                                    Entry::kEngagement_ScoreName, score);
    test_ukm_recorder_.ExpectMetric(*source, Entry::kEntryName,
                                    Entry::kPlaybacks_DeltaName,
                                    playbacks_delta);
    test_ukm_recorder_.ExpectEntry(*source, Entry::kEntryName, metrics);
  }

  void ExpectNoUkmEntry() { EXPECT_FALSE(test_ukm_recorder_.sources_count()); }

  void SimulateDestroy() { contents_observer_->WebContentsDestroyed(); }

  void SimulateSignificantAudioPlayer(int id) {
    SimulatePlaybackStarted(id, true, false);
    SimulateAudible();
    web_contents()->SetAudioMuted(false);
  }

  void SimulateSignificantVideoPlayer(int id) {
    SimulateAudioVideoPlaybackStarted(id);
    SimulateAudible();
    web_contents()->SetAudioMuted(false);
    SimulateResizeEventSignificantSize(id);
  }

  void ForceUpdateTimer() { contents_observer_->UpdateTimer(); }

  void ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason reason,
      int count) {
    histogram_tester_.ExpectBucketCount(
        MediaEngagementContentsObserver::
            kHistogramSignificantNotAddedFirstTimeName,
        static_cast<int>(reason), count);
  }

  void ExpectNotAddedAfterFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason reason,
      int count) {
    histogram_tester_.ExpectBucketCount(
        MediaEngagementContentsObserver::
            kHistogramSignificantNotAddedAfterFirstTimeName,
        static_cast<int>(reason), count);
  }

  void ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason reason,
      int count) {
    histogram_tester_.ExpectBucketCount(
        MediaEngagementContentsObserver::kHistogramSignificantRemovedName,
        static_cast<int>(reason), count);
  }

 private:
  // contents_observer_ auto-destroys when WebContents is destroyed.
  MediaEngagementContentsObserver* contents_observer_;

  base::test::ScopedFeatureList scoped_feature_list_;

  base::MockTimer* playback_timer_;

  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

  base::HistogramTester histogram_tester_;
};

// TODO(mlamouri): test that visits are not recorded multiple times when a
// same-origin navigation happens.

TEST_F(MediaEngagementContentsObserverTest, SignificantActivePlayerCount) {
  EXPECT_EQ(0u, GetSignificantActivePlayersCount());

  SimulateAudioVideoPlaybackStarted(0);
  SimulateResizeEventSignificantSize(0);
  EXPECT_EQ(1u, GetSignificantActivePlayersCount());

  SimulateAudioVideoPlaybackStarted(1);
  SimulateResizeEventSignificantSize(1);
  EXPECT_EQ(2u, GetSignificantActivePlayersCount());

  SimulateAudioVideoPlaybackStarted(2);
  SimulateResizeEventSignificantSize(2);
  EXPECT_EQ(3u, GetSignificantActivePlayersCount());

  SimulatePlaybackStopped(1);
  EXPECT_EQ(2u, GetSignificantActivePlayersCount());

  SimulatePlaybackStopped(0);
  EXPECT_EQ(1u, GetSignificantActivePlayersCount());

  SimulateResizeEvent(2, gfx::Size(1, 1));
  EXPECT_EQ(0u, GetSignificantActivePlayersCount());

  SimulateSignificantAudioPlayer(3);
  EXPECT_EQ(1u, GetSignificantActivePlayersCount());
}

TEST_F(MediaEngagementContentsObserverTest, AreConditionsMet) {
  EXPECT_FALSE(AreConditionsMet());

  SimulateSignificantVideoPlayer(0);
  EXPECT_TRUE(AreConditionsMet());

  SimulateResizeEvent(0, gfx::Size(1, 1));
  EXPECT_FALSE(AreConditionsMet());

  SimulateResizeEventSignificantSize(0);
  EXPECT_TRUE(AreConditionsMet());

  SimulateResizeEvent(
      0,
      gfx::Size(MediaEngagementContentsObserver::kSignificantSize.width(), 1));
  EXPECT_FALSE(AreConditionsMet());

  SimulateResizeEvent(
      0,
      gfx::Size(1, MediaEngagementContentsObserver::kSignificantSize.height()));
  EXPECT_FALSE(AreConditionsMet());
  SimulateResizeEventSignificantSize(0);

  web_contents()->SetAudioMuted(true);
  EXPECT_FALSE(AreConditionsMet());
  web_contents()->SetAudioMuted(false);

  SimulatePlaybackStopped(0);
  EXPECT_FALSE(AreConditionsMet());

  SimulateAudioVideoPlaybackStarted(0);
  EXPECT_TRUE(AreConditionsMet());

  SimulateMutedStateChange(0, true);
  EXPECT_FALSE(AreConditionsMet());

  SimulateSignificantVideoPlayer(1);
  EXPECT_TRUE(AreConditionsMet());
}

TEST_F(MediaEngagementContentsObserverTest, AreConditionsMet_AudioOnly) {
  EXPECT_FALSE(AreConditionsMet());

  SimulateSignificantAudioPlayer(0);
  EXPECT_TRUE(AreConditionsMet());
}

TEST_F(MediaEngagementContentsObserverTest, RecordInsignificantReason) {
  // Play the media.
  SimulateAudioVideoPlaybackStarted(0);
  SimulateResizeEvent(0, gfx::Size(1, 1));
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::
          kFrameSizeTooSmall,
      1);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 1);

  // Resize the frame to full size.
  SimulateResizeEventSignificantSize(0);

  // Resize the frame size.
  SimulateResizeEvent(0, gfx::Size(1, 1));
  SimulateResizeEventSignificantSize(0);
  ExpectRemovedBucketCount(MediaEngagementContentsObserver::
                               InsignificantPlaybackReason::kFrameSizeTooSmall,
                           1);
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 1);

  // Pause the player.
  ExpectRemovedBucketCount(MediaEngagementContentsObserver::
                               InsignificantPlaybackReason::kMediaPaused,
                           0);
  SimulatePlaybackStopped(0);
  ExpectRemovedBucketCount(MediaEngagementContentsObserver::
                               InsignificantPlaybackReason::kMediaPaused,
                           1);
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 2);
  SimulateAudioVideoPlaybackStarted(0);

  // Mute the player.
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kAudioMuted,
      0);
  SimulateMutedStateChange(0, true);
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kAudioMuted,
      1);
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 3);

  // Start a video only player.
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::
          kNoAudioTrack,
      0);
  SimulatePlaybackStarted(2, false, true);
  SimulateResizeEventSignificantSize(2);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::
          kNoAudioTrack,
      1);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 2);

  // Make sure we only record not added when we have the full state.
  SimulateAudioVideoPlaybackStarted(3);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 2);
  SimulateResizeEvent(3, gfx::Size(1, 1));
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 3);

  // Make sure we only record removed when we have the full state.
  SimulateAudioVideoPlaybackStarted(4);
  SimulateMutedStateChange(4, true);
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 3);
  SimulateResizeEventSignificantSize(4);
  SimulateMutedStateChange(4, false);
  SimulateMutedStateChange(4, true);
  ExpectRemovedBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 4);
}

TEST_F(MediaEngagementContentsObserverTest,
       RecordInsignificantReason_NotAdded_AfterFirstTime) {
  SimulatePlaybackStarted(0, false, true);
  SimulateMutedStateChange(0, true);
  SimulateResizeEventSignificantSize(0);

  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::
          kNoAudioTrack,
      1);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kAudioMuted,
      1);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 1);

  SimulateMutedStateChange(0, false);

  ExpectNotAddedAfterFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::
          kNoAudioTrack,
      1);
  ExpectNotAddedAfterFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 1);

  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::
          kNoAudioTrack,
      1);
  ExpectNotAddedFirstTimeBucketCount(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount, 1);
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

  SimulateSignificantVideoPlayer(0);
  EXPECT_TRUE(IsTimerRunning());

  SimulateResizeEvent(0, gfx::Size(1, 1));
  EXPECT_FALSE(IsTimerRunning());

  SimulateResizeEvent(
      0,
      gfx::Size(MediaEngagementContentsObserver::kSignificantSize.width(), 1));
  EXPECT_FALSE(IsTimerRunning());

  SimulateResizeEvent(
      0,
      gfx::Size(1, MediaEngagementContentsObserver::kSignificantSize.height()));
  EXPECT_FALSE(IsTimerRunning());
  SimulateResizeEventSignificantSize(0);

  web_contents()->SetAudioMuted(true);
  EXPECT_FALSE(IsTimerRunning());

  web_contents()->SetAudioMuted(false);
  EXPECT_TRUE(IsTimerRunning());

  SimulatePlaybackStopped(0);
  EXPECT_FALSE(IsTimerRunning());

  SimulateAudioVideoPlaybackStarted(0);
  EXPECT_TRUE(IsTimerRunning());

  SimulateMutedStateChange(0, true);
  EXPECT_FALSE(IsTimerRunning());

  SimulateSignificantVideoPlayer(1);
  EXPECT_TRUE(IsTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest,
       TimerRunsDependingOnConditions_AudioOnly) {
  EXPECT_FALSE(IsTimerRunning());

  SimulateSignificantAudioPlayer(0);
  EXPECT_TRUE(IsTimerRunning());
};

TEST_F(MediaEngagementContentsObserverTest, TimerDoesNotRunIfEntryRecorded) {
  SimulateSignificantPlaybackRecorded();
  SimulateSignificantVideoPlayer(0);
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest,
       SignificantPlaybackRecordedWhenTimerFires) {
  SimulateSignificantVideoPlayer(0);
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
  SimulateAudioVideoPlaybackStarted(0);
  SimulateInaudible();
  web_contents()->SetAudioMuted(false);
  EXPECT_FALSE(IsTimerRunning());
  EXPECT_FALSE(WasSignificantPlaybackRecorded());
}

TEST_F(MediaEngagementContentsObserverTest, DoNotRecordAudiolessTrack) {
  EXPECT_EQ(0u, GetSignificantActivePlayersCount());

  content::WebContentsObserver::MediaPlayerInfo player_info(true, false);
  SimulatePlaybackStarted(player_info, 0, false);
  EXPECT_EQ(0u, GetSignificantActivePlayersCount());
}

TEST_F(MediaEngagementContentsObserverTest,
       ResetStateOnNavigationWithPlayingPlayers) {
  Navigate(GURL("https://www.google.com"));
  SimulateSignificantVideoPlayer(0);
  ForceUpdateTimer();
  EXPECT_TRUE(IsTimerRunning());

  Navigate(GURL("https://www.example.com"));
  EXPECT_FALSE(GetSignificantActivePlayersCount());
  EXPECT_FALSE(GetStoredPlayerStatesCount());
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest, RecordScoreOnPlayback) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.google.co.uk");
  GURL url3("https://www.example.com");

  SetScores(url1, 5, 5);
  SetScores(url2, 5, 3);
  SetScores(url3, 1, 1);
  base::HistogramTester tester;
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 0);

  Navigate(url1);
  SimulateAudioVideoPlaybackStarted(0);
  tester.ExpectBucketCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 83, 1);

  Navigate(url2);
  SimulateAudioVideoPlaybackStarted(0);
  SimulateAudioVideoPlaybackStarted(1);
  SimulateMutedStateChange(0, false);
  tester.ExpectBucketCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 50, 2);

  Navigate(url3);
  SimulateAudioVideoPlaybackStarted(0);
  tester.ExpectBucketCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 0, 1);
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 4);

  SimulateMutedStateChange(1, false);
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 4);

  SimulateAudioVideoPlaybackStarted(1);
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 5);
}

TEST_F(MediaEngagementContentsObserverTest, DoNotRecordScoreOnPlayback_Muted) {
  GURL url("https://www.google.com");
  SetScores(url, 5, 5);

  base::HistogramTester tester;
  Navigate(url);
  content::WebContentsObserver::MediaPlayerInfo player_info(true, true);
  SimulatePlaybackStarted(player_info, 0, true);
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 0);

  SimulateMutedStateChange(0, false);
  tester.ExpectBucketCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 83, 1);
}

TEST_F(MediaEngagementContentsObserverTest,
       DoNotRecordScoreOnPlayback_NoAudioTrack) {
  GURL url("https://www.google.com");
  SetScores(url, 5, 5);

  base::HistogramTester tester;
  Navigate(url);
  content::WebContentsObserver::MediaPlayerInfo player_info(true, false);
  SimulatePlaybackStarted(player_info, 0, false);
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 0);
}

TEST_F(MediaEngagementContentsObserverTest,
       DoNotRecordScoreOnPlayback_InternalUrl) {
  GURL url("chrome://about");
  SetScores(url, 5, 5);

  base::HistogramTester tester;
  Navigate(url);
  SimulateAudioVideoPlaybackStarted(0);
  tester.ExpectTotalCount(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName, 0);
}

TEST_F(MediaEngagementContentsObserverTest, VisibilityNotRequired) {
  EXPECT_FALSE(IsTimerRunning());

  SimulateSignificantVideoPlayer(0);
  EXPECT_TRUE(IsTimerRunning());

  SimulateIsVisible();
  EXPECT_TRUE(IsTimerRunning());

  SimulateIsHidden();
  EXPECT_TRUE(IsTimerRunning());
}

TEST_F(MediaEngagementContentsObserverTest, RecordUkmMetricsOnDestroy) {
  GURL url("https://www.google.com");
  SetScores(url, 6, 5);
  Navigate(url);

  EXPECT_FALSE(WasSignificantPlaybackRecorded());
  SimulateSignificantVideoPlayer(0);
  SimulateSignificantPlaybackTime();
  ExpectScores(url, 6.0 / 7.0, 7, 6);
  EXPECT_TRUE(WasSignificantPlaybackRecorded());

  SimulateDestroy();
  ExpectUkmEntry(url, 6, 7, 86, 1, true);
}

TEST_F(MediaEngagementContentsObserverTest,
       RecordUkmMetricsOnDestroy_NoPlaybacks) {
  GURL url("https://www.google.com");
  SetScores(url, 6, 5);
  Navigate(url);

  EXPECT_FALSE(WasSignificantPlaybackRecorded());
  ExpectScores(url, 5.0 / 7.0, 7, 5);

  SimulateDestroy();
  ExpectUkmEntry(url, 5, 7, 71, 0, true);
}

TEST_F(MediaEngagementContentsObserverTest, RecordUkmMetricsOnNavigate) {
  GURL url("https://www.google.com");
  SetScores(url, 6, 5);
  Navigate(url);

  EXPECT_FALSE(WasSignificantPlaybackRecorded());
  SimulateSignificantVideoPlayer(0);
  SimulateSignificantPlaybackTime();
  ExpectScores(url, 6.0 / 7.0, 7, 6);
  EXPECT_TRUE(WasSignificantPlaybackRecorded());

  Navigate(GURL("https://www.example.org"));
  ExpectUkmEntry(url, 6, 7, 86, 1, true);
}

TEST_F(MediaEngagementContentsObserverTest,
       RecordUkmMetricsOnNavigate_NoPlaybacks) {
  GURL url("https://www.google.com");
  SetScores(url, 9, 2);
  Navigate(url);

  EXPECT_FALSE(WasSignificantPlaybackRecorded());
  ExpectScores(url, 2 / 10.0, 10, 2);

  Navigate(GURL("https://www.example.org"));
  ExpectUkmEntry(url, 2, 10, 20, 0, false);
}

TEST_F(MediaEngagementContentsObserverTest, DoNotRecordMetricsOnInternalUrl) {
  Navigate(GURL("chrome://about"));

  EXPECT_FALSE(WasSignificantPlaybackRecorded());
  SimulateSignificantVideoPlayer(0);
  SimulateSignificantPlaybackTime();
  EXPECT_TRUE(WasSignificantPlaybackRecorded());

  SimulateDestroy();
  ExpectNoUkmEntry();
}
