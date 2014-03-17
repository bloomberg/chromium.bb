// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NTP_USER_DATA_LOGGER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NTP_USER_DATA_LOGGER_H_

#include <string>

#include "base/strings/string16.h"
#include "chrome/common/ntp_logging_events.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// Helper class for logging data from the NTP. Attached to each NTP instance.
class NTPUserDataLogger
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NTPUserDataLogger> {
 public:
  virtual ~NTPUserDataLogger();

  static NTPUserDataLogger* GetOrCreateFromWebContents(
      content::WebContents* content);

  // Returns the name of the histogram that should be logged for an impression
  // of a specified Most Visited |provider|.
  static std::string GetMostVisitedImpressionHistogramNameForProvider(
      const std::string& provider);

  // Returns the name of the histogram that should be logged for a navigation
  // to a specified Most Visited |provider|.
  static std::string GetMostVisitedNavigationHistogramNameForProvider(
      const std::string& provider);

  // Logs a number of statistics regarding the NTP. Called when an NTP tab is
  // about to be deactivated (be it by switching tabs, losing focus or closing
  // the tab/shutting down Chrome), or when the user navigates to a URL.
  void EmitNtpStatistics();

  // Called each time an event occurs on the NTP that requires a counter to be
  // incremented.
  void LogEvent(NTPLoggingEventType event);

  // Logs an impression on one of the Most Visited tiles by a given provider.
  void LogMostVisitedImpression(int position, const base::string16& provider);

  // Logs a navigation on one of the Most Visited tiles by a given provider.
  void LogMostVisitedNavigation(int position, const base::string16& provider);

  // content::WebContentsObserver override
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;

 protected:
  explicit NTPUserDataLogger(content::WebContents* contents);

 private:
  friend class content::WebContentsUserData<NTPUserDataLogger>;

  // True if at least one iframe came from a server-side suggestion. In
  // practice, either all the iframes are server-side suggestions or none are.
  bool has_server_side_suggestions_;

  // Total number of tiles rendered, no matter if it's a thumbnail, a gray tile
  // or an external tile.
  size_t number_of_tiles_;

  // Total number of tiles using a local thumbnail image for this NTP session.
  size_t number_of_thumbnail_tiles_;

  // Total number of tiles for which no thumbnail is specified and a gray tile
  // with the domain is used as the main tile.
  size_t number_of_gray_tiles_;

  // Total number of tiles for which the visual appearance is handled externally
  // by the page itself.
  size_t number_of_external_tiles_;

  // Total number of errors that occurred when trying to load thumbnail images
  // for this NTP session. When these errors occur a grey tile is shown instead
  // of a thumbnail image.
  size_t number_of_thumbnail_errors_;

  // The number of times a gray tile with the domain was used as the fallback
  // for a failed thumbnail.
  size_t number_of_gray_tile_fallbacks_;

  // The number of times an external tile, for which the visual appearance is
  // handled by the page itself, was the fallback for a failed thumbnail.
  size_t number_of_external_tile_fallbacks_;

  // Total number of mouseovers for this NTP session.
  size_t number_of_mouseovers_;

  // The URL of this New Tab Page - varies based on NTP version.
  GURL ntp_url_;

  DISALLOW_COPY_AND_ASSIGN(NTPUserDataLogger);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NTP_USER_DATA_LOGGER_H_
