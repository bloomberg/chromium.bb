// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NTP_USER_DATA_LOGGER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NTP_USER_DATA_LOGGER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"


// Helper class for logging data from the NTP. Attached to each NTP instance.
class NTPUserDataLogger
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NTPUserDataLogger> {
 public:
  virtual ~NTPUserDataLogger();

  // Called each time the mouse hovers over a most visited tile or title.
  void increment_number_of_mouseovers();

  // To be set after initialization of this class. Used to determine whether a
  // tab visibility change event or navigation event comes from the NTP.
  void set_ntp_url(const GURL& url) {
    ntp_url_ = url;
  }

  const GURL& ntp_url() const { return ntp_url_; }

  // Logs total number of mouseovers per NTP session to UMA histogram. Called
  // when an NTP tab is about to be deactivated (be it by switching tabs, losing
  // focus or closing the tab/shutting down Chrome), or when the user navigates
  // to a URL.
  void EmitMouseoverCount();

  // content::WebContentsObserver override
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;

 private:
  explicit NTPUserDataLogger(content::WebContents* contents);
  friend class content::WebContentsUserData<NTPUserDataLogger>;

  // Total number of mouseovers for this NTP session.
  int number_of_mouseovers_;

  // The URL of this New Tab Page - varies based on NTP version.
  GURL ntp_url_;

  DISALLOW_COPY_AND_ASSIGN(NTPUserDataLogger);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NTP_USER_DATA_LOGGER_H_
