// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SCORE_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SCORE_H_

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/gurl.h"

class HostContentSettingsMap;

// Calculates and stores the Media Engagement Index score for a certain origin.
class MediaEngagementScore final {
 public:
  // The dictionary keys to store individual metrics. kVisitsKey will
  // store the number of visits to an origin and kMediaPlaybacksKey
  // will store the number of media playbacks on an origin.
  // kLastMediaPlaybackTimeKey will store the timestamp of the last
  // media playback on an origin.
  static const char kVisitsKey[];
  static const char kMediaPlaybacksKey[];
  static const char kLastMediaPlaybackTimeKey[];

  // Origins with a number of visits less than this number will recieve
  // a score of zero.
  static const int kScoreMinVisits;

  MediaEngagementScore(base::Clock* clock,
                       const GURL& origin,
                       HostContentSettingsMap* settings);
  ~MediaEngagementScore();

  MediaEngagementScore(MediaEngagementScore&&);
  MediaEngagementScore& operator=(MediaEngagementScore&&);

  // Returns the total score, as per the formula.
  double GetTotalScore() const;

  // Writes the values in this score into |settings_map_|. If there are multiple
  // instances of a score object for an origin, this could result in stale data
  // being stored.
  void Commit();

  // Get/increment the number of visits this origin has had.
  int visits() const { return visits_; }
  void IncrementVisits() { visits_++; }

  // Get/increment the number of media playbacks this origin has had.
  int media_playbacks() const { return media_playbacks_; }
  void IncrementMediaPlaybacks();

  // Get the last time media was played on this origin.
  base::Time last_media_playback_time() const {
    return last_media_playback_time_;
  }

  void SetVisits(int visits) { visits_ = visits; }
  void SetMediaPlaybacks(int media_playbacks) {
    media_playbacks_ = media_playbacks;
  }

  // Get a breakdown of the score that can be serialized by Mojo.
  media::mojom::MediaEngagementScoreDetails GetScoreDetails() const;

 private:
  friend class MediaEngagementServiceTest;
  friend class MediaEngagementScoreTest;

  // Used for tests.
  MediaEngagementScore(base::Clock* clock,
                       const GURL& origin,
                       std::unique_ptr<base::DictionaryValue> score_dict);

  // Update the dictionary continaing the latest score values and return whether
  // they have changed or not (since what was last retrieved from content
  // settings).
  bool UpdateScoreDict();

  // The number of media playbacks this origin has had.
  int media_playbacks_ = 0;

  // The number of visits this origin has had.
  int visits_ = 0;

  // The last time media was played back on this origin.
  base::Time last_media_playback_time_;

  // The origin this score represents.
  GURL origin_;

  // A clock that can be used for testing, owned by the service.
  base::Clock* clock_;

  // The dictionary that represents this engagement score.
  std::unique_ptr<base::DictionaryValue> score_dict_;

  // The content settings map that will persist the score,
  // has a lifetime of the Profile like the service which owns |this|.
  HostContentSettingsMap* settings_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementScore);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SCORE_H_
