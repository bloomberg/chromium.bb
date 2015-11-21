// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_PAGE_H_
#define CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_PAGE_H_

#include "base/strings/string16.h"
#include "components/security_interstitials/core/controller_client.h"
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

class SecurityInterstitialPage
    : public content::InterstitialPageDelegate,
      public security_interstitials::ControllerClient {
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

 protected:
  // security_interstitials::ControllerClient overrides
  void OpenUrlInCurrentTab(const GURL& url) override;
  const std::string& GetApplicationLocale() override;
  PrefService* GetPrefService() override;
  const std::string GetExtendedReportingPrefName() override;

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

  DISALLOW_COPY_AND_ASSIGN(SecurityInterstitialPage);
};

#endif  // CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_PAGE_H_
