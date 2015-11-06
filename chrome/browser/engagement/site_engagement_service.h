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
  // The maximum number of points that are allowed.
  static const double kMaxPoints;

  // The maximum number of points that can be accrued in one day.
  static double g_max_points_per_day;

  // The number of points given for navigations.
  static double g_navigation_points;

  // The number of points given for user input (indicating time-on-site).
  static double g_user_input_points;

  // The number of points given for media playing. Initially calibrated such
  // that at least 30 minutes of video watching or audio listening would be
  // required to allow a site to reach the daily engagement maximum.
  static double g_visible_media_playing_points;

  // The number of points given for media playing in a non-visible tab.
  static double g_hidden_media_playing_points;

  // Decaying works by removing a portion of the score periodically. This
  // constant determines how often that happens.
  static int g_decay_period_in_days;

  // How much the score decays after every kDecayPeriodInDays.
  static double g_decay_points;

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

  // Returns true if the maximum number of points today has been added.
  bool MaxPointsPerDayAdded();

  // Updates the content settings dictionary |score_dict| with the current score
  // fields. Returns true if |score_dict| changed, otherwise return false.
  bool UpdateScoreDict(base::DictionaryValue* score_dict);

 private:
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementScoreTest, PartiallyEmptyDictionary);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementScoreTest, PopulatedDictionary);
  friend class SiteEngagementScoreTest;

  // Keys used in the content settings dictionary.
  static const char* kRawScoreKey;
  static const char* kPointsAddedTodayKey;
  static const char* kLastEngagementTimeKey;

  // This version of the constructor is used in unit tests.
  explicit SiteEngagementScore(base::Clock* clock);

  // Determine the score, accounting for any decay.
  double DecayedScore() const;

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

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementScore);
};

class SiteEngagementScoreProvider {
 public:
  // Returns a non-negative integer representing the engagement score of the
  // origin for this URL.
  virtual double GetScore(const GURL& url) = 0;

  // Returns the sum of engagement points awarded to all sites.
  virtual double GetTotalEngagementPoints() = 0;
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
                              public SiteEngagementScoreProvider {
 public:
  // The name of the site engagement variation field trial.
  static const char kEngagementParams[];

  static SiteEngagementService* Get(Profile* profile);

  // Returns whether or not the SiteEngagementService is enabled.
  static bool IsEnabled();

  // Clears engagement scores for the given origins.
  static void ClearHistoryForURLs(Profile* profile,
                                  const std::set<GURL>& origins);

  explicit SiteEngagementService(Profile* profile);
  ~SiteEngagementService() override;

  // Returns a map of all stored origins and their engagement scores.
  std::map<GURL, double> GetScoreMap();

  // Update the karma score of the origin matching |url| for navigation.
  void HandleNavigation(const GURL& url, ui::PageTransition transition);

  // Update the karma score of the origin matching |url| for time-on-site, based
  // on user input.
  void HandleUserInput(const GURL& url,
                       SiteEngagementMetrics::EngagementType type);

  // Update the karma score of the origin matching |url| for media playing. The
  // points awarded are discounted if the media is being played in a non-visible
  // tab.
  void HandleMediaPlaying(const GURL& url, bool is_hidden);

  // Overridden from SiteEngagementScoreProvider:
  double GetScore(const GURL& url) override;
  double GetTotalEngagementPoints() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, CheckHistograms);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, CleanupEngagementScores);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, ClearHistoryForURLs);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, GetMedianEngagement);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, GetTotalNavigationPoints);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, GetTotalUserInputPoints);

  // Only used in tests.
  SiteEngagementService(Profile* profile, scoped_ptr<base::Clock> clock);

  // Adds the specified number of points to the given origin, respecting the
  // maximum limits for the day and overall.
  void AddPoints(const GURL& url, double points);

  // Post startup tasks: cleaning up origins which have decayed to 0, and
  // logging UMA statistics.
  void AfterStartupTask();
  void CleanupEngagementScores();
  void RecordMetrics();

  // Returns the median engagement score of all recorded origins.
  double GetMedianEngagement(std::map<GURL, double>& score_map);

  // Returns the number of origins with maximum daily and total engagement
  // respectively.
  int OriginsWithMaxDailyEngagement();
  int OriginsWithMaxEngagement(std::map<GURL, double>& score_map);

  Profile* profile_;

  // The clock used to vend times.
  scoped_ptr<base::Clock> clock_;

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
