// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"

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

 private:
  // Only MediaEngagementService can create a MediaEngagementContentsObserver.
  friend MediaEngagementService;
  friend MediaEngagementContentsObserverTest;

  static constexpr base::TimeDelta kSignificantMediaPlaybackTime =
      base::TimeDelta::FromSeconds(7);

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

  bool is_visible_ = false;
  bool significant_playback_recorded_ = false;

  url::Origin committed_origin_;

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementContentsObserver);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_CONTENTS_OBSERVER_H_
