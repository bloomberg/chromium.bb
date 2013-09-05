// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NTP_USER_DATA_LOGGER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NTP_USER_DATA_LOGGER_H_

#include "chrome/common/ntp_logging_events.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"


// Helper class for logging data from the NTP. Attached to each NTP instance.
class NTPUserDataLogger
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NTPUserDataLogger> {
 public:
  virtual ~NTPUserDataLogger();

  // To be set after initialization of this class. Used to determine whether a
  // tab visibility change event or navigation event comes from the NTP.
  void set_ntp_url(const GURL& url) {
    ntp_url_ = url;
  }

  const GURL& ntp_url() const { return ntp_url_; }

  // Logs the error percentage rate when loading thumbnail images for this NTP
  // session to UMA histogram. Called when the user navigates to a URL.
  void EmitThumbnailErrorRate();

  // Logs total number of mouseovers per NTP session to UMA histogram. Called
  // when an NTP tab is about to be deactivated (be it by switching tabs, losing
  // focus or closing the tab/shutting down Chrome), or when the user navigates
  // to a URL.
  void EmitMouseoverCount();

  // Called each time an event occurs on the NTP that requires a counter to be
  // incremented.
  void LogEvent(NTPLoggingEventType event);

  // content::WebContentsObserver override
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;

 protected:
  explicit NTPUserDataLogger(content::WebContents* contents);

  // Returns the percent error given |events| occurrences and |errors| errors.
  virtual size_t GetPercentError(size_t errors, size_t events) const;

 private:
  friend class content::WebContentsUserData<NTPUserDataLogger>;

  // Total number of mouseovers for this NTP session.
  size_t number_of_mouseovers_;

  // Total number of attempts made to load thumbnail images for this NTP
  // session.
  size_t number_of_thumbnail_attempts_;

  // Total number of errors that occurred when trying to load thumbnail images
  // for this NTP session. When these errors occur a grey tile is shown instead
  // of a thumbnail image.
  size_t number_of_thumbnail_errors_;

  // Total number of attempts made to load thumbnail images while providing a
  // fallback thumbnail for this NTP session.
  size_t number_of_fallback_thumbnails_requested_;

  // Total number of errors that occurred while trying to load the primary
  // thumbnail image and that caused a fallback to the secondary thumbnail.
  size_t number_of_fallback_thumbnails_used_;

  // The URL of this New Tab Page - varies based on NTP version.
  GURL ntp_url_;

  DISALLOW_COPY_AND_ASSIGN(NTPUserDataLogger);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NTP_USER_DATA_LOGGER_H_
