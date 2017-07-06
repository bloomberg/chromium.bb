// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_H_

#include <map>
#include <set>

#include "base/macros.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
class MediaEngagementContentsObserver;
class MediaEngagementScore;
class Profile;

namespace base {
class Clock;
}

namespace content {
class WebContents;
}  // namespace content

class MediaEngagementService : public KeyedService {
 public:
  // Returns the instance attached to the given |profile|.
  static MediaEngagementService* Get(Profile* profile);

  // Returns whether the feature is enabled.
  static bool IsEnabled();

  // Observe the given |web_contents| by creating an internal
  // WebContentsObserver.
  static void CreateWebContentsObserver(content::WebContents* web_contents);

  explicit MediaEngagementService(Profile* profile);
  ~MediaEngagementService() override;

  // Returns the engagement score of |url|.
  double GetEngagementScore(const GURL& url) const;

  // Returns a map of all stored origins and their engagement levels.
  std::map<GURL, double> GetScoreMapForTesting() const;

  // Record a visit of a |url|.
  void RecordVisit(const GURL& url);

  // Record a media playback on a |url|.
  void RecordPlayback(const GURL& url);

 private:
  friend class MediaEngagementServiceTest;
  friend class MediaEngagementContentsObserverTest;
  friend MediaEngagementContentsObserver;

  MediaEngagementService(Profile* profile, std::unique_ptr<base::Clock> clock);

  // Returns true if we should record engagement for this url. Currently,
  // engagement is only earned for HTTP and HTTPS.
  bool ShouldRecordEngagement(const GURL& url) const;

  // Retrieves the MediaEngagementScore for |url|.
  MediaEngagementScore* CreateEngagementScore(const GURL& url) const;

  std::set<MediaEngagementContentsObserver*> contents_observers_;

  Profile* profile_;

  // An internal clock for testing.
  std::unique_ptr<base::Clock> clock_;

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementService);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_H_
