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

namespace security_interstitials {
class MetricsHelper;
}

class ChromeControllerClient;

class SecurityInterstitialPage : public content::InterstitialPageDelegate {
 public:
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

  // Returns the boolean value of the given |pref| from the PrefService of the
  // Profile associated with |web_contents_|.
  bool IsPrefEnabled(const char* pref);

  // TODO(felt): Remove these. They are temporary methods, used to pass along
  // calls to the |controller_| for subclasses that don't yet have their own
  // ChromeControllerClients. crbug.com/488673
  void SetReportingPreference(bool report);
  void OpenExtendedReportingPrivacyPolicy();
  security_interstitials::MetricsHelper* metrics_helper();
  void set_metrics_helper(
      scoped_ptr<security_interstitials::MetricsHelper> metrics_helper);

 private:
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
  // For subclasses that don't have their own ChromeControllerClients yet.
  scoped_ptr<ChromeControllerClient> controller_;

  DISALLOW_COPY_AND_ASSIGN(SecurityInterstitialPage);
};

#endif  // CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_PAGE_H_
