// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_SETTINGS_HELPER_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_SETTINGS_HELPER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"

namespace content {
class WebContents;
}  // namespace content

class GURL;

// Utility class for reading and updating ContentSettings for app banners.
class AppBannerSettingsHelper {
 public:
  // TODO(benwells): Use this method to implement smarter triggering logic.
  // See http://crbug.com/452825.
  // Records that a banner could have been shown for the given package or start
  // url.
  //
  // These events are used to decide when banners should be shown, using a
  // heuristic based on how many different days in a recent period of time (for
  // example the past two weeks) the banner could have been shown. The desired
  // effect is to have banners appear once a user has demonstrated an ongoing
  // relationship with the app.
  //
  // At most one event is stored per day, and events outside the window the
  // heuristic uses are discarded. Local times are used to enforce these rules,
  // to ensure what we count as a day matches what the user perceives to be
  // days.
  static void RecordCouldShowBannerEvent(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& package_name_or_start_url,
      base::Time time);

  // Gets the could have been shown events that are stored for the given package
  // or start url. This is only used for testing.
  static std::vector<base::Time> GetCouldShowBannerEvents(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& package_name_or_start_url);

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
