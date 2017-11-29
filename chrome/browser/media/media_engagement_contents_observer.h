// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace base {
class Clock;
}  // namespace base

namespace gfx {
class Size;
}  // namespace gfx

namespace ukm {
class UkmRecorder;
}  // namespace ukm

class MediaEngagementContentsObserverTest;
class MediaEngagementService;

class MediaEngagementContentsObserver : public content::WebContentsObserver {
 public:
  ~MediaEngagementContentsObserver() override;

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void MediaStartedPlaying(const MediaPlayerInfo& media_player_info,
                           const MediaPlayerId& media_player_id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& media_player_info,
      const MediaPlayerId& media_player_id,
      WebContentsObserver::MediaStoppedReason reason) override;
  void DidUpdateAudioMutingState(bool muted) override;
  void MediaMutedStatusChanged(const MediaPlayerId& id, bool muted) override;
  void MediaResized(const gfx::Size& size, const MediaPlayerId& id) override;

  static const gfx::Size kSignificantSize;
  static const char* const kHistogramScoreAtPlaybackName;

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaEngagementContentsObserverTest,
                           RecordInsignificantReason);
  FRIEND_TEST_ALL_PREFIXES(MediaEngagementContentsObserverTest,
                           RecordInsignificantReason_NotAdded_AfterFirstTime);
  // Only MediaEngagementService can create a MediaEngagementContentsObserver.
  friend MediaEngagementService;
  friend MediaEngagementContentsObserverTest;
  friend class MediaEngagementBrowserTest;

  MediaEngagementContentsObserver(content::WebContents* web_contents,
                                  MediaEngagementService* service);

  // This is the maximum playback time for media to be considered 'short'.
  static const base::TimeDelta kMaxShortPlaybackTime;

  // This enum is used to record a histogram and should not be renumbered.
  enum class InsignificantPlaybackReason {
    // The frame size of the video is too small.
    kFrameSizeTooSmall = 0,

    // The player was muted.
    kAudioMuted,

    // The media is paused.
    kMediaPaused,

    // No audio track was present on the media.
    kNoAudioTrack,

    // This is always recorded to the histogram so we can see the number
    // of events that occured.
    kCount,

    // Add new items before this one, always keep this one at the end.
    kReasonMax,
  };

  enum class InsignificantHistogram {
    // The player isn't currently significant and can't be because it
    // doesn't meet all the criteria (first time only).
    kPlayerNotAddedFirstTime = 0,

    // The player isn't currently significant and can't be because it
    // doesn't meet all the critera (after the first time).
    kPlayerNotAddedAfterFirstTime,

    // The player was significant but no longer meets the criteria.
    kPlayerRemoved,
  };

  void OnSignificantMediaPlaybackTimeForPlayer(const MediaPlayerId& id);
  void OnSignificantMediaPlaybackTimeForPage();
  void UpdatePlayerTimer(const MediaPlayerId&);
  void UpdatePageTimer();
  bool AreConditionsMet() const;

  void SetTaskRunnerForTest(scoped_refptr<base::SequencedTaskRunner>);

  // |this| is owned by |service_|.
  MediaEngagementService* service_;

  // Timer that will fire when the playback time reaches the minimum for
  // significant media playback.
  std::unique_ptr<base::Timer> playback_timer_;

  // Set of active players that can produce a significant playback. In other
  // words, whether this set is empty can be used to know if there is a
  // significant playback.
  std::set<MediaPlayerId> significant_players_;

  // Measures playback time for a player.
  class PlaybackTimer {
   public:
    explicit PlaybackTimer(base::Clock*);

    void Start();
    void Stop();
    bool IsRunning() const;
    base::TimeDelta Elapsed() const;
    void Reset();

    DISALLOW_COPY_AND_ASSIGN(PlaybackTimer);

   private:
    // The clock is owned by |service_| which already owns |this|.
    base::Clock* clock_;

    base::Optional<base::Time> start_time_;
    base::TimeDelta recorded_time_;
  };

  // A structure containing all the information we have about a player's state.
  struct PlayerState {
    explicit PlayerState(base::Clock*);
    ~PlayerState();
    PlayerState(PlayerState&&);

    base::Optional<bool> muted;
    base::Optional<bool> playing;           // Currently playing.
    base::Optional<bool> significant_size;  // The video track has at least
                                            // a certain frame size.
    base::Optional<bool> has_audio;         // The media has an audio track.
    base::Optional<bool> has_video;         // The media has a video track.

    // The engagement score of the origin at playback has been recorded
    // to a histogram.
    bool score_recorded = false;

    // The reasons why the player was not significant have been recorded
    // to a histogram.
    bool reasons_recorded = false;

    bool reached_end_of_stream = false;
    std::unique_ptr<PlaybackTimer> playback_timer;

    DISALLOW_COPY_AND_ASSIGN(PlayerState);
  };
  std::map<MediaPlayerId, PlayerState> player_states_;
  PlayerState& GetPlayerState(const MediaPlayerId& id);
  void ClearPlayerStates();

  // Inserts/removes players from significant_players_ based on whether
  // they are considered significant by GetInsignificantPlayerReason.
  void MaybeInsertRemoveSignificantPlayer(const MediaPlayerId& id);

  // Returns a vector containing InsignificantPlaybackReason's why a player
  // would not be considered significant.
  std::vector<MediaEngagementContentsObserver::InsignificantPlaybackReason>
  GetInsignificantPlayerReasons(const PlayerState& state);

  // Records why a player is not significant to a historgram.
  void RecordInsignificantReasons(
      std::vector<InsignificantPlaybackReason> reasons,
      const PlayerState& state,
      InsignificantHistogram histogram);

  // Returns whether we have recieved all the state information about a
  // player in order to be able to make a decision about it.
  bool IsPlayerStateComplete(const PlayerState& state);

  // Returns whether the player is considered to be significant and record
  // any reasons why not to a histogram.
  bool IsSignificantPlayerAndRecord(
      const MediaPlayerId& id,
      MediaEngagementContentsObserver::InsignificantHistogram histogram);

  // Commits any pending data to website settings. CommitTrigger is the event
  // that is triggering the commit.
  enum CommitTrigger {
    // A significant media playback has occured.
    kSignificantMediaPlayback,

    // A visit has ended either by navigating off the origin or when the
    // observer is destroyed.
    kVisitEnd,
  };
  void MaybeCommitPendingData(CommitTrigger);

  static const char* const kHistogramSignificantNotAddedAfterFirstTimeName;
  static const char* const kHistogramSignificantNotAddedFirstTimeName;
  static const char* const kHistogramSignificantRemovedName;
  static const int kMaxInsignificantPlaybackReason;

  // Record the score and change in score to UKM.
  void RecordUkmMetrics(int audible_players_count,
                        int significant_players_count);

  // Record the length of an ignored media playback.
  void RecordUkmIgnoredEvent(int length_msec);
  ukm::UkmRecorder* GetUkmRecorder();

  bool significant_playback_recorded_ = false;

  // Records the engagement score for the current origin to a histogram so we
  // can identify whether the playback would have been blocked.
  void RecordEngagementScoreToHistogramAtPlayback(const MediaPlayerId& id);

  void StashAudiblePlayers();

  // Stores pending media engagement data that needs to be committed either
  // after a navigation to another domain, when the observer is destroyed or
  // when we have had a media playback.
  struct PendingCommitState {
    bool visit = false;
    bool media_playback = false;
    int audible_players = 0;
    int significant_players = 0;
  };
  PendingCommitState& GetPendingCommitState();
  base::Optional<PendingCommitState> pending_data_to_commit_;

  // Stores the ids of the players that were audible. The boolean will be true
  // if the player was significant.
  using AudiblePlayerRow = std::pair<bool, std::unique_ptr<base::Timer>>;
  std::map<MediaPlayerId, AudiblePlayerRow> audible_players_;

  // The task runner to use when creating timers. It is used only for testing.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The current UKM source id for |committed_origin_|.
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  // The time between significant playbacks to be recorded to UKM.
  base::TimeDelta time_since_playback_for_ukm_;

  url::Origin committed_origin_;

  static const base::TimeDelta kSignificantMediaPlaybackTime;

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementContentsObserver);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_CONTENTS_OBSERVER_H_
