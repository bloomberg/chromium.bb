// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_H_

#include <map>
#include <set>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "components/history/core/browser/history_service_observer.h"
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

namespace history {
class HistoryService;
}

class MediaEngagementService : public KeyedService,
                               public history::HistoryServiceObserver {
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

  // Returns an array of engagement score details for all origins which
  // have a score.
  std::vector<media::mojom::MediaEngagementScoreDetailsPtr> GetAllScoreDetails()
      const;

  // Overridden from history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // KeyedService support:
  void Shutdown() override;

  // Clear data if the last playback time is between these two time points.
  void ClearDataBetweenTime(const base::Time& delete_begin,
                            const base::Time& delete_end);

  // Retrieves the MediaEngagementScore for |url|.
  MediaEngagementScore CreateEngagementScore(const GURL& url) const;

  // The name of the histogram that scores are logged to on startup.
  static const char kHistogramScoreAtStartupName[];

 private:
  friend class MediaEngagementBrowserTest;
  friend class MediaEngagementServiceTest;
  friend class MediaEngagementContentsObserverTest;
  friend MediaEngagementContentsObserver;

  MediaEngagementService(Profile* profile, std::unique_ptr<base::Clock> clock);

  // Returns true if we should record engagement for this url. Currently,
  // engagement is only earned for HTTP and HTTPS.
  bool ShouldRecordEngagement(const GURL& url) const;

  std::set<MediaEngagementContentsObserver*> contents_observers_;

  Profile* profile_;

  // Clear any data for a specific origin.
  void Clear(const GURL& url);

  // An internal clock for testing.
  std::unique_ptr<base::Clock> clock_;

  // Records all the stored scores to a histogram.
  void RecordStoredScoresToHistogram();

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementService);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_H_
