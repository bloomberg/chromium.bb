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
#include "base/observer_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "chrome/browser/engagement/site_engagement_observer.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/WebKit/public/platform/site_engagement.mojom.h"
#include "ui/base/page_transition_types.h"

namespace base {
class Clock;
}

namespace content {
class WebContents;
}

namespace history {
class HistoryService;
}

class GURL;
class HostContentSettingsMap;
class Profile;
class SiteEngagementScore;

#if defined(OS_ANDROID)
class SiteEngagementServiceAndroid;
#endif

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
// An engagement score is a non-negative double that represents how much a user
// has engaged with an origin - the higher it is, the more engagement the user
// has had with this site recently.
//
// User activity such as visiting the origin often, interacting with the origin,
// and adding it to the homescreen will increase the site engagement score. If
// a site's score does not increase for some time, it will decay, eventually
// reaching zero with further disuse.
//
// The SiteEngagementService object must be created and used on the UI thread
// only. Engagement scores may be queried in a read-only fashion from other
// threads using SiteEngagementService::GetScoreFromSettings, but use of this
// method is discouraged unless it is not possible to use the UI thread.
class SiteEngagementService : public KeyedService,
                              public history::HistoryServiceObserver,
                              public SiteEngagementScoreProvider {
 public:
  // WebContentsObserver that detects engagement triggering events and notifies
  // the service of them.
  class Helper;

  // The name of the site engagement variation field trial.
  static const char kEngagementParams[];

  // Returns the site engagement service attached to this profile. The service
  // exists in incognito mode; scores will be initialised using the score from
  // the profile that the incognito session was created from, and will increase
  // and decrease as usual. Engagement earned or decayed in incognito will not
  // be persisted or reflected in the original profile.
  //
  // This method must be called on the UI thread.
  static SiteEngagementService* Get(Profile* profile);

  // Returns the maximum possible amount of engagement that a site can accrue.
  static double GetMaxPoints();

  // Returns whether or not the site engagement service is enabled.
  static bool IsEnabled();

  // Returns the score for |origin| based on |settings|. Can be called on any
  // thread and does not cause any cleanup, decay, etc.
  //
  // Should only be used if you cannot create a SiteEngagementService (i.e. you
  // cannot run on the UI thread).
  static double GetScoreFromSettings(HostContentSettingsMap* settings,
                                     const GURL& origin);

  explicit SiteEngagementService(Profile* profile);
  ~SiteEngagementService() override;

  // Returns the engagement level of |url|.
  blink::mojom::EngagementLevel GetEngagementLevel(const GURL& url) const;

  // Returns a map of all stored origins and their engagement scores.
  std::map<GURL, double> GetScoreMap() const;

  // Returns whether the engagement service has enough data to make meaningful
  // decisions. Clients should avoid using engagement in their heuristic until
  // this is true.
  bool IsBootstrapped() const;

  // Returns whether |url| has at least the given |level| of engagement.
  bool IsEngagementAtLeast(const GURL& url,
                           blink::mojom::EngagementLevel level) const;

  // Resets the engagement score |url| to |score|, clearing daily limits.
  void ResetScoreForURL(const GURL& url, double score);

  // Update the last time |url| was opened from an installed shortcut to be
  // clock_->Now().
  void SetLastShortcutLaunchTime(const GURL& url);

  void HelperCreated(SiteEngagementService::Helper* helper);
  void HelperDeleted(SiteEngagementService::Helper* helper);

  // Overridden from SiteEngagementScoreProvider.
  double GetScore(const GURL& url) const override;
  double GetTotalEngagementPoints() const override;

 private:
  friend class SiteEngagementObserver;
  friend class SiteEngagementServiceAndroid;
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, CheckHistograms);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, CleanupEngagementScores);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest,
                           CleanupMovesScoreBackToNow);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest,
                           CleanupMovesScoreBackToRebase);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest,
                           CleanupEngagementScoresProportional);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, ClearHistoryForURLs);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, GetMedianEngagement);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, GetTotalNavigationPoints);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, GetTotalUserInputPoints);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, RestrictedToHTTPAndHTTPS);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, LastShortcutLaunch);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest,
                           CleanupOriginsOnHistoryDeletion);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, IsBootstrapped);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, EngagementLevel);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, Observers);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, ScoreDecayHistograms);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, LastEngagementTime);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest,
                           IncognitoEngagementService);
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, GetScoreFromSettings);
  FRIEND_TEST_ALL_PREFIXES(AppBannerSettingsHelperTest, SiteEngagementTrigger);

