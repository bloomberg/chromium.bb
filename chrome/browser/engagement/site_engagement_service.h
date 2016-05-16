// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_H_

#include <map>
#include <memory>
#include <set>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/page_transition_types.h"

namespace base {
class DictionaryValue;
class Clock;
}

namespace history {
class HistoryService;
}

class GURL;
class Profile;

class SiteEngagementScoreProvider {
 public:
  // Returns a non-negative integer representing the engagement score of the
  // origin for this URL.
  virtual double GetScore(const GURL& url) const = 0;

  // Returns the sum of engagement points awarded to all sites.
  virtual double GetTotalEngagementPoints() const = 0;
};

// Stores and retrieves the engagement score of an origin.
//
// An engagement score is a positive integer that represents how much a user has
// engaged with an origin - the higher it is, the more engagement the user has
// had with this site recently.
//
// Positive user activity, such as visiting the origin often and adding it to
// the homescreen, will increase the site engagement score. Negative activity,
// such as rejecting permission prompts or not responding to notifications, will
// decrease the site engagement score.
class SiteEngagementService : public KeyedService,
                              public history::HistoryServiceObserver,
                              public SiteEngagementScoreProvider {
 public:
  // WebContentsObserver that detects engagement triggering events and notifies
  // the service of them.
  class Helper;

  enum EngagementLevel {
    ENGAGEMENT_LEVEL_NONE,
    ENGAGEMENT_LEVEL_LOW,
    ENGAGEMENT_LEVEL_MEDIUM,
    ENGAGEMENT_LEVEL_HIGH,
    ENGAGEMENT_LEVEL_MAX,
  };

  // The name of the site engagement variation field trial.
  static const char kEngagementParams[];

  // Returns the site engagement service attached to this profile. May return
  // null if the service does not exist (e.g. the user is in incognito).
  static SiteEngagementService* Get(Profile* profile);

  // Returns the maximum possible amount of engagement that a site can accrue.
  static double GetMaxPoints();

  // Returns whether or not the site engagement service is enabled.
  static bool IsEnabled();

  explicit SiteEngagementService(Profile* profile);
  ~SiteEngagementService() override;

  // Returns the engagement level of |url|. This is the recommended API for
  // clients
  EngagementLevel GetEngagementLevel(const GURL& url) const;

  // Returns a map of all stored origins and their engagement scores.
  std::map<GURL, double> GetScoreMap() const;

  // Returns whether the engagement service has enough data to make meaningful
  // decisions. Clients should avoid using engagement in their heuristic until
  // this is true.
  bool IsBootstrapped();

  // Returns whether |url| has at least the given |level| of engagement.
  bool IsEngagementAtLeast(const GURL& url, EngagementLevel level) const;

  // Resets the engagement score |url| to |score|, clearing daily limits.
  void ResetScoreForURL(const GURL& url, double score);

  // Update the last time |url| was opened from an installed shortcut to be
  // clock_->Now().
  void SetLastShortcutLaunchTime(const GURL& url);

  // Overridden from SiteEngagementScoreProvider.
  double GetScore(const GURL& url) const override;
  double GetTotalEngagementPoints() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, CheckHistograms);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, CleanupEngagementScores);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, ClearHistoryForURLs);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, GetMedianEngagement);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, GetTotalNavigationPoints);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, GetTotalUserInputPoints);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, LastShortcutLaunch);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest,
                           CleanupOriginsOnHistoryDeletion);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, IsBootstrapped);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, EngagementLevel);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, ScoreDecayHistograms);
  FRIEND_TEST_ALL_PREFIXES(AppBannerSettingsHelperTest, SiteEngagementTrigger);

  // Only used in tests.
  SiteEngagementService(Profile* profile, std::unique_ptr<base::Clock> clock);

  // Adds the specified number of points to the given origin, respecting the
  // maximum limits for the day and overall.
  void AddPoints(const GURL& url, double points);

  // Post startup tasks: cleaning up origins which have decayed to 0, and
  // logging UMA statistics.
  void AfterStartupTask();
  void CleanupEngagementScores();
  void RecordMetrics();

  // Returns the median engagement score of all recorded origins.
  double GetMedianEngagement(const std::map<GURL, double>& score_map) const;

  // Update the engagement score of the origin matching |url| for media playing.
  // The points awarded are discounted if the media is being played in a non-
  // visible tab.
  void HandleMediaPlaying(const GURL& url, bool is_hidden);

  // Update the engagement score of the origin matching |url| for navigation.
  void HandleNavigation(const GURL& url, ui::PageTransition transition);

  // Update the engagement score of the origin matching |url| for time-on-site,
  // based on user input.
  void HandleUserInput(const GURL& url,
                       SiteEngagementMetrics::EngagementType type);

  // Overridden from history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Returns the number of origins with maximum daily and total engagement
  // respectively.
  int OriginsWithMaxDailyEngagement() const;
  int OriginsWithMaxEngagement(const std::map<GURL, double>& score_map) const;

  // Callback for the history service when it is asked for a map of origins to
  // how many URLs corresponding to that origin remain in history.
  void GetCountsAndLastVisitForOriginsComplete(
    history::HistoryService* history_service,
    const std::multiset<GURL>& deleted_url_origins,
    bool expired,
    const history::OriginCountAndLastVisitMap& remaining_origin_counts);

  // Resets the engagement score for |url| to |score|, and sets the last
  // engagement time and last shortcut launch time (if it is non-null) to
  // |updated_time|. Clears daily limits.
  void ResetScoreAndAccessTimesForURL(const GURL& url,
                                      double score,
                                      const base::Time* updated_time);

  Profile* profile_;

  // The clock used to vend times.
  std::unique_ptr<base::Clock> clock_;

  // Metrics are recorded at non-incognito browser startup, and then
  // approximately once per hour thereafter. Store the local time at which
  // metrics were previously uploaded: the first event which affects any
  // origin's engagement score after an hour has elapsed triggers the next
  // upload.
  base::Time last_metrics_time_;

  base::WeakPtrFactory<SiteEngagementService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementService);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_H_
