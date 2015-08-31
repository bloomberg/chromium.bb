// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_SETTINGS_HELPER_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_SETTINGS_HELPER_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "ui/base/page_transition_types.h"

namespace content {
class WebContents;
}  // namespace content

class GURL;
class Profile;

// Utility class to record banner events for the given package or start url.
//
// These events are used to decide when banners should be shown, using a
// heuristic based on how many different days in a recent period of time (for
// example the past two weeks) the banner could have been shown, when it was
// last shown, when it was last blocked, and when it was last installed (for
// ServiceWorker style apps - native apps can query whether the app was
// installed directly).
//
// The desired effect is to have banners appear once a user has demonstrated
// an ongoing relationship with the app, and not to pester the user too much.
//
// For most events only the last event is recorded. The exception are the
// could show events. For these a list of the events is maintained. At most
// one event is stored per day, and events outside the window the heuristic
// uses are discarded. Local times are used to enforce these rules, to ensure
// what we count as a day matches what the user perceives to be days.
class AppBannerSettingsHelper {
 public:
  enum AppBannerEvent {
    APP_BANNER_EVENT_COULD_SHOW,
    APP_BANNER_EVENT_DID_SHOW,
    APP_BANNER_EVENT_DID_BLOCK,
    APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
    APP_BANNER_EVENT_NUM_EVENTS,
  };

  enum AppBannerRapporMetric {
    WEB,
    NATIVE,
  };

  // BannerEvents record the time that a site was accessed, along with an
  // engagement weight representing the importance of the access.
  struct BannerEvent {
   base::Time time;
   double engagement;
  };

  // The content setting basically records a simplified subset of history.
  // For privacy reasons this needs to be cleared. The ClearHistoryForURLs
  // function removes any information from the banner content settings for the
  // given URls.
  static void ClearHistoryForURLs(Profile* profile,
                                  const std::set<GURL>& origin_urls);

  // Record a banner installation event, for either a WEB or NATIVE app.
  static void RecordBannerInstallEvent(
      content::WebContents* web_contents,
      const std::string& package_name_or_start_url,
      AppBannerRapporMetric rappor_metric);

  // Record a banner dismissal event, for either a WEB or NATIVE app.
  static void RecordBannerDismissEvent(
      content::WebContents* web_contents,
      const std::string& package_name_or_start_url,
      AppBannerRapporMetric rappor_metric);

  // Record a banner event. Should not be used for could show events, as they
  // require a transition type.
  static void RecordBannerEvent(content::WebContents* web_contents,
                                const GURL& origin_url,
                                const std::string& package_name_or_start_url,
                                AppBannerEvent event,
                                base::Time time);

  // Record a banner could show event, with a specified transition type.
  static void RecordBannerCouldShowEvent(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& package_name_or_start_url,
      base::Time time,
      ui::PageTransition transition_type);

  // Determine if the banner should be shown, given the recorded events for the
  // supplied app.
  static bool ShouldShowBanner(content::WebContents* web_contents,
                               const GURL& origin_url,
                               const std::string& package_name_or_start_url,
                               base::Time time);

  // Gets the could have been shown events that are stored for the given package
  // or start url. This is only exposed for testing.
  static std::vector<BannerEvent> GetCouldShowBannerEvents(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& package_name_or_start_url);

  // Get the recorded event for an event type that only records the last event.
  // Should not be used with APP_BANNER_EVENT_COULD_SHOW. This is only exposed
  // for testing.
  static base::Time GetSingleBannerEvent(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& package_name_or_start_url,
      AppBannerEvent event);

  // Set the engagement weights assigned to direct and indirect navigations.
  static void SetEngagementWeights(double direct_engagement,
                                   double indirect_engagement);

  // Set the minimum number of minutes between banner visits that will
  // trigger a could show banner event. This must be less than the
  // number of minutes in a day, and evenly divide the number of minutes
  // in a day.
  static void SetMinimumMinutesBetweenVisits(unsigned int minutes);

  // Set the total engagement weight required to trigger a banner.
  static void SetTotalEngagementToTrigger(double total_engagement);

  // Bucket a given time to the given resolution in local time.
  static base::Time BucketTimeToResolution(base::Time time,
                                           unsigned int minutes);

  // Updates all values from field trial.
  static void UpdateFromFieldTrial();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AppBannerSettingsHelper);
};

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_SETTINGS_HELPER_H_