#if defined(OS_ANDROID)
  // Shim class to expose the service to Java.
  SiteEngagementServiceAndroid* GetAndroidService() const;
  void SetAndroidService(
      std::unique_ptr<SiteEngagementServiceAndroid> android_service);
#endif

  // Only used in tests.
  SiteEngagementService(Profile* profile, std::unique_ptr<base::Clock> clock);

  // Adds the specified number of points to the given origin, respecting the
  // maximum limits for the day and overall.
  void AddPoints(const GURL& url, double points);

  // Retrieves the SiteEngagementScore object for |origin|.
  SiteEngagementScore CreateEngagementScore(const GURL& origin) const;

  // Runs site engagement maintenance tasks.
  void AfterStartupTask();

  // Removes any origins which have decayed to 0 engagement. If
  // |update_last_engagement_time| is true, the last engagement time of all
  // origins is reset by calculating the delta between the last engagement event
  // recorded by the site engagement service and the origin. The origin's last
  // engagement time is then set to clock_->Now() - delta.
  //
  // If a user does not use the browser at all for some period of time,
  // engagement is not decayed, and the state is restored equivalent to how they
  // left it once they return.
  void CleanupEngagementScores(bool update_last_engagement_time) const;

  // Records UMA metrics.
  void RecordMetrics();

  // Returns true if we should record engagement for this URL. Currently,
  // engagement is only earned for HTTP and HTTPS.
  bool ShouldRecordEngagement(const GURL& url) const;

  // Get and set the last engagement time from prefs.
  base::Time GetLastEngagementTime() const;
  void SetLastEngagementTime(base::Time last_engagement_time) const;

  // Get the maximum decay period and the stale period for last engagement
  // times.
  base::TimeDelta GetMaxDecayPeriod() const;
  base::TimeDelta GetStalePeriod() const;

  // Returns the median engagement score of all recorded origins.
  double GetMedianEngagement(const std::map<GURL, double>& score_map) const;

  // Update the engagement score of the origin loaded in |web_contents| for
  // media playing. The points awarded are discounted if the media is being
  // played in a non-visible tab.
  void HandleMediaPlaying(content::WebContents* web_contents, bool is_hidden);

  // Update the engagement score of the origin loaded in |web_contents| for
  // navigation.
  void HandleNavigation(content::WebContents* web_contents,
                        ui::PageTransition transition);

  // Update the engagement score of the origin loaded in |web_contents| for
  // time-on-site, based on user input.
  void HandleUserInput(content::WebContents* web_contents,
                       SiteEngagementMetrics::EngagementType type);

  // Called if |url| changes to |level| engagement, and informs every Helper of
  // the change.
  void SendLevelChangeToHelpers(const GURL& url,
                                blink::mojom::EngagementLevel level);

  // Returns true if the last engagement increasing event seen by the site
  // engagement service was sufficiently long ago that we need to reset all
  // scores to be relative to now. This ensures that users who do not use the
  // browser for an extended period of time do not have their engagement decay.
  bool IsLastEngagementStale() const;

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

  // Add and remove observers of this service.
  void AddObserver(SiteEngagementObserver* observer);
  void RemoveObserver(SiteEngagementObserver* observer);

  Profile* profile_;

  // The clock used to vend times.
  std::unique_ptr<base::Clock> clock_;

#if defined(OS_ANDROID)
  std::unique_ptr<SiteEngagementServiceAndroid> android_service_;
#endif

  // Metrics are recorded at non-incognito browser startup, and then
  // approximately once per hour thereafter. Store the local time at which
  // metrics were previously uploaded: the first event which affects any
  // origin's engagement score after an hour has elapsed triggers the next
  // upload.
  base::Time last_metrics_time_;

  // All helpers currently attached to a WebContents.
  std::set<SiteEngagementService::Helper*> helpers_;

  // A list of observers. When any origin registers an engagement-increasing
  // event, each observer's OnEngagementIncreased method will be called.
  base::ObserverList<SiteEngagementObserver> observer_list_;

  base::WeakPtrFactory<SiteEngagementService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementService);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_H_
