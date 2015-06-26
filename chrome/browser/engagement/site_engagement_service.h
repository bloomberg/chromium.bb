// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class DictionaryValue;
}

class GURL;
class Profile;

class SiteEngagementScore {
 public:
  // Keys used in the content settings dictionary.
  static const char* kRawScoreKey;
  static const char* kPointsAddedTodayKey;
  static const char* kLastEngagementTimeKey;

  // The maximum number of points that are allowed.
  static const double kMaxPoints;

  // The maximum number of points that can be accrued in one day.
  static const double kMaxPointsPerDay;

  // The number of points given for a navigation.
  static const double kNavigationPoints;

  // Decaying works by removing a portion of the score periodically. This
  // constant determines how often that happens.
  static const int kDecayPeriodInDays;

  // How much the score decays after every kDecayPeriodInDays.
  static const double kDecayPoints;

  // The SiteEngagementService does not take ownership of |clock|. It is the
  // responsibility of the caller to make sure |clock| outlives this
  // SiteEngagementScore.
  SiteEngagementScore(base::Clock* clock,
                      const base::DictionaryValue& settings);
  ~SiteEngagementScore();

  double Score() const;
  void AddPoints(double points);

  // Updates the content settings dictionary |settings| with the current score
  // fields. Returns true if |settings| changed, otherwise return false.
  bool UpdateSettings(base::DictionaryValue* settings);

 private:
  friend class SiteEngagementScoreTest;

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
class SiteEngagementService : public KeyedService {
 public:
  static SiteEngagementService* Get(Profile* profile);

  // Returns whether or not the SiteEngagementService is enabled.
  static bool IsEnabled();

  explicit SiteEngagementService(Profile* profile);
  ~SiteEngagementService() override;

  // Update the karma score of the origin matching |url| for user navigation.
  void HandleNavigation(const GURL& url);

  // Returns a non-negative integer representing the engagement score of the
  // origin for this URL.
  int GetScore(const GURL& url);

 private:
  Profile* profile_;

  // The clock used to vend times.
  base::DefaultClock clock_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementService);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_H_
