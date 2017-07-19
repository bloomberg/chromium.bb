// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"

namespace gfx {
class Size;
}  // namespace gfx

class MediaEngagementContentsObserverTest;
class MediaEngagementService;

class MediaEngagementContentsObserver : public content::WebContentsObserver {
 public:
  ~MediaEngagementContentsObserver() override;

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WasShown() override;
  void WasHidden() override;
  void MediaStartedPlaying(const MediaPlayerInfo& media_player_info,
                           const MediaPlayerId& media_player_id) override;
  void MediaStoppedPlaying(const MediaPlayerInfo& media_player_info,
                           const MediaPlayerId& media_player_id) override;
  void DidUpdateAudioMutingState(bool muted) override;
  void MediaMutedStatusChanged(const MediaPlayerId& id, bool muted) override;
  void MediaResized(const gfx::Size& size, const MediaPlayerId& id) override;

  static const gfx::Size kSignificantSize;
  static const char* kHistogramScoreAtPlaybackName;

 private:
  // Only MediaEngagementService can create a MediaEngagementContentsObserver.
  friend MediaEngagementService;
  friend MediaEngagementContentsObserverTest;

  MediaEngagementContentsObserver(content::WebContents* web_contents,
                                  MediaEngagementService* service);

  void OnSignificantMediaPlaybackTime();
  bool AreConditionsMet() const;
  void UpdateTimer();

  void SetTimerForTest(std::unique_ptr<base::Timer> timer);

  // |this| is owned by |service_|.
  MediaEngagementService* service_;

  // Timer that will fire when the playback time reaches the minimum for
  // significant media playback.
  std::unique_ptr<base::Timer> playback_timer_;

  // Set of active players that can produce a significant playback. In other
  // words, whether this set is empty can be used to know if there is a
  // significant playback.
  std::set<MediaPlayerId> significant_players_;

  // A structure containing all the information we have about a player's state.
  struct PlayerState {
    bool muted = true;
    bool playing = false;           // Currently playing.
    bool significant_size = false;  // The video track has at least a certain
                                    // frame size.
    bool has_audio = false;         // The media has an audio track.
    bool score_recorded = false;    // The player has logged the engagement
                                    // score on playback.
  };
  std::map<MediaPlayerId, PlayerState*> player_states_;
  PlayerState* GetPlayerState(const MediaPlayerId& id);
  void ClearPlayerStates();

  // Inserts/removes players from significant_players_ based on their volume,
  // play state and size.
  void MaybeInsertSignificantPlayer(const MediaPlayerId& id);
  void MaybeRemoveSignificantPlayer(const MediaPlayerId& id);
  bool IsSignificantPlayer(const MediaPlayerId& id);

  // Record the score and change in score to UKM.
  void RecordUkmMetrics();

  static const char* kUkmEntryName;
  static const char* kUkmMetricPlaybacksTotalName;
  static const char* kUkmMetricVisitsTotalName;
  static const char* kUkmMetricEngagementScoreName;
  static const char* kUkmMetricPlaybacksDeltaName;

  bool is_visible_ = false;
  bool significant_playback_recorded_ = false;

  // Records the engagement score for the current origin to a histogram so we
  // can identify whether the playback would have been blocked.
  void RecordEngagementScoreToHistogramAtPlayback(const MediaPlayerId& id);

  url::Origin committed_origin_;

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementContentsObserver);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_CONTENTS_OBSERVER_H_
