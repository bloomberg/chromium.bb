// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_PAGE_H_
#define CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_PAGE_H_

#include "base/strings/string16.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace content {
class InterstitialPage;
class WebContents;
}

namespace interstitials {
// Constants used to communicate with the JavaScript.
extern const char kBoxChecked[];
extern const char kDisplayCheckBox[];
extern const char kOptInLink[];
extern const char kPrivacyLinkHtml[];
}

namespace security_interstitials {
class MetricsHelper;
}

class SecurityInterstitialPage : public content::InterstitialPageDelegate {
 public:
  // These represent the commands sent from the interstitial JavaScript.
  // DO NOT reorder or change these without also changing the JavaScript!
  // See chrome/browser/resources/security_warnings/interstitial_v2.js
  enum SecurityInterstitialCommands {
    // Used by tests
    CMD_ERROR = -3,
    CMD_TEXT_FOUND = -2,
    CMD_TEXT_NOT_FOUND = -1,
    // Decisions
    CMD_DONT_PROCEED = 0,
    CMD_PROCEED = 1,
    // Ways for user to get more information
    CMD_SHOW_MORE_SECTION = 2,
    CMD_OPEN_HELP_CENTER = 3,
    CMD_OPEN_DIAGNOSTIC = 4,
    // Primary button actions
    CMD_RELOAD = 5,
    CMD_OPEN_DATE_SETTINGS = 6,
    CMD_OPEN_LOGIN = 7,
    // Safe Browsing Extended Reporting
    CMD_DO_REPORT = 8,
    CMD_DONT_REPORT = 9,
    CMD_OPEN_REPORTING_PRIVACY = 10,
    // Report a phishing error
    CMD_REPORT_PHISHING_ERROR = 11,
  };

  SecurityInterstitialPage(content::WebContents* web_contents,
                           const GURL& url);
  ~SecurityInterstitialPage() override;

  // Creates an interstitial and shows it.
  virtual void Show();

  // Prevents creating the actual interstitial view for testing.
  void DontCreateViewForTesting();

 protected:
  // Returns true if the interstitial should create a new navigation entry.
  virtual bool ShouldCreateNewNavigation() const = 0;

  // Populates the strings used to generate the HTML from the template.
  virtual void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) = 0;

  // InterstitialPageDelegate method:
  std::string GetHTMLContents() override;

  // Returns the formatted host name for the request url.
  base::string16 GetFormattedHostName() const;

  content::InterstitialPage* interstitial_page() const;
  content::WebContents* web_contents() const;
  GURL request_url() const;

  // Record the user's preference for reporting information about
  // malware and SSL errors.
  void SetReportingPreference(bool report);

  // Returns the boolean value of the given |pref| from the PrefService of the
  // Profile associated with |web_contents_|.
  bool IsPrefEnabled(const char* pref);

  void OpenExtendedReportingPrivacyPolicy();

  security_interstitials::MetricsHelper* metrics_helper() const;
  void set_metrics_helper(
      scoped_ptr<security_interstitials::MetricsHelper> metrics_helper);

 private:
  scoped_ptr<security_interstitials::MetricsHelper> metrics_helper_;
  // The WebContents with which this interstitial page is
  // associated. Not available in ~SecurityInterstitialPage, since it
  // can be destroyed before this class is destroyed.
  content::WebContents* web_contents_;
  const GURL request_url_;
  // Once shown, |interstitial_page| takes ownership of this
  // SecurityInterstitialPage instance.
  content::InterstitialPage* interstitial_page_;
  // Whether the interstitial should create a view.
  bool create_view_;

  DISALLOW_COPY_AND_ASSIGN(SecurityInterstitialPage);
};

#endif  // CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_PAGE_H_
