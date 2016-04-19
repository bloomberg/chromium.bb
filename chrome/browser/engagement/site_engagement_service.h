// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_H_

#include <map>
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

class GURL;
class Profile;

class SiteEngagementScore {
 public:
  // The parameters which can be varied via field trial. All "points" values
  // should be appended to the end of the enum prior to MAX_VARIATION.
  enum Variation {
    // The maximum number of points that can be accrued in one day.
    MAX_POINTS_PER_DAY = 0,

    // The period over which site engagement decays.
    DECAY_PERIOD_IN_DAYS,

    // The number of points to decay per period.
    DECAY_POINTS,

    // The number of points given for navigations.
    NAVIGATION_POINTS,

    // The number of points given for user input.
    USER_INPUT_POINTS,

    // The number of points given for media playing. Initially calibrated such
    // that at least 30 minutes of foreground media would be required to allow a
    // site to reach the daily engagement maximum.
    VISIBLE_MEDIA_POINTS,
    HIDDEN_MEDIA_POINTS,

    // The number of points added to engagement when a site is launched from
    // homescreen or added as a bookmark app. This bonus will apply for ten days
    // following a launch; each new launch resets the ten days.
    WEB_APP_INSTALLED_POINTS,

    // The number of points given for the first engagement event of the day for
    // each site.
    FIRST_DAILY_ENGAGEMENT,

    // The number of points that the engagement service must accumulate to be
    // considered 'useful'.
    BOOTSTRAP_POINTS,

    // The boundaries between low/medium and medium/high engagement as returned
    // by GetEngagementLevel().
    MEDIUM_ENGAGEMENT_BOUNDARY,
    HIGH_ENGAGEMENT_BOUNDARY,

    MAX_VARIATION
  };

  // The maximum number of points that are allowed.
  static const double kMaxPoints;

  static double GetMaxPointsPerDay();
  static double GetDecayPeriodInDays();
  static double GetDecayPoints();
  static double GetNavigationPoints();
  static double GetUserInputPoints();
  static double GetVisibleMediaPoints();
  static double GetHiddenMediaPoints();
  static double GetWebAppInstalledPoints();
  static double GetFirstDailyEngagementPoints();
  static double GetBootstrapPoints();
  static double GetMediumEngagementBoundary();
  static double GetHighEngagementBoundary();

  // Update the default engagement settings via variations.
  static void UpdateFromVariations();

  // The SiteEngagementService does not take ownership of |clock|. It is the
  // responsibility of the caller to make sure |clock| outlives this
  // SiteEngagementScore.
  SiteEngagementScore(base::Clock* clock,
                      const base::DictionaryValue& score_dict);
  ~SiteEngagementScore();

  double Score() const;
  void AddPoints(double points);

  // Resets the score to |points| and reset the daily point limit.
  void Reset(double points);

  // Returns true if the maximum number of points today has been added.
  bool MaxPointsPerDayAdded() const;

  // Get/set the last time this origin was launched from an installed shortcut.
  base::Time last_shortcut_launch_time() const {
    return last_shortcut_launch_time_;
  }
  void set_last_shortcut_launch_time(const base::Time& time) {
    last_shortcut_launch_time_ = time;
  }

  // Updates the content settings dictionary |score_dict| with the current score
  // fields. Returns true if |score_dict| changed, otherwise return false.
  bool UpdateScoreDict(base::DictionaryValue* score_dict);

 private:
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementScoreTest, PartiallyEmptyDictionary);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementScoreTest, PopulatedDictionary);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementScoreTest, Reset);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementScoreTest, FirstDailyEngagementBonus);
  friend class SiteEngagementHelperTest;
  friend class SiteEngagementScoreTest;
  friend class SiteEngagementServiceTest;

  // Array holding the values corresponding to each item in Variation array.
  static double param_values[];

  // Keys used in the content settings dictionary.
  static const char* kRawScoreKey;
  static const char* kPointsAddedTodayKey;
  static const char* kLastEngagementTimeKey;
  static const char* kLastShortcutLaunchTimeKey;

  // This version of the constructor is used in unit tests.
  explicit SiteEngagementScore(base::Clock* clock);

  // Determine the score, accounting for any decay.
  double DecayedScore() const;

  // Determine any score bonus from having installed shortcuts.
  double BonusScore() const;

  // Sets fixed parameter values for testing site engagement. Ensure that any
  // newly added parameters receive a fixed value here.
  static void SetParamValuesForTesting();

  // The clock used to vend times. Enables time travelling in tests. Owned by
  // the SiteEngagementService.
  base::Clock* clock_;

  // |raw_score_| is the score before any decay is applied.
  double raw_score_;

  // The points added 'today' are tracked to avoid adding more than
  // kMaxPointsPerDay on any one day. 'Today' is defined in local time.
  double points_added_today_;

  // The last time the score was updated for engagement. Used in conjunction
  // with |points_added_today_| to avoid adding more than kMaxPointsPerDay on
  // any one day.
  base::Time last_engagement_time_;

  // The last time the site with this score was launched from an installed
  // shortcut.
  base::Time last_shortcut_launch_time_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementScore);
};

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
  enum EngagementLevel {
    ENGAGEMENT_LEVEL_NONE,
    ENGAGEMENT_LEVEL_LOW,
    ENGAGEMENT_LEVEL_MEDIUM,
    ENGAGEMENT_LEVEL_HIGH,
    ENGAGEMENT_LEVEL_MAX,
  };

  // The name of the site engagement variation field trial.
  static const char kEngagementParams[];

  static SiteEngagementService* Get(Profile* profile);

  // Returns whether or not the SiteEngagementService is enabled.
  static bool IsEnabled();

  explicit SiteEngagementService(Profile* profile);
  ~SiteEngagementService() override;

  // Returns a map of all stored origins and their engagement scores.
  std::map<GURL, double> GetScoreMap() const;

  // Returns whether the engagement service has enough data to make meaningful
  // decisions. Clients should avoid using engagement in their heuristic until
  // this is true.
  bool IsBootstrapped();

  // Returns the engagement level of |url|. This is the recommended API for
  // clients
  EngagementLevel GetEngagementLevel(const GURL& url) const;

  // Returns whether |url| has at least the given |level| of engagement.
  bool IsEngagementAtLeast(const GURL& url, EngagementLevel level) const;

  // Update the engagement score of the origin matching |url| for navigation.
  void HandleNavigation(const GURL& url, ui::PageTransition transition);

  // Update the engagement score of the origin matching |url| for time-on-site,
  // based on user input.
  void HandleUserInput(const GURL& url,
                       SiteEngagementMetrics::EngagementType type);

  // Update the engagement score of the origin matching |url| for media playing.
  // The points awarded are discounted if the media is being played in a non-
  // visible tab.
  void HandleMediaPlaying(const GURL& url, bool is_hidden);

  // Resets the engagement score |url| to |score|, clearing daily limits.
  void ResetScoreForURL(const GURL& url, double score);

  // Overridden from history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Update the last time |url| was opened from an installed shortcut to be
  // clock_->Now().
  void SetLastShortcutLaunchTime(const GURL& url);

  // Overridden from SiteEngagementScoreProvider:
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

  // Returns the number of origins with maximum daily and total engagement
  // respectively.
  int OriginsWithMaxDailyEngagement() const;
  int OriginsWithMaxEngagement(const std::map<GURL, double>& score_map) const;

  void GetCountsForOriginsComplete(
    const std::multiset<GURL>& deleted_url_origins,
    bool expired,
    const history::OriginCountMap& remaining_origin_counts);

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
