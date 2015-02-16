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

  // The content setting basically records a simplified subset of history.
  // For privacy reasons this needs to be cleared. The ClearHistoryForURLs
  // function removes any information from the banner content settings for the
  // given URls.
  static void ClearHistoryForURLs(Profile* profile,
                                  const std::set<GURL>& origin_urls);

  static void RecordBannerEvent(content::WebContents* web_contents,
                                const GURL& origin_url,
                                const std::string& package_name_or_start_url,
                                AppBannerEvent event,
                                base::Time time);

  // Determine if the banner should be shown, given the recorded events for the
  // supplied app.
  static bool ShouldShowBanner(content::WebContents* web_contents,
                               const GURL& origin_url,
                               const std::string& package_name_or_start_url,
                               base::Time time);

  // Gets the could have been shown events that are stored for the given package
  // or start url. This is only exposed for testing.
  static std::vector<base::Time> GetCouldShowBannerEvents(
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

  // Checks if a URL is allowed to show a banner for the given package or start
  // url.
  static bool IsAllowed(content::WebContents* web_contents,
                        const GURL& origin_url,
                        const std::string& package_name_or_start_url);

  // Blocks a URL from showing a banner for the given package or start url.
  static void Block(content::WebContents* web_contents,
                    const GURL& origin_url,
                    const std::string& package_name_or_start_url);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AppBannerSettingsHelper);
};

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_SETTINGS_HELPER_H_
